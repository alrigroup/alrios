/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "alrios/sandbox_defs.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/ptrace.h>
#include <time.h>

/* Disable ASan leak detection to prevent LSan fork/ptrace scan after seccomp activation */
#if defined(__has_feature)
#  if __has_feature(address_sanitizer)
const char *__asan_default_options(void) { return "detect_leaks=0"; }
#  endif
#elif defined(__SANITIZE_ADDRESS__)
const char *__asan_default_options(void) { return "detect_leaks=0"; }
#endif

static void test_whitelisted_and_unlisted_syscalls(void) {
    pid_t pid = fork();
    assert(pid >= 0);

    if (pid == 0) {
        if (alrios_sandbox_apply_seccomp() != ALRIOS_SANDBOX_OK) {
            syscall(SYS_exit_group, 1);
        }

        /* 1. Whitelisted: clock_gettime */
        struct timespec ts;
        if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
            syscall(SYS_exit_group, 2);
        }

        /* 2. Blocked socket domains: AF_NETLINK (16) -> EACCES */
        errno = 0;
        long s_ret = syscall(SYS_socket, 16, SOCK_RAW, 0);
        if (s_ret != -1 || errno != EACCES) {
            syscall(SYS_exit_group, 3);
        }

        /* 3. Blocked socket domains: AF_PACKET (17) -> EACCES */
        errno = 0;
        s_ret = syscall(SYS_socket, 17, SOCK_RAW, 0);
        if (s_ret != -1 || errno != EACCES) {
            syscall(SYS_exit_group, 4);
        }

        /* 4. Non-whitelisted syscall: SYS_getpid -> returns -1 with EPERM */
        errno = 0;
        long p_ret = syscall(SYS_getpid);
        if (p_ret != -1 || errno != EPERM) {
            syscall(SYS_exit_group, 5);
        }

        syscall(SYS_exit_group, 0);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 0);
}

static void test_blacklisted_kill_process(void) {
    pid_t pid = fork();
    assert(pid >= 0);

    if (pid == 0) {
        if (alrios_sandbox_apply_seccomp() != ALRIOS_SANDBOX_OK) {
            syscall(SYS_exit_group, 1);
        }

        /* SYS_ptrace is blacklisted: must trigger SECCOMP_RET_KILL_PROCESS immediately */
        syscall(SYS_ptrace, PTRACE_TRACEME, 0, NULL, NULL);

        /* Should never be reached */
        syscall(SYS_exit_group, 99);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    assert(WIFSIGNALED(status));
    assert(WTERMSIG(status) == SIGSYS);
}

int main(void) {
    /* Verify syscall permissions and denials in isolated child processes */
    test_whitelisted_and_unlisted_syscalls();
    test_blacklisted_kill_process();

    /* Direct filter application in main process */
    assert(alrios_sandbox_apply_seccomp() == ALRIOS_SANDBOX_OK);
    printf("TASK-017 (Seccomp-BPF Assembly Filter): PASS\n");
    return 0;
}
