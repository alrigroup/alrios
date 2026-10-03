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
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>

int alrios_sandbox_launch_jailed(
    const char *slot_id,
    const char *executable_path,
    char *const argv[],
    char *const envp[],
    pid_t *out_pid
) {
    if (!slot_id || !executable_path || !out_pid) {
        return ALRIOS_SANDBOX_ERR_UNSHARE_FAIL;
    }

    *out_pid = -1;

    pid_t pid = fork();
    if (pid < 0) {
        return ALRIOS_SANDBOX_ERR_UNSHARE_FAIL;
    }

    if (pid == 0) {
        pid_t child_pid = getpid();
        if (alrios_sandbox_enter_jail(slot_id, child_pid) != ALRIOS_SANDBOX_OK) {
            _exit(126);
        }
        if (alrios_sandbox_apply_seccomp() != ALRIOS_SANDBOX_OK) {
            _exit(127);
        }
        execve(executable_path, argv, envp);
        _exit(125);
    }

    if (alrios_sandbox_setup_cgroups(slot_id, pid) != ALRIOS_SANDBOX_OK) {
        kill(pid, SIGKILL);
        int status = 0;
        waitpid(pid, &status, 0);
        alrios_sandbox_cleanup_cgroups(slot_id);
        return ALRIOS_SANDBOX_ERR_CGROUP_FAIL;
    }

    *out_pid = pid;
    return ALRIOS_SANDBOX_OK;
}
