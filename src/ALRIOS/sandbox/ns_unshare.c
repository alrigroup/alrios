/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "alrios/sandbox_defs.h"

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define ALRIOS_NS_MAP_BUF_MAX 64U
#define ALRIOS_NS_WRITE_RETRY_MAX 16U

static int alrios_ns_is_valid_slot_id(const char *slot_id) {
    if (!slot_id || slot_id[0] == '\0') {
        return 0;
    }

    size_t len = 0U;
    for (const char *p = slot_id; *p != '\0'; ++p, ++len) {
        if (len >= ALRIOS_CGROUP_SLOT_MAX) {
            return 0;
        }

        const char c = *p;
        if (!((c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') ||
              c == '_' || c == '-')) {
            return 0;
        }
    }

    return (len > 0U) ? 1 : 0;
}

static int alrios_ns_write_all(int fd, const char *buf, size_t len) {
    if (fd < 0 || (!buf && len > 0U)) {
        return -1;
    }

    size_t offset = 0U;
    unsigned int retries = 0U;
    while (offset < len) {
        const ssize_t written = write(fd, buf + offset, len - offset);
        if (written < 0) {
            if (errno == EINTR && retries < ALRIOS_NS_WRITE_RETRY_MAX) {
                ++retries;
                continue;
            }
            return -1;
        }
        if (written == 0) {
            return -1;
        }
        offset += (size_t)written;
        retries = 0U;
    }

    return 0;
}

static int alrios_ns_write_file(const char *path, const char *content) {
    if (!path || !content) {
        return -1;
    }

    const size_t len = strnlen(content, ALRIOS_NS_MAP_BUF_MAX);
    if (len == 0U || len >= ALRIOS_NS_MAP_BUF_MAX) {
        return -1;
    }

    const int fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        return -1;
    }

    const int result = alrios_ns_write_all(fd, content, len);
    const int saved_errno = errno;
    if (close(fd) != 0 && result == 0) {
        return -1;
    }
    errno = saved_errno;
    return result;
}

static int alrios_ns_write_optional_file(const char *path, const char *content) {
    if (alrios_ns_write_file(path, content) == 0) {
        return 0;
    }
    return (errno == ENOENT) ? 0 : -1;
}

static int alrios_ns_map_current_user(uid_t host_uid, gid_t host_gid) {
    char map[ALRIOS_NS_MAP_BUF_MAX];

    if (alrios_ns_write_optional_file("/proc/self/setgroups", "deny\n") != 0) {
        return -1;
    }

    int n = snprintf(map, sizeof(map), "0 %lu 1\n", (unsigned long)host_gid);
    if (n <= 0 || (size_t)n >= sizeof(map)) {
        return -1;
    }
    if (alrios_ns_write_file("/proc/self/gid_map", map) != 0) {
        return -1;
    }

    n = snprintf(map, sizeof(map), "0 %lu 1\n", (unsigned long)host_uid);
    if (n <= 0 || (size_t)n >= sizeof(map)) {
        return -1;
    }
    if (alrios_ns_write_file("/proc/self/uid_map", map) != 0) {
        return -1;
    }

    return 0;
}

static int alrios_ns_unshare_pid_net_mount(uid_t host_uid, gid_t host_gid) {
    const int ns_flags = CLONE_NEWPID | CLONE_NEWNET | CLONE_NEWNS;

    if (unshare(ns_flags) == 0) {
        return 0;
    }

    if (!(errno == EPERM || errno == EACCES)) {
        return -1;
    }

    if (unshare(CLONE_NEWUSER) != 0) {
        return -1;
    }

    if (alrios_ns_map_current_user(host_uid, host_gid) != 0) {
        return -1;
    }

    if (unshare(ns_flags) != 0) {
        return -1;
    }

    return 0;
}

static int alrios_ns_wait_for_clean_exit(pid_t child) {
    int status = 0;
    pid_t waited;

    do {
        waited = waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);

    if (waited != child) {
        return -1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return -1;
    }

    return 0;
}

static void alrios_ns_probe_child_exit(uid_t host_uid, gid_t host_gid) {
    _exit((alrios_ns_unshare_pid_net_mount(host_uid, host_gid) == 0) ? 0 : 1);
}

static int alrios_ns_probe_kernel_support(void) {
    const uid_t host_uid = getuid();
    const gid_t host_gid = getgid();

    const pid_t probe = fork();
    if (probe < 0) {
        return -1;
    }

    if (probe == 0) {
        alrios_ns_probe_child_exit(host_uid, host_gid);
    }

    return alrios_ns_wait_for_clean_exit(probe);
}

int alrios_sandbox_enter_jail(const char *slot_id, pid_t pid) {
    if (!alrios_ns_is_valid_slot_id(slot_id) || pid <= 0) {
        return ALRIOS_SANDBOX_ERR_UNSHARE_FAIL;
    }

    return (alrios_ns_probe_kernel_support() == 0)
        ? ALRIOS_SANDBOX_OK
        : ALRIOS_SANDBOX_ERR_UNSHARE_FAIL;
}
