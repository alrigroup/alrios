/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "alrios/sandbox_defs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define ALRIOS_CGROUP_FALLBACK_ROOT "/tmp/alrios_cgroup"

static char s_custom_cgroup_root[PATH_MAX] = {0};

int alrios_sandbox_set_cgroup_root(const char *root_path) {
    if (!root_path) {
        s_custom_cgroup_root[0] = '\0';
        return ALRIOS_SANDBOX_OK;
    }
    size_t len = strnlen(root_path, PATH_MAX);
    if (len == 0 || len >= PATH_MAX || root_path[0] != '/') {
        return ALRIOS_SANDBOX_ERR_CGROUP_FAIL;
    }
    int n = snprintf(s_custom_cgroup_root, sizeof(s_custom_cgroup_root), "%s", root_path);
    if (n < 0 || (size_t)n >= sizeof(s_custom_cgroup_root)) {
        return ALRIOS_SANDBOX_ERR_CGROUP_FAIL;
    }
    return ALRIOS_SANDBOX_OK;
}

const char *alrios_sandbox_get_cgroup_root(void) {
    if (s_custom_cgroup_root[0] != '\0') {
        return s_custom_cgroup_root;
    }
    const char *env_root = getenv("ALRIOS_CGROUP_ROOT");
    if (env_root && env_root[0] == '/') {
        return env_root;
    }
    if (access("/sys/fs/cgroup", W_OK) == 0) {
        return ALRIOS_CGROUP_DEFAULT_ROOT;
    }
    return ALRIOS_CGROUP_FALLBACK_ROOT;
}

static int is_valid_slot_id(const char *slot_id) {
    if (!slot_id || slot_id[0] == '\0') {
        return 0;
    }
    size_t len = 0;
    for (const char *p = slot_id; *p != '\0'; p++, len++) {
        if (len >= ALRIOS_CGROUP_SLOT_MAX) {
            return 0;
        }
        char c = *p;
        /* Zero Trust: Whitelist only [a-zA-Z0-9_-] */
        if (!((c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') ||
              c == '_' || c == '-')) {
            return 0;
        }
    }
    return (len > 0);
}

static int ensure_dir_exists(const char *dir) {
    if (!dir || dir[0] == '\0') {
        return -1;
    }
    char tmp[PATH_MAX];
    size_t len = strnlen(dir, sizeof(tmp));
    if (len == 0 || len >= sizeof(tmp)) {
        return -1;
    }
    memcpy(tmp, dir, len + 1);

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0) {
                if (errno != EEXIST) {
                    return -1;
                }
                struct stat st;
                if (stat(tmp, &st) != 0 || !S_ISDIR(st.st_mode)) {
                    return -1;
                }
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0) {
        if (errno != EEXIST) {
            return -1;
        }
        struct stat st;
        if (stat(tmp, &st) != 0 || !S_ISDIR(st.st_mode)) {
            return -1;
        }
    }
    return 0;
}

static int write_control_file(const char *dir, const char *filename, const char *content) {
    char path[PATH_MAX];
    int n = snprintf(path, sizeof(path), "%s/%s", dir, filename);
    if (n < 0 || (size_t)n >= sizeof(path)) {
        return -1;
    }

    int fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0 && errno == ENOENT) {
        fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    }
    if (fd < 0) {
        return -1;
    }

    size_t len = strlen(content);
    ssize_t written = write(fd, content, len);
    (void)close(fd);

    if (written < 0 || (size_t)written != len) {
        return -1;
    }
    return 0;
}

static void try_enable_controllers(const char *root_dir) {
    char path[PATH_MAX];
    int n = snprintf(path, sizeof(path), "%s/cgroup.subtree_control", root_dir);
    if (n < 0 || (size_t)n >= sizeof(path)) {
        return;
    }
    int fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd >= 0) {
        const char *ctrls = "+memory +pids\n";
        size_t len = strlen(ctrls);
        ssize_t written = write(fd, ctrls, len);
        if (written < 0 || (size_t)written != len) {
            /* Controller subtree activation is best-effort on unprivileged mounts */
        }
        (void)close(fd);
    }
}

int alrios_sandbox_setup_cgroups(const char *slot_id, pid_t pid) {
    if (!is_valid_slot_id(slot_id) || pid <= 0) {
        return ALRIOS_SANDBOX_ERR_CGROUP_FAIL;
    }

    const char *root = alrios_sandbox_get_cgroup_root();
    if (!root || root[0] != '/') {
        return ALRIOS_SANDBOX_ERR_CGROUP_FAIL;
    }

    if (ensure_dir_exists(root) != 0) {
        return ALRIOS_SANDBOX_ERR_CGROUP_FAIL;
    }

    try_enable_controllers(root);

    char slot_path[PATH_MAX];
    int n = snprintf(slot_path, sizeof(slot_path), "%s/%s", root, slot_id);
    if (n < 0 || (size_t)n >= sizeof(slot_path)) {
        return ALRIOS_SANDBOX_ERR_CGROUP_FAIL;
    }

    if (ensure_dir_exists(slot_path) != 0) {
        return ALRIOS_SANDBOX_ERR_CGROUP_FAIL;
    }

    char mem_val[32];
    char swap_val[32];
    char pids_val[32];
    char procs_val[32];

    n = snprintf(mem_val, sizeof(mem_val), "%llu\n", (unsigned long long)ALRIOS_CGROUP_RAM_CAP_BYTES);
    if (n < 0 || (size_t)n >= sizeof(mem_val)) {
        return ALRIOS_SANDBOX_ERR_CGROUP_FAIL;
    }

    n = snprintf(swap_val, sizeof(swap_val), "%llu\n", (unsigned long long)ALRIOS_CGROUP_SWAP_MAX_BYTES);
    if (n < 0 || (size_t)n >= sizeof(swap_val)) {
        return ALRIOS_SANDBOX_ERR_CGROUP_FAIL;
    }

    n = snprintf(pids_val, sizeof(pids_val), "%u\n", (unsigned int)ALRIOS_CGROUP_PIDS_MAX);
    if (n < 0 || (size_t)n >= sizeof(pids_val)) {
        return ALRIOS_SANDBOX_ERR_CGROUP_FAIL;
    }

    n = snprintf(procs_val, sizeof(procs_val), "%ld\n", (long)pid);
    if (n < 0 || (size_t)n >= sizeof(procs_val)) {
        return ALRIOS_SANDBOX_ERR_CGROUP_FAIL;
    }

    if (write_control_file(slot_path, "memory.max", mem_val) != 0) {
        return ALRIOS_SANDBOX_ERR_CGROUP_FAIL;
    }

    if (write_control_file(slot_path, "memory.swap.max", swap_val) != 0) {
        return ALRIOS_SANDBOX_ERR_CGROUP_FAIL;
    }

    if (write_control_file(slot_path, "pids.max", pids_val) != 0) {
        return ALRIOS_SANDBOX_ERR_CGROUP_FAIL;
    }

    if (write_control_file(slot_path, "cgroup.procs", procs_val) != 0) {
        return ALRIOS_SANDBOX_ERR_CGROUP_FAIL;
    }

    return ALRIOS_SANDBOX_OK;
}

int alrios_sandbox_cleanup_cgroups(const char *slot_id) {
    if (!is_valid_slot_id(slot_id)) {
        return ALRIOS_SANDBOX_ERR_CGROUP_FAIL;
    }

    const char *root = alrios_sandbox_get_cgroup_root();
    if (!root || root[0] != '/') {
        return ALRIOS_SANDBOX_ERR_CGROUP_FAIL;
    }

    char slot_path[PATH_MAX];
    int n = snprintf(slot_path, sizeof(slot_path), "%s/%s", root, slot_id);
    if (n < 0 || (size_t)n >= sizeof(slot_path)) {
        return ALRIOS_SANDBOX_ERR_CGROUP_FAIL;
    }

    const char *files[] = {"cgroup.procs", "memory.max", "memory.swap.max", "pids.max"};
    char file_path[PATH_MAX];
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        n = snprintf(file_path, sizeof(file_path), "%s/%s", slot_path, files[i]);
        if (n > 0 && (size_t)n < sizeof(file_path)) {
            (void)unlink(file_path);
        }
    }

    (void)rmdir(slot_path);
    return ALRIOS_SANDBOX_OK;
}
