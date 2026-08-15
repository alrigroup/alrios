/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include <windows.h>

static void *os_module_load(const char *path) {
    return (void *)LoadLibraryA(path);
}

static void *os_module_sym(void *handle, const char *name) {
    return (void *)GetProcAddress((HMODULE)handle, name);
}

static int os_module_unload(void *handle) {
    return FreeLibrary((HMODULE)handle) ? 0 : -1;
}
