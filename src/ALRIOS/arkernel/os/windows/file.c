/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */
#include <windows.h>
#include <stddef.h>
#include <errno.h>

static int os_file_create(const char *directory, const char *filename, const char *content) {
    if (!CreateDirectoryA(directory, NULL)) {
        if (GetLastError() != ERROR_ALREADY_EXISTS)
            return -1;
    }

    size_t dirlen = strlen(directory);
    size_t filelen = strlen(filename);
    char fullpath[4096];
    if (dirlen + 1 + filelen >= sizeof(fullpath))
        return -ENAMETOOLONG;
    memcpy(fullpath, directory, dirlen);
    fullpath[dirlen] = '\\';
    memcpy(fullpath + dirlen + 1, filename, filelen + 1);

    HANDLE hFile = CreateFileA(fullpath, GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return -1;

    DWORD written;
    size_t len = strlen(content);
    BOOL ok = WriteFile(hFile, content, (DWORD)len, &written, NULL);
    CloseHandle(hFile);

    if (!ok || (size_t)written != len)
        return -1;

    return 0;
}

static int os_file_delete(const char *path) {
    return DeleteFileA(path) ? 0 : -1;
}

static int os_file_exists(const char *path) {
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES ? 1 : 0;
}
