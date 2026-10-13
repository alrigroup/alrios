/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */
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
