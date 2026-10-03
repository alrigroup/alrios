/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "aros_hal.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__linux__)
#include <fcntl.h>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/landlock.h>
#include <linux/seccomp.h>
#include <stddef.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef SECCOMP_RET_KILL_PROCESS
#define SECCOMP_RET_KILL_PROCESS SECCOMP_RET_KILL
#endif

#ifndef LANDLOCK_CREATE_RULESET_VERSION
#define LANDLOCK_CREATE_RULESET_VERSION (1U << 0)
#endif

static int landlock_create_ruleset_wrap(const struct landlock_ruleset_attr *attr,
                                        size_t size,
                                        uint32_t flags) {
#ifdef SYS_landlock_create_ruleset
    return (int)syscall(SYS_landlock_create_ruleset, attr, size, flags);
#else
    (void)attr;
    (void)size;
    (void)flags;
    errno = ENOSYS;
    return -1;
#endif
}

static int landlock_add_rule_wrap(int ruleset_fd,
                                  enum landlock_rule_type rule_type,
                                  const void *rule_attr,
                                  uint32_t flags) {
#ifdef SYS_landlock_add_rule
    return (int)syscall(SYS_landlock_add_rule, ruleset_fd, rule_type, rule_attr, flags);
#else
    (void)ruleset_fd;
    (void)rule_type;
    (void)rule_attr;
    (void)flags;
    errno = ENOSYS;
    return -1;
#endif
}

static int landlock_restrict_self_wrap(int ruleset_fd, uint32_t flags) {
#ifdef SYS_landlock_restrict_self
    return (int)syscall(SYS_landlock_restrict_self, ruleset_fd, flags);
#else
    (void)ruleset_fd;
    (void)flags;
    errno = ENOSYS;
    return -1;
#endif
}

static int add_landlock_path_rule(int ruleset_fd, const char *path, uint64_t rights) {
    struct landlock_path_beneath_attr rule;
    int fd;
    int saved_errno;

    if (!path || path[0] == '\0') {
        return 0;
    }

    fd = open(path, O_PATH | O_CLOEXEC);
    if (fd < 0) {
        return -errno;
    }

    rule.allowed_access = rights;
    rule.parent_fd = fd;

    if (landlock_add_rule_wrap(ruleset_fd, LANDLOCK_RULE_PATH_BENEATH, &rule, 0) != 0) {
        saved_errno = errno;
        (void)close(fd);
        return -saved_errno;
    }

    if (close(fd) != 0) {
        return -errno;
    }
    return 0;
}
#endif

int ar_process_enable_noexec_confinement(void) {
#if defined(__linux__)
#if defined(__x86_64__)
    const uint32_t expected_arch = AUDIT_ARCH_X86_64;
#elif defined(__aarch64__)
    const uint32_t expected_arch = AUDIT_ARCH_AARCH64;
#elif defined(__i386__)
    const uint32_t expected_arch = AUDIT_ARCH_I386;
#else
    return -ENOTSUP;
#endif
    struct sock_filter filter[] = {
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, (uint32_t)offsetof(struct seccomp_data, arch)),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, expected_arch, 1, 0),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, (uint32_t)offsetof(struct seccomp_data, nr)),
#if defined(__x86_64__)
        BPF_JUMP(BPF_JMP | BPF_JGE | BPF_K, 0x40000000U, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
#endif
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, (uint32_t)__NR_execve, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
#ifdef __NR_execveat
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, (uint32_t)__NR_execveat, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
#endif
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW)
    };
    struct sock_fprog prog;

    prog.len = (unsigned short)(sizeof(filter) / sizeof(filter[0]));
    prog.filter = filter;

    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        return -errno;
    }
    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog) != 0) {
        return -errno;
    }
    return 0;
#else
    return -ENOTSUP;
#endif
}

int ar_fs_restrict_to_paths(const char *const *read_roots,
                            size_t read_count,
                            const char *const *write_roots,
                            size_t write_count) {
#if defined(__linux__)
    const uint64_t read_rights = LANDLOCK_ACCESS_FS_READ_FILE |
                                 LANDLOCK_ACCESS_FS_READ_DIR;
    const uint64_t write_rights = read_rights |
                                  LANDLOCK_ACCESS_FS_WRITE_FILE |
                                  LANDLOCK_ACCESS_FS_REMOVE_DIR |
                                  LANDLOCK_ACCESS_FS_REMOVE_FILE |
                                  LANDLOCK_ACCESS_FS_MAKE_CHAR |
                                  LANDLOCK_ACCESS_FS_MAKE_DIR |
                                  LANDLOCK_ACCESS_FS_MAKE_REG |
                                  LANDLOCK_ACCESS_FS_MAKE_SOCK |
                                  LANDLOCK_ACCESS_FS_MAKE_FIFO |
                                  LANDLOCK_ACCESS_FS_MAKE_BLOCK |
                                  LANDLOCK_ACCESS_FS_MAKE_SYM;
    struct landlock_ruleset_attr ruleset;
    int abi;
    int ruleset_fd;
    int rc;
    int saved_errno;

    if ((read_count > 0U && !read_roots) || (write_count > 0U && !write_roots)) {
        return -EINVAL;
    }

    abi = landlock_create_ruleset_wrap(NULL, 0, LANDLOCK_CREATE_RULESET_VERSION);
    if (abi < 1) {
        return -ENOTSUP;
    }

    ruleset.handled_access_fs = write_rights;
    ruleset_fd = landlock_create_ruleset_wrap(&ruleset, sizeof(ruleset), 0);
    if (ruleset_fd < 0) {
        return -errno;
    }

    for (size_t i = 0; i < read_count; i++) {
        rc = add_landlock_path_rule(ruleset_fd, read_roots[i], read_rights);
        if (rc != 0) {
            (void)close(ruleset_fd);
            return rc;
        }
    }

    for (size_t i = 0; i < write_count; i++) {
        rc = add_landlock_path_rule(ruleset_fd, write_roots[i], write_rights);
        if (rc != 0) {
            (void)close(ruleset_fd);
            return rc;
        }
    }

    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        saved_errno = errno;
        (void)close(ruleset_fd);
        return -saved_errno;
    }

    if (landlock_restrict_self_wrap(ruleset_fd, 0) != 0) {
        saved_errno = errno;
        (void)close(ruleset_fd);
        return -saved_errno;
    }

    if (close(ruleset_fd) != 0) {
        return -errno;
    }
    return 0;
#else
    (void)read_roots;
    (void)read_count;
    (void)write_roots;
    (void)write_count;
    return -ENOTSUP;
#endif
}
