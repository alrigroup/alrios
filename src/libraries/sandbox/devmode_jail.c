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

#include "alrios/sandbox/devmode_jail.h"
#include "alrios/sandbox_defs.h"
#include "alrios/package/arapp_v2.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __linux__
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <net/if.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/syscall.h>

#define X32_SYSCALL_BIT 0x40000000

#if defined(__x86_64__)
    #define ALRIOS_AUDIT_ARCH AUDIT_ARCH_X86_64
#elif defined(__aarch64__)
    #define ALRIOS_AUDIT_ARCH AUDIT_ARCH_AARCH64
#else
    #error "Unsupported architecture for ALRIOS DevMode Sandbox"
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define ALRIOS_DEVMODE_CPU_MAX_DEFAULT "50000 100000\n"

static int is_valid_slot_string(const char *slot_id) {
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

static int write_control_value(const char *dir, const char *file, const char *val) {
    char path[PATH_MAX];
    int n = snprintf(path, sizeof(path), "%s/%s", dir, file);
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

    size_t len = strlen(val);
    ssize_t written = write(fd, val, len);
    (void)close(fd);

    if (written < 0 || (size_t)written != len) {
        return -1;
    }
    return 0;
}

static int map_user_ns_root(uid_t host_uid, gid_t host_gid) {
    int fd = open("/proc/self/setgroups", O_WRONLY | O_CLOEXEC);
    if (fd >= 0) {
        (void)write(fd, "deny\n", 5);
        (void)close(fd);
    }

    char map[64];
    int n = snprintf(map, sizeof(map), "0 %lu 1\n", (unsigned long)host_gid);
    if (n <= 0 || (size_t)n >= sizeof(map)) {
        return -1;
    }
    fd = open("/proc/self/gid_map", O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        return -1;
    }
    if (write(fd, map, (size_t)n) != (ssize_t)n) {
        (void)close(fd);
        return -1;
    }
    (void)close(fd);

    n = snprintf(map, sizeof(map), "0 %lu 1\n", (unsigned long)host_uid);
    if (n <= 0 || (size_t)n >= sizeof(map)) {
        return -1;
    }
    fd = open("/proc/self/uid_map", O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        return -1;
    }
    if (write(fd, map, (size_t)n) != (ssize_t)n) {
        (void)close(fd);
        return -1;
    }
    (void)close(fd);

    return 0;
}

static int bring_loopback_up(void) {
    int s = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (s < 0) {
        return -1;
    }

    struct ifreq ifr;
    (void)memset(&ifr, 0, sizeof(ifr));
    int n = snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "lo");
    if (n <= 0 || (size_t)n >= sizeof(ifr.ifr_name)) {
        (void)close(s);
        return -1;
    }

    if (ioctl(s, SIOCGIFFLAGS, &ifr) != 0) {
        (void)close(s);
        return -1;
    }

    ifr.ifr_flags = (short)((unsigned short)ifr.ifr_flags | (IFF_UP | IFF_RUNNING));
    if (ioctl(s, SIOCSIFFLAGS, &ifr) != 0) {
        (void)close(s);
        return -1;
    }

    (void)close(s);
    return 0;
}
#endif /* __linux__ */

int alrios_devmode_policy_evaluate(const alrios_devmode_policy_t *policy,
                                   const alrios_devmode_request_t *request,
                                   alrios_devmode_admission_t *out_admission) {
    if (!policy || !request || !out_admission) {
        return ALRIOS_DEVMODE_POLICY_ERR_INVALID;
    }
    if (policy->abi_version != ALRIOS_DEVMODE_POLICY_ABI_VERSION) {
        return ALRIOS_DEVMODE_POLICY_ERR_INVALID;
    }
    if (policy->deployment != ALRIOS_DEPLOYMENT_DEVELOPMENT &&
        policy->deployment != ALRIOS_DEPLOYMENT_PRODUCTION) {
        return ALRIOS_DEVMODE_POLICY_ERR_INVALID;
    }
    if (request->artifact_trust != ALRIOS_ARTIFACT_UNSIGNED &&
        request->artifact_trust != ALRIOS_ARTIFACT_SIGNATURE_VERIFIED) {
        return ALRIOS_DEVMODE_POLICY_ERR_INVALID;
    }
    if (request->requested_ring != ALRIOS_EXECUTION_RING_DEVMODE &&
        request->requested_ring != ALRIOS_EXECUTION_RING_SOVEREIGN) {
        return ALRIOS_DEVMODE_POLICY_ERR_INVALID;
    }

    if (policy->deployment == ALRIOS_DEPLOYMENT_PRODUCTION || policy->devmode_enabled == 0) {
        if (request->artifact_trust == ALRIOS_ARTIFACT_UNSIGNED ||
            request->requested_ring == ALRIOS_EXECUTION_RING_DEVMODE) {
            return ALRIOS_DEVMODE_POLICY_ERR_DISABLED;
        }
    }

    if (request->artifact_trust == ALRIOS_ARTIFACT_UNSIGNED) {
        if (request->requested_ring == ALRIOS_EXECUTION_RING_SOVEREIGN) {
            return ALRIOS_DEVMODE_POLICY_ERR_UNSIGNED_SOVEREIGN;
        }
        out_admission->abi_version = ALRIOS_DEVMODE_POLICY_ABI_VERSION;
        out_admission->artifact_trust = ALRIOS_ARTIFACT_UNSIGNED;
        out_admission->execution_ring = ALRIOS_EXECUTION_RING_DEVMODE;
        return ALRIOS_DEVMODE_POLICY_OK;
    }

    out_admission->abi_version = ALRIOS_DEVMODE_POLICY_ABI_VERSION;
    out_admission->artifact_trust = ALRIOS_ARTIFACT_SIGNATURE_VERIFIED;
    out_admission->execution_ring = request->requested_ring;
    return ALRIOS_DEVMODE_POLICY_OK;
}

int alrios_devmode_jail_validate_admission(
    alrios_profile_t node_profile,
    int devmode_allowed_on_node,
    int app_marked_devmode,
    int app_signed
) {
    if (node_profile == PROFILE_SOVEREIGN_MAX) {
        if (!app_signed || app_marked_devmode) {
            return ALRIOS_DEVMODE_JAIL_ERR_SOVEREIGN_DENIED;
        }
        return ALRIOS_DEVMODE_JAIL_OK;
    }

    if (!app_signed) {
        if (!devmode_allowed_on_node || !app_marked_devmode) {
            return ALRIOS_DEVMODE_JAIL_ERR_REJECTED;
        }
        return ALRIOS_DEVMODE_JAIL_OK;
    }

    if (app_marked_devmode && !devmode_allowed_on_node) {
        return ALRIOS_DEVMODE_JAIL_ERR_REJECTED;
    }

    return ALRIOS_DEVMODE_JAIL_OK;
}

int alrios_devmode_jail_setup_cgroups(
    const alrios_devmode_jail_config_t *config
) {
#ifdef __linux__
    if (!config || !is_valid_slot_string(config->slot_id) || config->target_pid <= 0) {
        return ALRIOS_DEVMODE_JAIL_ERR_INVALID;
    }

    int rc = alrios_sandbox_setup_cgroups(config->slot_id, config->target_pid);
    if (rc != ALRIOS_SANDBOX_OK) {
        return ALRIOS_DEVMODE_JAIL_ERR_CGROUP;
    }

    const char *root = alrios_sandbox_get_cgroup_root();
    if (!root || root[0] != '/') {
        return ALRIOS_DEVMODE_JAIL_ERR_CGROUP;
    }

    char slot_path[PATH_MAX];
    int n = snprintf(slot_path, sizeof(slot_path), "%s/%s", root, config->slot_id);
    if (n < 0 || (size_t)n >= sizeof(slot_path)) {
        return ALRIOS_DEVMODE_JAIL_ERR_CGROUP;
    }

    if (write_control_value(slot_path, "cpu.max", ALRIOS_DEVMODE_CPU_MAX_DEFAULT) != 0) {
        return ALRIOS_DEVMODE_JAIL_ERR_CGROUP;
    }

    if (config->memory_max_bytes > 0U) {
        char mem_val[32];
        n = snprintf(mem_val, sizeof(mem_val), "%llu\n",
                     (unsigned long long)config->memory_max_bytes);
        if (n > 0 && (size_t)n < sizeof(mem_val)) {
            if (write_control_value(slot_path, "memory.max", mem_val) != 0) {
                return ALRIOS_DEVMODE_JAIL_ERR_CGROUP;
            }
        }
    }

    if (config->pids_max > 0U) {
        char pids_val[32];
        n = snprintf(pids_val, sizeof(pids_val), "%u\n",
                     (unsigned int)config->pids_max);
        if (n > 0 && (size_t)n < sizeof(pids_val)) {
            if (write_control_value(slot_path, "pids.max", pids_val) != 0) {
                return ALRIOS_DEVMODE_JAIL_ERR_CGROUP;
            }
        }
    }

    return ALRIOS_DEVMODE_JAIL_OK;
#else
    (void)config;
    return ALRIOS_DEVMODE_JAIL_ERR_CGROUP;
#endif
}

int alrios_devmode_jail_cleanup_cgroups(const char *slot_id) {
#ifdef __linux__
    if (!is_valid_slot_string(slot_id)) {
        return ALRIOS_DEVMODE_JAIL_ERR_INVALID;
    }

    const char *root = alrios_sandbox_get_cgroup_root();
    if (root && root[0] == '/') {
        char cpu_path[PATH_MAX];
        int n = snprintf(cpu_path, sizeof(cpu_path), "%s/%s/cpu.max", root, slot_id);
        if (n > 0 && (size_t)n < sizeof(cpu_path)) {
            (void)unlink(cpu_path);
        }
    }

    int rc = alrios_sandbox_cleanup_cgroups(slot_id);
    return (rc == ALRIOS_SANDBOX_OK) ? ALRIOS_DEVMODE_JAIL_OK : ALRIOS_DEVMODE_JAIL_ERR_CGROUP;
#else
    (void)slot_id;
    return ALRIOS_DEVMODE_JAIL_ERR_CGROUP;
#endif
}

int alrios_devmode_jail_apply_namespaces(
    const alrios_devmode_jail_config_t *config
) {
#ifdef __linux__
    const int ns_flags = CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWUTS | CLONE_NEWNET;

    if (unshare(ns_flags) != 0) {
        if (!(errno == EPERM || errno == EACCES)) {
            return ALRIOS_DEVMODE_JAIL_ERR_UNSHARE;
        }

        const uid_t uid = getuid();
        const gid_t gid = getgid();

        if (unshare(CLONE_NEWUSER) != 0) {
            return ALRIOS_DEVMODE_JAIL_ERR_UNSHARE;
        }

        if (map_user_ns_root(uid, gid) != 0) {
            return ALRIOS_DEVMODE_JAIL_ERR_UNSHARE;
        }

        if (unshare(ns_flags) != 0) {
            return ALRIOS_DEVMODE_JAIL_ERR_UNSHARE;
        }
    }

    (void)sethostname("alrios-devmode-jail", 19);
    (void)mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL);

    if (config && config->net_mode == ALRIOS_DEVMODE_NET_LOOPBACK_ONLY) {
        if (bring_loopback_up() != 0) {
            return ALRIOS_DEVMODE_JAIL_ERR_NET;
        }
    }

    return ALRIOS_DEVMODE_JAIL_OK;
#else
    (void)config;
    return ALRIOS_DEVMODE_JAIL_ERR_UNSHARE;
#endif
}

int alrios_devmode_jail_apply_seccomp(void) {
#ifdef __linux__
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        return ALRIOS_DEVMODE_JAIL_ERR_SECCOMP;
    }

    struct sock_filter filter[] = {
        /* [0] Architecture Verification */
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, (offsetof(struct seccomp_data, arch))),
        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, ALRIOS_AUDIT_ARCH, 1, 0),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),

        /* [3] Load Syscall Number */
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, (offsetof(struct seccomp_data, nr))),

#if defined(__x86_64__)
        /* Block x32 ABI calls */
        BPF_JUMP(BPF_JMP | BPF_K | BPF_JGE, X32_SYSCALL_BIT, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
#endif

        /* BLACKLISTED HIGH HAZARD SYSCALLS (Instant Kill -> SIGSYS) */
        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_ptrace, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_reboot, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_mount, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_umount2, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_kexec_load, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_chroot, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_bpf, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),

        /* RAW & PACKET NETWORK DENIAL (Block socket domains and SOCK_RAW) */
        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_socket, 1, 0),
        BPF_STMT(BPF_JMP | BPF_JA, 10),

        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, (offsetof(struct seccomp_data, args[0]))),
        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, 16, 0, 1), /* Block AF_NETLINK */
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EPERM & SECCOMP_RET_DATA)),
        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, 17, 0, 1), /* Block AF_PACKET */
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EPERM & SECCOMP_RET_DATA)),

        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, (offsetof(struct seccomp_data, args[1]))),
        BPF_STMT(BPF_ALU | BPF_K | BPF_AND, 0xf),
        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SOCK_RAW, 0, 1), /* Block SOCK_RAW */
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EPERM & SECCOMP_RET_DATA)),

        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, (offsetof(struct seccomp_data, nr))),

        /* SAFE WHITELISTED SYSCALLS */
        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_read, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_write, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_close, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_mmap, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_mprotect, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_munmap, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_brk, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_exit_group, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_exit, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_sigaltstack, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_rt_sigaction, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_rt_sigprocmask, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_clock_gettime, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_getpid, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_getuid, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_getgid, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_socket, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_connect, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_bind, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_nanosleep, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_uname, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        /* Default: Block and return EPERM */
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EPERM & SECCOMP_RET_DATA))
    };

    struct sock_fprog prog = {
        .len = (unsigned short)(sizeof(filter) / sizeof(filter[0])),
        .filter = filter,
    };

    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog) != 0) {
        return ALRIOS_DEVMODE_JAIL_ERR_SECCOMP;
    }

    return ALRIOS_DEVMODE_JAIL_OK;
#else
    return ALRIOS_DEVMODE_JAIL_ERR_SECCOMP;
#endif
}

int alrios_devmode_jail_enter(
    alrios_profile_t node_profile,
    int devmode_allowed_on_node,
    int app_marked_devmode,
    int app_signed,
    const alrios_devmode_jail_config_t *config
) {
    int rc = alrios_devmode_jail_validate_admission(
        node_profile,
        devmode_allowed_on_node,
        app_marked_devmode,
        app_signed
    );
    if (rc != ALRIOS_DEVMODE_JAIL_OK) {
        return rc;
    }

    if (config && config->slot_id) {
        rc = alrios_devmode_jail_setup_cgroups(config);
        if (rc != ALRIOS_DEVMODE_JAIL_OK) {
            return rc;
        }
    }

    rc = alrios_devmode_jail_apply_namespaces(config);
    if (rc != ALRIOS_DEVMODE_JAIL_OK) {
        return rc;
    }

    rc = alrios_devmode_jail_apply_seccomp();
    if (rc != ALRIOS_DEVMODE_JAIL_OK) {
        return rc;
    }

    return ALRIOS_DEVMODE_JAIL_OK;
}
