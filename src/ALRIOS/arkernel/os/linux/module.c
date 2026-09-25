/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */
#include <dlfcn.h>
#include <stddef.h>

static void *os_module_load(const char *path) {
    void *handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
#ifdef AR_DEBUG
        fprintf(stderr, "dlopen(%s): %s\n", path, dlerror());
#endif
    }
    return handle;
}

static void *os_module_sym(void *handle, const char *name) {
    return dlsym(handle, name);
}

static int os_module_unload(void *handle) {
    if (dlclose(handle) != 0) return -1;
    return 0;
}
