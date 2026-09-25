/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "alrios/atomic_swap.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

int alrios_atomic_symlink_swap(const char *target_dir, const char *symlink_current, pid_t master_pid) {
    if (!target_dir || !symlink_current || target_dir[0] == '\0' || symlink_current[0] == '\0' || master_pid < 0) {
        return -1;
    }

    char tmp[PATH_MAX];
    int n = snprintf(tmp, sizeof(tmp), "%s.tmp.%ld", symlink_current, (long)getpid());
    if (n < 0 || (size_t)n >= sizeof(tmp)) {
        return -1;
    }

    /* Remove any stale temporary link from prior interrupted executions */
    (void)unlink(tmp);

    /* Create the candidate symlink pointing to the new release target */
    if (symlink(target_dir, tmp) != 0) {
        return -2;
    }

    /*
     * Perform atomic replacement via renameat2 syscall (Linux 3.15+).
     * If renameat2 returns ENOSYS, fall back to atomic rename().
     */
    int res = renameat2(AT_FDCWD, tmp, AT_FDCWD, symlink_current, 0);
    if (res != 0 && errno == ENOSYS) {
        res = rename(tmp, symlink_current);
    }

    if (res != 0) {
        (void)unlink(tmp);
        return -3;
    }

    /*
     * Notify master daemon to hot-reload / drain connections via SIGHUP.
     * PID <= 1 is guarded to prevent accidental signaling of PID 0 (pgrp) or PID 1 (init).
     */
    if (master_pid > 1) {
        if (kill(master_pid, SIGHUP) != 0) {
            return -4;
        }
    }

    return 0;
}
