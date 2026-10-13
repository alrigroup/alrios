/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif

#include "alrios/hooks/lifecycle.h"
#include "alrios/atomic_swap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <time.h>

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL [%s:%d]: %s (errno=%d)\n", __FILE__, __LINE__, #cond, errno); \
        exit(1); \
    } \
} while (0)

static void write_file(const char *path, const char *content, mode_t mode) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, mode);
    CHECK(fd >= 0);
    size_t len = strlen(content);
    ssize_t written = write(fd, content, len);
    CHECK(written == (ssize_t)len);
    CHECK(close(fd) == 0);
    CHECK(chmod(path, mode) == 0);
}

static void remove_dir_recursive(const char *dir_path) {
    DIR *d = opendir(dir_path);
    if (!d) {
        (void)unlink(dir_path);
        return;
    }
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }
        char child[1024];
        (void)snprintf(child, sizeof(child), "%s/%s", dir_path, de->d_name);
        struct stat st;
        if (lstat(child, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                remove_dir_recursive(child);
            } else {
                (void)unlink(child);
            }
        }
    }
    (void)closedir(d);
    (void)rmdir(dir_path);
}

static void test_deterministic_execution_order_all_six_phases(const char *base_dir) {
    printf("[1/4] Testing deterministic execution order across all 6 lifecycle phases...\n");

    alrios_hook_phase_t phases[] = {
        ALRIOS_HOOK_PRE_BUILD,
        ALRIOS_HOOK_POST_BUILD,
        ALRIOS_HOOK_PRE_SWAP,
        ALRIOS_HOOK_POST_SWAP,
        ALRIOS_HOOK_PRE_DRAIN,
        ALRIOS_HOOK_POST_DRAIN
    };

    char log_path[1024];
    (void)snprintf(log_path, sizeof(log_path), "%s/exec_order.log", base_dir);
    (void)unlink(log_path);

    char slot_path[1024];
    (void)snprintf(slot_path, sizeof(slot_path), "%s/slot_order", base_dir);
    CHECK(mkdir(slot_path, 0755) == 0);

    for (size_t p = 0; p < sizeof(phases) / sizeof(phases[0]); p++) {
        const char *pname = alrios_hook_phase_name(phases[p]);
        char hook_dir[1024];
        (void)snprintf(hook_dir, sizeof(hook_dir), "%s/hooks.d/%s", slot_path, pname);

        char mkdir_cmd[1024];
        (void)snprintf(mkdir_cmd, sizeof(mkdir_cmd), "%s/hooks.d", slot_path);
        (void)mkdir(mkdir_cmd, 0755);
        CHECK(mkdir(hook_dir, 0755) == 0);

        /* Create 3 hooks in non-sorted order: 03-c, 01-a, 02-b */
        char h1[1024], h2[1024], h3[1024];
        (void)snprintf(h1, sizeof(h1), "%s/01-a.sh", hook_dir);
        (void)snprintf(h2, sizeof(h2), "%s/02-b.sh", hook_dir);
        (void)snprintf(h3, sizeof(h3), "%s/03-c.sh", hook_dir);

        char script3[1024], script1[1024], script2[1024];
        (void)snprintf(script1, sizeof(script1), "#!/bin/sh\necho \"$ALRIOS_HOOK_PHASE:01-a\" >> \"%s\"\nexit 0\n", log_path);
        (void)snprintf(script2, sizeof(script2), "#!/bin/sh\necho \"$ALRIOS_HOOK_PHASE:02-b\" >> \"%s\"\nexit 0\n", log_path);
        (void)snprintf(script3, sizeof(script3), "#!/bin/sh\necho \"$ALRIOS_HOOK_PHASE:03-c\" >> \"%s\"\nexit 0\n", log_path);

        write_file(h3, script3, 0755);
        write_file(h1, script1, 0755);
        write_file(h2, script2, 0755);

        alrios_hook_context_t ctx = {
            .phase = phases[p],
            .app_id = "com.alrigroup.test",
            .slot_path = slot_path,
            .timeout_ms = 5000,
            .drop_privileges = true
        };
        alrios_hook_result_t res;
        int rc = alrios_hook_run_phase(&ctx, &res);
        CHECK(rc == ALRIOS_HOOK_OK);
    }

    /* Verify log file contains exact lexicographical sequence */
    FILE *f = fopen(log_path, "r");
    CHECK(f != NULL);

    char line[256];
    const char *expected[] = {
        "pre-build:01-a\n", "pre-build:02-b\n", "pre-build:03-c\n",
        "post-build:01-a\n", "post-build:02-b\n", "post-build:03-c\n",
        "pre-swap:01-a\n", "pre-swap:02-b\n", "pre-swap:03-c\n",
        "post-swap:01-a\n", "post-swap:02-b\n", "post-swap:03-c\n",
        "pre-drain:01-a\n", "pre-drain:02-b\n", "pre-drain:03-c\n",
        "post-drain:01-a\n", "post-drain:02-b\n", "post-drain:03-c\n"
    };

    size_t line_count = 0;
    while (fgets(line, sizeof(line), f)) {
        CHECK(line_count < sizeof(expected) / sizeof(expected[0]));
        CHECK(strcmp(line, expected[line_count]) == 0);
        line_count++;
    }
    CHECK(line_count == sizeof(expected) / sizeof(expected[0]));
    CHECK(fclose(f) == 0);

    /* Test registered list sequential declaration order */
    char list_log[1024];
    (void)snprintf(list_log, sizeof(list_log), "%s/list_order.log", base_dir);
    char hook_w[1024], hook_x[1024], hook_y[1024];
    (void)snprintf(hook_w, sizeof(hook_w), "%s/w.sh", slot_path);
    (void)snprintf(hook_x, sizeof(hook_x), "%s/x.sh", slot_path);
    (void)snprintf(hook_y, sizeof(hook_y), "%s/y.sh", slot_path);

    char script_w[1024], script_x[1024], script_y[1024];
    (void)snprintf(script_w, sizeof(script_w), "#!/bin/sh\necho \"STEP:W\" >> \"%s\"\nexit 0\n", list_log);
    (void)snprintf(script_x, sizeof(script_x), "#!/bin/sh\necho \"STEP:X\" >> \"%s\"\nexit 0\n", list_log);
    (void)snprintf(script_y, sizeof(script_y), "#!/bin/sh\necho \"STEP:Y\" >> \"%s\"\nexit 0\n", list_log);

    write_file(hook_w, script_w, 0755);
    write_file(hook_x, script_x, 0755);
    write_file(hook_y, script_y, 0755);

    /* Register in reverse alphabetical order: Y, X, W */
    alrios_hook_entry_t entries[3];
    memset(entries, 0, sizeof(entries));
    (void)snprintf(entries[0].name, sizeof(entries[0].name), "hook-y");
    (void)snprintf(entries[0].path, sizeof(entries[0].path), "%s", hook_y);
    (void)snprintf(entries[1].name, sizeof(entries[1].name), "hook-x");
    (void)snprintf(entries[1].path, sizeof(entries[1].path), "%s", hook_x);
    (void)snprintf(entries[2].name, sizeof(entries[2].name), "hook-w");
    (void)snprintf(entries[2].path, sizeof(entries[2].path), "%s", hook_w);

    alrios_hook_context_t list_ctx = {
        .phase = ALRIOS_HOOK_PRE_BUILD,
        .app_id = "com.alrigroup.test",
        .timeout_ms = 5000,
        .drop_privileges = true
    };
    alrios_hook_result_t list_res;
    int list_rc = alrios_hook_run_list(&list_ctx, entries, 3, &list_res);
    CHECK(list_rc == ALRIOS_HOOK_OK);

    FILE *lf = fopen(list_log, "r");
    CHECK(lf != NULL);
    CHECK(fgets(line, sizeof(line), lf) != NULL && strcmp(line, "STEP:Y\n") == 0);
    CHECK(fgets(line, sizeof(line), lf) != NULL && strcmp(line, "STEP:X\n") == 0);
    CHECK(fgets(line, sizeof(line), lf) != NULL && strcmp(line, "STEP:W\n") == 0);
    CHECK(fgets(line, sizeof(line), lf) == NULL);
    CHECK(fclose(lf) == 0);

    printf("  [PASS] Deterministic order across all phases and registered chains verified.\n");
}

static void test_timeout_enforcement_sigterm_sigkill(const char *base_dir) {
    printf("[2/4] Testing timeout enforcement (SIGTERM/SIGKILL after deadline)...\n");

    char hang_script[1024];
    (void)snprintf(hang_script, sizeof(hang_script), "%s/hang.sh", base_dir);

    /* Hook that traps SIGTERM to verify SIGKILL escalation */
    const char *content =
        "#!/bin/sh\n"
        "trap '' TERM\n"
        "while true; do\n"
        "    sleep 1\n"
        "done\n";
    write_file(hang_script, content, 0755);

    alrios_hook_context_t ctx = {
        .phase = ALRIOS_HOOK_PRE_BUILD,
        .app_id = "com.alrigroup.test",
        .timeout_ms = 100, /* 100ms timeout */
        .drop_privileges = true
    };

    struct timespec t0, t1;
    CHECK(clock_gettime(CLOCK_MONOTONIC, &t0) == 0);

    alrios_hook_result_t res;
    int rc = alrios_hook_run_single(&ctx, hang_script, 100, &res);

    CHECK(clock_gettime(CLOCK_MONOTONIC, &t1) == 0);
    int64_t sec_diff = (int64_t)t1.tv_sec - (int64_t)t0.tv_sec;
    int64_t nsec_diff = (int64_t)t1.tv_nsec - (int64_t)t0.tv_nsec;
    if (nsec_diff < 0) {
        sec_diff -= 1;
        nsec_diff += 1000000000L;
    }
    uint64_t elapsed_ms = (uint64_t)sec_diff * 1000ULL + (uint64_t)nsec_diff / 1000000ULL;

    CHECK(rc == ALRIOS_HOOK_ERR_TIMEOUT);
    CHECK(res.timed_out == true);
    CHECK(res.error_code == ALRIOS_HOOK_ERR_TIMEOUT);

    /* Verify execution was terminated promptly (bounded under 1000ms) */
    CHECK(elapsed_ms < 1000ULL);

    printf("  [PASS] Hanging hook successfully killed and terminated in %llu ms.\n",
           (unsigned long long)elapsed_ms);
}

static void test_preswap_failure_aborts_swap_with_rollback(const char *base_dir) {
    printf("[3/4] Testing pre-swap failure aborts atomic swap with rollback...\n");

    char slot_alpha[1024];
    char slot_beta[1024];
    char symlink_current[1024];

    (void)snprintf(slot_alpha, sizeof(slot_alpha), "%s/slot_alpha", base_dir);
    (void)snprintf(slot_beta, sizeof(slot_beta), "%s/slot_beta", base_dir);
    (void)snprintf(symlink_current, sizeof(symlink_current), "%s/current", base_dir);

    CHECK(mkdir(slot_alpha, 0755) == 0);
    CHECK(mkdir(slot_beta, 0755) == 0);

    char ver_alpha[1024], ver_beta[1024];
    (void)snprintf(ver_alpha, sizeof(ver_alpha), "%s/version.txt", slot_alpha);
    (void)snprintf(ver_beta, sizeof(ver_beta), "%s/version.txt", slot_beta);

    write_file(ver_alpha, "VERSION_ALPHA\n", 0644);
    write_file(ver_beta, "VERSION_BETA\n", 0644);

    /* Initial state: symlink points to slot_alpha */
    (void)unlink(symlink_current);
    CHECK(symlink(slot_alpha, symlink_current) == 0);

    /* Create candidate slot_beta with FAILING pre-swap hook */
    char beta_hooks[1024];
    (void)snprintf(beta_hooks, sizeof(beta_hooks), "%s/hooks.d/pre-swap", slot_beta);
    char beta_hooks_parent[1024];
    (void)snprintf(beta_hooks_parent, sizeof(beta_hooks_parent), "%s/hooks.d", slot_beta);
    (void)mkdir(beta_hooks_parent, 0755);
    CHECK(mkdir(beta_hooks, 0755) == 0);

    char fail_hook[1024];
    (void)snprintf(fail_hook, sizeof(fail_hook), "%s/01-failing-check.sh", beta_hooks);
    write_file(fail_hook, "#!/bin/sh\nexit 42\n", 0755);

    alrios_hook_context_t ctx = {
        .phase = ALRIOS_HOOK_PRE_SWAP,
        .app_id = "com.alrigroup.test",
        .slot_path = slot_beta,
        .old_slot_path = slot_alpha,
        .new_slot_path = slot_beta,
        .timeout_ms = 3000,
        .drop_privileges = true
    };
    alrios_hook_result_t res;

    int swap_rc = alrios_lifecycle_atomic_swap(&ctx, slot_beta, symlink_current, 0, &res);

    /* Pre-swap failure must return immediately with ABORT */
    CHECK(swap_rc == ALRIOS_HOOK_ERR_ABORT);
    CHECK(res.aborted == true);

    /* Verify active slot was untouched: current symlink STILL points to slot_alpha */
    char target[1024] = {0};
    ssize_t rlen = readlink(symlink_current, target, sizeof(target) - 1);
    CHECK(rlen > 0);
    target[rlen] = '\0';
    CHECK(strcmp(target, slot_alpha) == 0);

    char active_ver[1024];
    (void)snprintf(active_ver, sizeof(active_ver), "%s/version.txt", symlink_current);
    FILE *vf = fopen(active_ver, "r");
    CHECK(vf != NULL);
    char buf[64];
    CHECK(fgets(buf, sizeof(buf), vf) != NULL);
    CHECK(strcmp(buf, "VERSION_ALPHA\n") == 0);
    CHECK(fclose(vf) == 0);

    /* Now update the hook in slot_beta to SUCCEED */
    write_file(fail_hook, "#!/bin/sh\nexit 0\n", 0755);

    /* Also add post-swap hook in slot_beta */
    char beta_post_hooks[1024];
    (void)snprintf(beta_post_hooks, sizeof(beta_post_hooks), "%s/hooks.d/post-swap", slot_beta);
    CHECK(mkdir(beta_post_hooks, 0755) == 0);

    char post_log[1024];
    (void)snprintf(post_log, sizeof(post_log), "%s/post_swap.log", base_dir);
    char post_hook[1024];
    (void)snprintf(post_hook, sizeof(post_hook), "%s/01-notify.sh", beta_post_hooks);
    char post_script[1024];
    (void)snprintf(post_script, sizeof(post_script), "#!/bin/sh\necho \"POST_SWAP_OK\" > \"%s\"\nexit 0\n", post_log);
    write_file(post_hook, post_script, 0755);

    swap_rc = alrios_lifecycle_atomic_swap(&ctx, slot_beta, symlink_current, 0, &res);
    CHECK(swap_rc == ALRIOS_HOOK_OK);

    /* Verify symlink successfully points to slot_beta */
    memset(target, 0, sizeof(target));
    rlen = readlink(symlink_current, target, sizeof(target) - 1);
    CHECK(rlen > 0);
    target[rlen] = '\0';
    CHECK(strcmp(target, slot_beta) == 0);

    vf = fopen(active_ver, "r");
    CHECK(vf != NULL);
    CHECK(fgets(buf, sizeof(buf), vf) != NULL);
    CHECK(strcmp(buf, "VERSION_BETA\n") == 0);
    CHECK(fclose(vf) == 0);

    /* Verify post-swap hook executed */
    FILE *pf = fopen(post_log, "r");
    CHECK(pf != NULL);
    CHECK(fgets(buf, sizeof(buf), pf) != NULL);
    CHECK(strcmp(buf, "POST_SWAP_OK\n") == 0);
    CHECK(fclose(pf) == 0);

    printf("  [PASS] Pre-swap failure aborted swap atomically; succeeding hook promoted slot.\n");
}

static void test_environment_isolation_and_sandboxed_privileges(const char *base_dir) {
    printf("[4/4] Testing environment isolation and sandboxed privileges...\n");

    /* Set leak canary in parent environment */
    (void)setenv("LEAK_CANARY_SECRET", "CLASSIFIED_ALRIOS_KEY_XYZ", 1);

    char env_script[1024];
    (void)snprintf(env_script, sizeof(env_script), "%s/env_test.sh", base_dir);

    const char *content =
        "#!/bin/sh\n"
        "if [ -n \"$LEAK_CANARY_SECRET\" ]; then\n"
        "    echo \"LEAK DETECTED: $LEAK_CANARY_SECRET\"\n"
        "    exit 1\n"
        "fi\n"
        "if [ \"$ALRIOS_HOOK_PHASE\" != \"pre-swap\" ]; then\n"
        "    echo \"INVALID STAGE: $ALRIOS_HOOK_PHASE\"\n"
        "    exit 2\n"
        "fi\n"
        "if [ \"$ALRIOS_APP_ID\" != \"com.alrigroup.secure\" ]; then\n"
        "    echo \"INVALID APP ID\"\n"
        "    exit 3\n"
        "fi\n"
        "if [ \"$CUSTOM_VAR\" != \"allowed_value\" ]; then\n"
        "    echo \"MISSING CUSTOM VAR\"\n"
        "    exit 4\n"
        "fi\n"
        "exit 0\n";
    write_file(env_script, content, 0755);

    const char *custom_env[] = {
        "CUSTOM_VAR=allowed_value"
    };

    alrios_hook_context_t ctx = {
        .phase = ALRIOS_HOOK_PRE_SWAP,
        .app_id = "com.alrigroup.secure",
        .slot_path = base_dir,
        .timeout_ms = 3000,
        .custom_env = custom_env,
        .custom_env_count = 1,
        .drop_privileges = true
    };
    alrios_hook_result_t res;

    int rc = alrios_hook_run_single(&ctx, env_script, 3000, &res);
    CHECK(rc == ALRIOS_HOOK_OK);
    CHECK(res.exit_code == 0);

    printf("  [PASS] Host environment clean; context and custom variables isolated.\n");
}

int main(void) {
    printf("=== ALRIOS Lifecycle Hooks Integration Test Suite ===\n");

    char tmpl[] = "/tmp/alrios-test-hooks-XXXXXX";
    char *base_dir = mkdtemp(tmpl);
    CHECK(base_dir != NULL);

    test_deterministic_execution_order_all_six_phases(base_dir);
    test_timeout_enforcement_sigterm_sigkill(base_dir);
    test_preswap_failure_aborts_swap_with_rollback(base_dir);
    test_environment_isolation_and_sandboxed_privileges(base_dir);

    remove_dir_recursive(base_dir);

    printf("=== ALL LIFECYCLE HOOKS TESTS PASSED ===\n");
    return 0;
}
