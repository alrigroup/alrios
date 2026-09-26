/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "alrios/sandbox_defs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static void read_file_exact(const char *path, char *buf, size_t max_len) {
    int fd = open(path, O_RDONLY);
    assert(fd >= 0);
    ssize_t n = read(fd, buf, max_len - 1);
    assert(n > 0);
    buf[n] = '\0';
    close(fd);
}

static void test_cgroups_creation_and_limits(void) {
    const char *slot = "slot_alpha";
    pid_t pid = 1234;

    int ret = alrios_sandbox_setup_cgroups(slot, pid);
    (void)ret;
    assert(ret == ALRIOS_SANDBOX_OK);

    const char *root = alrios_sandbox_get_cgroup_root();
    assert(root != NULL);

    char path[PATH_MAX];
    char content[128];

    /* 1. Verify memory.max is 256MB */
    int n = snprintf(path, sizeof(path), "%s/%s/memory.max", root, slot);
    (void)n;
    assert(n > 0 && (size_t)n < sizeof(path));
    read_file_exact(path, content, sizeof(content));
    assert(strcmp(content, "268435456\n") == 0);

    /* 2. Verify memory.swap.max is 0 */
    n = snprintf(path, sizeof(path), "%s/%s/memory.swap.max", root, slot);
    assert(n > 0 && (size_t)n < sizeof(path));
    read_file_exact(path, content, sizeof(content));
    assert(strcmp(content, "0\n") == 0);

    /* 3. Verify pids.max is 32 */
    n = snprintf(path, sizeof(path), "%s/%s/pids.max", root, slot);
    assert(n > 0 && (size_t)n < sizeof(path));
    read_file_exact(path, content, sizeof(content));
    assert(strcmp(content, "32\n") == 0);

    /* 4. Verify cgroup.procs contains target PID */
    n = snprintf(path, sizeof(path), "%s/%s/cgroup.procs", root, slot);
    assert(n > 0 && (size_t)n < sizeof(path));
    read_file_exact(path, content, sizeof(content));
    assert(strcmp(content, "1234\n") == 0);

    /* 5. Clean up */
    assert(alrios_sandbox_cleanup_cgroups(slot) == ALRIOS_SANDBOX_OK);
}

static void test_custom_root_and_cleanup(void) {
    const char *custom_root = "/tmp/alrios_custom_cgroup_unit";
    assert(alrios_sandbox_set_cgroup_root(custom_root) == ALRIOS_SANDBOX_OK);
    assert(strcmp(alrios_sandbox_get_cgroup_root(), custom_root) == 0);

    const char *slot = "slot_custom_42";
    pid_t pid = 7890;
    (void)pid;
    assert(alrios_sandbox_setup_cgroups(slot, pid) == ALRIOS_SANDBOX_OK);

    char path[PATH_MAX];
    char content[128];
    int n = snprintf(path, sizeof(path), "%s/%s/pids.max", custom_root, slot);
    (void)n;
    assert(n > 0 && (size_t)n < sizeof(path));
    read_file_exact(path, content, sizeof(content));
    assert(strcmp(content, "32\n") == 0);

    assert(alrios_sandbox_cleanup_cgroups(slot) == ALRIOS_SANDBOX_OK);
    (void)rmdir(custom_root);

    /* Reset to default */
    assert(alrios_sandbox_set_cgroup_root(NULL) == ALRIOS_SANDBOX_OK);
}

static void test_security_sanitization_and_errors(void) {
    /* NULL and empty */
    assert(alrios_sandbox_setup_cgroups(NULL, 1234) == ALRIOS_SANDBOX_ERR_CGROUP_FAIL);
    assert(alrios_sandbox_setup_cgroups("", 1234) == ALRIOS_SANDBOX_ERR_CGROUP_FAIL);

    /* Invalid PID */
    assert(alrios_sandbox_setup_cgroups("slot_test", 0) == ALRIOS_SANDBOX_ERR_CGROUP_FAIL);
    assert(alrios_sandbox_setup_cgroups("slot_test", -1) == ALRIOS_SANDBOX_ERR_CGROUP_FAIL);
    assert(alrios_sandbox_setup_cgroups("slot_test", -9999) == ALRIOS_SANDBOX_ERR_CGROUP_FAIL);

    /* Path traversal attempts */
    assert(alrios_sandbox_setup_cgroups("../escape", 1234) == ALRIOS_SANDBOX_ERR_CGROUP_FAIL);
    assert(alrios_sandbox_setup_cgroups("slot/evil", 1234) == ALRIOS_SANDBOX_ERR_CGROUP_FAIL);
    assert(alrios_sandbox_setup_cgroups("/root/slot", 1234) == ALRIOS_SANDBOX_ERR_CGROUP_FAIL);

    /* Special characters and shell injection */
    assert(alrios_sandbox_setup_cgroups("slot;rm", 1234) == ALRIOS_SANDBOX_ERR_CGROUP_FAIL);
    assert(alrios_sandbox_setup_cgroups("slot name", 1234) == ALRIOS_SANDBOX_ERR_CGROUP_FAIL);
    assert(alrios_sandbox_setup_cgroups("slot$eval", 1234) == ALRIOS_SANDBOX_ERR_CGROUP_FAIL);
    assert(alrios_sandbox_setup_cgroups("slot`id`", 1234) == ALRIOS_SANDBOX_ERR_CGROUP_FAIL);

    /* Length overflow check (> 64 bytes) */
    char long_slot[128];
    memset(long_slot, 'a', sizeof(long_slot) - 1);
    long_slot[sizeof(long_slot) - 1] = '\0';
    assert(alrios_sandbox_setup_cgroups(long_slot, 1234) == ALRIOS_SANDBOX_ERR_CGROUP_FAIL);

    /* Invalid custom root (not starting with /) */
    assert(alrios_sandbox_set_cgroup_root("relative/path") == ALRIOS_SANDBOX_ERR_CGROUP_FAIL);

    /* Cleanup invalid slot_id */
    assert(alrios_sandbox_cleanup_cgroups(NULL) == ALRIOS_SANDBOX_ERR_CGROUP_FAIL);
    assert(alrios_sandbox_cleanup_cgroups("../evil") == ALRIOS_SANDBOX_ERR_CGROUP_FAIL);
}

int main(void) {
    test_cgroups_creation_and_limits();
    test_custom_root_and_cleanup();
    test_security_sanitization_and_errors();

    /* Original contract assertion */
    assert(alrios_sandbox_setup_cgroups("slot_alpha", 1234) == ALRIOS_SANDBOX_OK);
    assert(alrios_sandbox_cleanup_cgroups("slot_alpha") == ALRIOS_SANDBOX_OK);

    printf("TASK-018 (Cgroups v2 256MB Cap): PASS\n");
    return 0;
}
