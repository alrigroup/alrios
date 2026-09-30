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

#include "alrios/sandbox_defs.h"
#include "alrios/package/arapp_v2.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/socket.h>
#include <errno.h>

#if defined(__has_feature)
#  if __has_feature(address_sanitizer)
const char *__asan_default_options(void) { return "detect_leaks=0"; }
#  endif
#elif defined(__SANITIZE_ADDRESS__)
const char *__asan_default_options(void) { return "detect_leaks=0"; }
#endif

extern int supervisor_devmode_validate(const arapp_v2_package_t *pkg, int is_signed, int devmode_enabled);
extern int alrios_sandbox_launch_jailed(const char *slot_id, const char *executable_path, char *const argv[], char *const envp[], pid_t *out_pid);

static void test_unsigned_sovereign_rejection(void) {
    arapp_v2_package_t pkg;
    memset(&pkg, 0, sizeof(pkg));
    const char *manifest = "{\"execution_ring\":\"sovereign_trust\"}";
    pkg.manifest_json = (char *)manifest;
    pkg.manifest_len = strlen(manifest);

    assert(supervisor_devmode_validate(&pkg, 0, 1) != 0);
}

static void test_production_disable_devmode(void) {
    arapp_v2_package_t pkg;
    memset(&pkg, 0, sizeof(pkg));
    const char *manifest = "{\"execution_ring\":\"devmode_sandbox\"}";
    pkg.manifest_json = (char *)manifest;
    pkg.manifest_len = strlen(manifest);

    assert(supervisor_devmode_validate(&pkg, 0, 0) != 0);
}

static void test_escape_attempts(void) {
    pid_t pid = fork();
    assert(pid >= 0);

    if (pid == 0) {
        if (alrios_sandbox_apply_seccomp() != ALRIOS_SANDBOX_OK) {
            syscall(SYS_exit_group, 1);
        }

        errno = 0;
        long p_ret = syscall(SYS_ptrace, PTRACE_TRACEME, 0, NULL, NULL);
        (void)p_ret;

        syscall(SYS_exit_group, 0);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    assert(WIFSIGNALED(status) || (WIFEXITED(status) && WEXITSTATUS(status) != 0));
}

int main(void) {
    test_unsigned_sovereign_rejection();
    test_production_disable_devmode();
    test_escape_attempts();

    printf("TEST_DEVMODE_ESCAPE: PASS\n");
    return 0;
}
