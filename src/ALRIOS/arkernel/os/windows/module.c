/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */
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
