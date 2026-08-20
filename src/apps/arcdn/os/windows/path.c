/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include <windows.h>
#include <string.h>

static int os_get_exe_dir(char *buf, int size) {
    DWORD n = GetModuleFileNameA(NULL, buf, (DWORD)size);
    if (n == 0 || n >= (DWORD)size) return -1;
    char *p = strrchr(buf, '\\');
    if (p) { *p = '\0'; return 0; }
    p = strrchr(buf, '/');
    if (p) { *p = '\0'; return 0; }
    strcpy(buf, ".");
    return 0;
}
