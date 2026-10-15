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

void *os_module_load(const char *path) {
    return (void *)LoadLibraryA(path);
}

void *os_module_sym(void *handle, const char *name) {
    union {
        FARPROC function;
        void *object;
    } symbol;
    symbol.function = GetProcAddress((HMODULE)handle, name);
    return symbol.object;
}

int os_module_unload(void *handle) {
    return FreeLibrary((HMODULE)handle) ? 0 : -1;
}
