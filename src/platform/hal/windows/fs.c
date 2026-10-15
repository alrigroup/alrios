/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */
#include "hal_private.h"
#include <windows.h>

int os_fs_mkdir(const char *path) {
    if (CreateDirectoryA(path, NULL)) return 0;
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;
    return -1;
}

int os_fs_rmdir(const char *path) {
    return RemoveDirectoryA(path) ? 0 : -1;
}

int os_fs_exists(const char *path) {
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES ? 1 : 0;
}
