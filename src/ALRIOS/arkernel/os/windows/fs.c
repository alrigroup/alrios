/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */
#include <windows.h>

static int os_fs_mkdir(const char *path) {
    if (CreateDirectoryA(path, NULL)) return 0;
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;
    return -1;
}

static int os_fs_rmdir(const char *path) {
    return RemoveDirectoryA(path) ? 0 : -1;
}

static int os_fs_exists(const char *path) {
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES ? 1 : 0;
}
