/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stddef.h>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sys/syscall.h>
#include <sys/prctl.h>
#include <errno.h>

#include "alrios/sandbox_defs.h"

#define X32_SYSCALL_BIT 0x40000000

#if defined(__x86_64__)
    #define ALRIOS_AUDIT_ARCH AUDIT_ARCH_X86_64
#elif defined(__aarch64__)
    #define ALRIOS_AUDIT_ARCH AUDIT_ARCH_AARCH64
#else
    #error "Unsupported architecture for ALRIOS Seccomp Sandbox"
#endif

int alrios_sandbox_apply_seccomp(void) {
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        return ALRIOS_SANDBOX_ERR_PRCTL_FAIL;
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

        /* BLACKLISTED HIGH HAZARD SYSCALLS (Instant Kill) */
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

        /* RAW & PACKET NETWORK DENIAL */
        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_socket, 1, 0),
        BPF_STMT(BPF_JMP | BPF_JA, 6),

        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, (offsetof(struct seccomp_data, args[0]))),
        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, 16, 0, 1), /* Block AF_NETLINK */
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EACCES & SECCOMP_RET_DATA)),
        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, 17, 0, 1), /* Block AF_PACKET */
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EACCES & SECCOMP_RET_DATA)),

        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, (offsetof(struct seccomp_data, nr))),

        /* SAFE WHITELIST SYSCALLS */
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

        BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, SYS_clock_gettime, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        /* Default: Block and return EPERM */
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EPERM & SECCOMP_RET_DATA))
    };

    struct sock_fprog prog = {
        .len = (unsigned short)(sizeof(filter) / sizeof(filter[0])),
        .filter = filter,
    };

    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog) != 0) {
        return ALRIOS_SANDBOX_ERR_SECCOMP_FAIL;
    }

    return ALRIOS_SANDBOX_OK;
}
