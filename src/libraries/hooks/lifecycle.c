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

#include "alrios/hooks/lifecycle.h"
#include "alrios/atomic_swap.h"
#include "aros_hal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static uint64_t monotonic_time_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
    }
    return 0;
}

const char *alrios_hook_phase_name(alrios_hook_phase_t phase) {
    switch (phase) {
        case ALRIOS_HOOK_PRE_BUILD:  return "pre-build";
        case ALRIOS_HOOK_POST_BUILD: return "post-build";
        case ALRIOS_HOOK_PRE_SWAP:   return "pre-swap";
        case ALRIOS_HOOK_POST_SWAP:  return "post-swap";
        case ALRIOS_HOOK_PRE_DRAIN:  return "pre-drain";
        case ALRIOS_HOOK_POST_DRAIN: return "post-drain";
        default:                     return "unknown";
    }
}

int alrios_hook_phase_from_name(const char *name, alrios_hook_phase_t *out_phase) {
    if (!name || !out_phase) {
        return ALRIOS_HOOK_ERR_INVALID;
    }
    if (strcmp(name, "pre-build") == 0) {
        *out_phase = ALRIOS_HOOK_PRE_BUILD;
        return ALRIOS_HOOK_OK;
    }
    if (strcmp(name, "post-build") == 0) {
        *out_phase = ALRIOS_HOOK_POST_BUILD;
        return ALRIOS_HOOK_OK;
    }
    if (strcmp(name, "pre-swap") == 0) {
        *out_phase = ALRIOS_HOOK_PRE_SWAP;
        return ALRIOS_HOOK_OK;
    }
    if (strcmp(name, "post-swap") == 0) {
        *out_phase = ALRIOS_HOOK_POST_SWAP;
        return ALRIOS_HOOK_OK;
    }
    if (strcmp(name, "pre-drain") == 0) {
        *out_phase = ALRIOS_HOOK_PRE_DRAIN;
        return ALRIOS_HOOK_OK;
    }
    if (strcmp(name, "post-drain") == 0) {
        *out_phase = ALRIOS_HOOK_POST_DRAIN;
        return ALRIOS_HOOK_OK;
    }
    return ALRIOS_HOOK_ERR_INVALID;
}

bool alrios_hook_phase_aborts_on_fail(alrios_hook_phase_t phase) {
    switch (phase) {
        case ALRIOS_HOOK_PRE_BUILD:  return true;
        case ALRIOS_HOOK_POST_BUILD: return true;
        case ALRIOS_HOOK_PRE_SWAP:   return true;
        case ALRIOS_HOOK_POST_SWAP:  return false;
        case ALRIOS_HOOK_PRE_DRAIN:  return true;
        case ALRIOS_HOOK_POST_DRAIN: return false;
        default:                     return true;
    }
}

const char *alrios_hook_strerror(int code) {
    switch (code) {
        case ALRIOS_HOOK_OK:             return "Success";
        case ALRIOS_HOOK_ERR_INVALID:    return "Invalid parameter or stage";
        case ALRIOS_HOOK_ERR_TIMEOUT:    return "Hook execution timed out";
        case ALRIOS_HOOK_ERR_SIGNATURE:  return "Hook cryptographic signature verification failed";
        case ALRIOS_HOOK_ERR_EXEC:       return "Hook process execution failed";
        case ALRIOS_HOOK_ERR_ABORT:      return "Hook execution aborted by lifecycle policy";
        case ALRIOS_HOOK_ERR_NOMEM:      return "Memory allocation failed";
        case ALRIOS_HOOK_ERR_PRIVILEGE:  return "Privilege drop or sandbox restriction failed";
        case ALRIOS_HOOK_ERR_SWAP:       return "Atomic symlink swap failed";
        case ALRIOS_HOOK_ERR_ROLLBACK:   return "Swap rollback failed";
        default:                         return "Unknown hook error";
    }
}

static void free_namelist(struct dirent **namelist, int count) {
    if (!namelist) {
        return;
    }
    for (int i = 0; i < count; i++) {
        if (namelist[i]) {
            free(namelist[i]);
            namelist[i] = NULL;
        }
    }
    free(namelist);
}

int alrios_hook_run_single(const alrios_hook_context_t *ctx,
                           const char *hook_path,
                           uint32_t timeout_ms,
                           alrios_hook_result_t *out_result) {
    if (!ctx || !hook_path || hook_path[0] == '\0') {
        if (out_result) {
            memset(out_result, 0, sizeof(*out_result));
            out_result->error_code = ALRIOS_HOOK_ERR_INVALID;
        }
        return ALRIOS_HOOK_ERR_INVALID;
    }

    uint32_t effective_timeout = (timeout_ms > 0) ? timeout_ms :
        ((ctx->timeout_ms > 0) ? ctx->timeout_ms : ALRIOS_HOOK_DEFAULT_TIMEOUT_MS);

    uint64_t start_ms = monotonic_time_ms();
    uint64_t deadline_ms = start_ms + (uint64_t)effective_timeout;

    char env_phase[128];
    char env_stage[128];
    char env_app[512];
    char env_slot[ALRIOS_HOOK_MAX_PATH_LEN];
    char env_old_slot[ALRIOS_HOOK_MAX_PATH_LEN];
    char env_new_slot[ALRIOS_HOOK_MAX_PATH_LEN];

    const char *phase_str = alrios_hook_phase_name(ctx->phase);
    (void)snprintf(env_phase, sizeof(env_phase), "ALRIOS_HOOK_PHASE=%s", phase_str);
    (void)snprintf(env_stage, sizeof(env_stage), "ALRIOS_HOOK_STAGE=%s", phase_str);
    (void)snprintf(env_app, sizeof(env_app), "ALRIOS_APP_ID=%s", ctx->app_id ? ctx->app_id : "");
    (void)snprintf(env_slot, sizeof(env_slot), "ALRIOS_SLOT_PATH=%s", ctx->slot_path ? ctx->slot_path : "");
    (void)snprintf(env_old_slot, sizeof(env_old_slot), "ALRIOS_OLD_SLOT_PATH=%s",
                   ctx->old_slot_path ? ctx->old_slot_path : "");
    (void)snprintf(env_new_slot, sizeof(env_new_slot), "ALRIOS_NEW_SLOT_PATH=%s",
                   ctx->new_slot_path ? ctx->new_slot_path : "");

    char *child_env[ALRIOS_HOOK_MAX_ENV_COUNT + 16];
    size_t env_idx = 0;
    child_env[env_idx++] = env_phase;
    child_env[env_idx++] = env_stage;
    child_env[env_idx++] = env_app;
    child_env[env_idx++] = env_slot;
    child_env[env_idx++] = env_old_slot;
    child_env[env_idx++] = env_new_slot;
    child_env[env_idx++] = "PATH=/usr/bin:/bin:/usr/sbin:/sbin";

    if (ctx->custom_env) {
        for (size_t c = 0; c < ctx->custom_env_count && env_idx < (sizeof(child_env) / sizeof(child_env[0]) - 1); c++) {
            if (ctx->custom_env[c]) {
                child_env[env_idx++] = (char *)(uintptr_t)ctx->custom_env[c];
            }
        }
    }
    child_env[env_idx] = NULL;

    char *child_argv[] = {
        (char *)(uintptr_t)hook_path,
        NULL
    };

    const char *working_directory = ctx->working_directory && ctx->working_directory[0] != '\0'
        ? ctx->working_directory : ctx->slot_path;
    unsigned long target_uid = (unsigned long)ctx->run_as_uid;
    unsigned long target_gid = (unsigned long)ctx->run_as_gid;
    if (ctx->drop_privileges && geteuid() == 0) {
        if (target_uid == 0UL) target_uid = 65534UL;
        if (target_gid == 0UL) target_gid = 65534UL;
    }
    int pid = ar_process_spawn_as(hook_path, child_argv, child_env,
                                  working_directory,
                                  AR_PROCESS_FLAG_NEW_GROUP |
                                  AR_PROCESS_FLAG_NO_NEW_PRIVS,
                                  target_uid, target_gid);
    if (pid < 0) {
        if (out_result) {
            memset(out_result, 0, sizeof(*out_result));
            out_result->error_code = ALRIOS_HOOK_ERR_EXEC;
            (void)snprintf(out_result->failed_hook,
                           sizeof(out_result->failed_hook), "%s", hook_path);
        }
        return ALRIOS_HOOK_ERR_EXEC;
    }

    /* Parent monitor loop with monotonic deadline */
    int status = 0;
    int child_done = 0;
    int timed_out = 0;

    while (!child_done) {
        int wait_result = ar_process_wait_raw_nohang(pid, &status);
        if (wait_result > 0) {
            child_done = 1;
            break;
        }
        if (wait_result < 0) {
            break;
        }

        uint64_t now_ms = monotonic_time_ms();
        if (now_ms >= deadline_ms) {
            timed_out = 1;
            break;
        }

        struct timespec sleep_interval = { .tv_sec = 0, .tv_nsec = 5000000L }; /* 5ms */
        nanosleep(&sleep_interval, NULL);
    }

    if (timed_out) {
        /* Enforce deadline: escalate SIGTERM -> SIGKILL to process group */
        (void)ar_process_signal(pid, AR_PROCESS_SIGNAL_TERM, 1);

        uint64_t term_deadline = monotonic_time_ms() + 100ULL;
        while (monotonic_time_ms() < term_deadline) {
            int wait_result = ar_process_wait_raw_nohang(pid, &status);
            if (wait_result != 0) {
                child_done = 1;
                break;
            }
            struct timespec ts = { .tv_sec = 0, .tv_nsec = 5000000L };
            nanosleep(&ts, NULL);
        }

        if (!child_done) {
            (void)ar_process_signal(pid, AR_PROCESS_SIGNAL_KILL, 1);
            (void)ar_process_wait_raw(pid, &status);
        }

        if (out_result) {
            memset(out_result, 0, sizeof(*out_result));
            out_result->timed_out = true;
            out_result->error_code = ALRIOS_HOOK_ERR_TIMEOUT;
            out_result->elapsed_ms = monotonic_time_ms() - start_ms;
            (void)snprintf(out_result->failed_hook, sizeof(out_result->failed_hook), "%s", hook_path);
        }
        return ALRIOS_HOOK_ERR_TIMEOUT;
    }

    uint64_t elapsed = monotonic_time_ms() - start_ms;
    if (out_result) {
        memset(out_result, 0, sizeof(*out_result));
        out_result->elapsed_ms = elapsed;
    }

    if (child_done) {
        int exit_code = 0;
        int failed = 0;
        if (WIFEXITED(status)) {
            exit_code = WEXITSTATUS(status);
            if (exit_code != 0) {
                failed = 1;
            }
        } else if (WIFSIGNALED(status)) {
            exit_code = 128 + WTERMSIG(status);
            failed = 1;
        }

        if (out_result) {
            out_result->exit_code = exit_code;
        }

        if (failed) {
            if (out_result) {
                out_result->error_code = ALRIOS_HOOK_ERR_EXEC;
                (void)snprintf(out_result->failed_hook, sizeof(out_result->failed_hook), "%s", hook_path);
            }
            return ALRIOS_HOOK_ERR_EXEC;
        }
    }

    if (out_result) {
        out_result->error_code = ALRIOS_HOOK_OK;
    }
    return ALRIOS_HOOK_OK;
}

int alrios_hook_run_list(const alrios_hook_context_t *ctx,
                         const alrios_hook_entry_t *hooks,
                         size_t hook_count,
                         alrios_hook_result_t *out_result) {
    if (!ctx || (!hooks && hook_count > 0)) {
        if (out_result) {
            memset(out_result, 0, sizeof(*out_result));
            out_result->error_code = ALRIOS_HOOK_ERR_INVALID;
        }
        return ALRIOS_HOOK_ERR_INVALID;
    }

    if (out_result) {
        memset(out_result, 0, sizeof(*out_result));
    }

    for (size_t i = 0; i < hook_count; i++) {
        const alrios_hook_entry_t *entry = &hooks[i];
        if (entry->path[0] == '\0') {
            continue;
        }

        uint32_t t = (entry->timeout_ms > 0) ? entry->timeout_ms : ctx->timeout_ms;
        alrios_hook_result_t single_res;
        memset(&single_res, 0, sizeof(single_res));

        int rc = alrios_hook_run_single(ctx, entry->path, t, &single_res);
        if (out_result) {
            out_result->elapsed_ms += single_res.elapsed_ms;
        }

        if (rc != ALRIOS_HOOK_OK) {
            if (out_result) {
                out_result->exit_code = single_res.exit_code;
                out_result->error_code = single_res.error_code;
                out_result->timed_out = single_res.timed_out;
                (void)snprintf(out_result->failed_hook, sizeof(out_result->failed_hook),
                               "%s", entry->name[0] ? entry->name : entry->path);
            }

            if (!entry->optional) {
                if (out_result) {
                    out_result->aborted = true;
                }
                return rc;
            }
        }
    }

    if (out_result) {
        out_result->error_code = ALRIOS_HOOK_OK;
    }
    return ALRIOS_HOOK_OK;
}

int alrios_hook_run_directory(const alrios_hook_context_t *ctx,
                              const char *dir_path,
                              alrios_hook_result_t *out_result) {
    if (!ctx || !dir_path || dir_path[0] == '\0') {
        if (out_result) {
            memset(out_result, 0, sizeof(*out_result));
            out_result->error_code = ALRIOS_HOOK_ERR_INVALID;
        }
        return ALRIOS_HOOK_ERR_INVALID;
    }

    struct stat st;
    if (stat(dir_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        if (out_result) {
            memset(out_result, 0, sizeof(*out_result));
            out_result->error_code = ALRIOS_HOOK_OK;
        }
        return ALRIOS_HOOK_OK;
    }

    struct dirent **namelist = NULL;
    int count = scandir(dir_path, &namelist, NULL, alphasort);
    if (count < 0) {
        if (out_result) {
            memset(out_result, 0, sizeof(*out_result));
            out_result->error_code = ALRIOS_HOOK_OK;
        }
        return ALRIOS_HOOK_OK;
    }

    int result = ALRIOS_HOOK_OK;
    bool abort_on_fail = alrios_hook_phase_aborts_on_fail(ctx->phase);

    for (int i = 0; i < count; i++) {
        if (!namelist[i]) {
            continue;
        }

        const char *name = namelist[i]->d_name;
        if (name[0] == '.') {
            continue;
        }

        size_t nlen = strlen(name);
        if (nlen > 0 && name[nlen - 1] == '~') {
            continue;
        }

        char hook_path[ALRIOS_HOOK_MAX_PATH_LEN];
        int w = snprintf(hook_path, sizeof(hook_path), "%s/%s", dir_path, name);
        if (w < 0 || (size_t)w >= sizeof(hook_path)) {
            continue;
        }

        struct stat fst;
        if (stat(hook_path, &fst) != 0 || !S_ISREG(fst.st_mode)) {
            continue;
        }

        if (access(hook_path, X_OK) != 0) {
            continue;
        }

        alrios_hook_result_t single_res;
        memset(&single_res, 0, sizeof(single_res));

        int rc = alrios_hook_run_single(ctx, hook_path, ctx->timeout_ms, &single_res);
        if (out_result) {
            out_result->elapsed_ms += single_res.elapsed_ms;
        }

        if (rc != ALRIOS_HOOK_OK) {
            if (out_result) {
                out_result->exit_code = single_res.exit_code;
                out_result->error_code = single_res.error_code;
                out_result->timed_out = single_res.timed_out;
                (void)snprintf(out_result->failed_hook, sizeof(out_result->failed_hook), "%s", hook_path);
            }

            if (abort_on_fail) {
                if (out_result) {
                    out_result->aborted = true;
                }
                result = rc;
                break;
            }
        }
    }

    free_namelist(namelist, count);
    if (out_result && result == ALRIOS_HOOK_OK) {
        out_result->error_code = ALRIOS_HOOK_OK;
    }
    return result;
}

int alrios_hook_run_phase(const alrios_hook_context_t *ctx,
                          alrios_hook_result_t *out_result) {
    if (!ctx) {
        if (out_result) {
            memset(out_result, 0, sizeof(*out_result));
            out_result->error_code = ALRIOS_HOOK_ERR_INVALID;
        }
        return ALRIOS_HOOK_ERR_INVALID;
    }

    const char *phase_str = alrios_hook_phase_name(ctx->phase);
    bool abort_on_fail = alrios_hook_phase_aborts_on_fail(ctx->phase);

    if (out_result) {
        memset(out_result, 0, sizeof(*out_result));
    }

    /* 1. Slot hooks: <slot_path>/hooks.d/<phase> */
    if (ctx->slot_path && ctx->slot_path[0] != '\0') {
        char slot_hooks_dir[ALRIOS_HOOK_MAX_PATH_LEN];
        int w = snprintf(slot_hooks_dir, sizeof(slot_hooks_dir), "%s/hooks.d/%s",
                         ctx->slot_path, phase_str);
        if (w > 0 && (size_t)w < sizeof(slot_hooks_dir)) {
            int ret = alrios_hook_run_directory(ctx, slot_hooks_dir, out_result);
            if (ret != ALRIOS_HOOK_OK && abort_on_fail) {
                return ret;
            }
        }
    }

    /* 2. System hooks: /etc/alrios/hooks.d/<phase> */
    char sys_hooks_dir[ALRIOS_HOOK_MAX_PATH_LEN];
    int sw = snprintf(sys_hooks_dir, sizeof(sys_hooks_dir), "/etc/alrios/hooks.d/%s", phase_str);
    if (sw > 0 && (size_t)sw < sizeof(sys_hooks_dir)) {
        int ret = alrios_hook_run_directory(ctx, sys_hooks_dir, out_result);
        if (ret != ALRIOS_HOOK_OK && abort_on_fail) {
            return ret;
        }
    }

    if (out_result) {
        out_result->error_code = ALRIOS_HOOK_OK;
    }
    return ALRIOS_HOOK_OK;
}

int alrios_lifecycle_atomic_swap(const alrios_hook_context_t *ctx,
                                 const char *target_dir,
                                 const char *symlink_current,
                                 pid_t master_pid,
                                 alrios_hook_result_t *out_result) {
    if (!target_dir || !symlink_current || target_dir[0] == '\0' || symlink_current[0] == '\0') {
        if (out_result) {
            memset(out_result, 0, sizeof(*out_result));
            out_result->error_code = ALRIOS_HOOK_ERR_INVALID;
        }
        return ALRIOS_HOOK_ERR_INVALID;
    }

    if (out_result) {
        memset(out_result, 0, sizeof(*out_result));
    }

    /* Preserve previous symlink target for rollback verification */
    char previous_target[PATH_MAX] = {0};
    ssize_t rlen = readlink(symlink_current, previous_target, sizeof(previous_target) - 1);
    if (rlen > 0) {
        previous_target[rlen] = '\0';
    }

    /* 1. Execute PRE_SWAP lifecycle hooks on the candidate release slot */
    alrios_hook_context_t pre_ctx;
    if (ctx) {
        pre_ctx = *ctx;
    } else {
        memset(&pre_ctx, 0, sizeof(pre_ctx));
    }
    pre_ctx.phase = ALRIOS_HOOK_PRE_SWAP;
    pre_ctx.new_slot_path = target_dir;
    if (previous_target[0] != '\0' && !pre_ctx.old_slot_path) {
        pre_ctx.old_slot_path = previous_target;
    }
    if (!pre_ctx.slot_path) {
        pre_ctx.slot_path = target_dir;
    }

    int pre_status = alrios_hook_run_phase(&pre_ctx, out_result);
    if (pre_status != ALRIOS_HOOK_OK) {
        /*
         * Pre-swap failure strictly aborts the swap atomically.
         * The active symlink remains completely untouched.
         */
        if (out_result) {
            out_result->aborted = true;
            out_result->error_code = pre_status;
        }
        return ALRIOS_HOOK_ERR_ABORT;
    }

    /* 2. Execute atomic symlink swap via renameat2 / rename */
    int swap_rc = alrios_atomic_symlink_swap(target_dir, symlink_current, master_pid);
    if (swap_rc != 0) {
        /* Rollback: if symlink was modified or broke, restore previous target */
        if (previous_target[0] != '\0') {
            (void)alrios_atomic_symlink_swap(previous_target, symlink_current, 0);
        }
        if (out_result) {
            out_result->aborted = true;
            out_result->error_code = ALRIOS_HOOK_ERR_SWAP;
        }
        return ALRIOS_HOOK_ERR_SWAP;
    }

    /* 3. Execute POST_SWAP lifecycle hooks asynchronously / notification */
    alrios_hook_context_t post_ctx = pre_ctx;
    post_ctx.phase = ALRIOS_HOOK_POST_SWAP;
    (void)alrios_hook_run_phase(&post_ctx, out_result);

    if (out_result) {
        out_result->error_code = ALRIOS_HOOK_OK;
    }
    return ALRIOS_HOOK_OK;
}
