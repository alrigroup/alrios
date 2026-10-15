/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */
#include "hal_private.h"
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

int os_fs_mkdir(const char *path) {
    if (mkdir(path, 0755) == -1) return -errno;
    return 0;
}

int os_fs_rmdir(const char *path) {
    if (rmdir(path) == -1) return -errno;
    return 0;
}

int os_fs_exists(const char *path) {
    return access(path, F_OK) == 0 ? 1 : 0;
}
