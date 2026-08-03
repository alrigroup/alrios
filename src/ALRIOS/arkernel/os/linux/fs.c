/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

static int os_fs_mkdir(const char *path) {
    if (mkdir(path, 0755) == -1) return -errno;
    return 0;
}

static int os_fs_rmdir(const char *path) {
    if (rmdir(path) == -1) return -errno;
    return 0;
}

static int os_fs_exists(const char *path) {
    return access(path, F_OK) == 0 ? 1 : 0;
}
