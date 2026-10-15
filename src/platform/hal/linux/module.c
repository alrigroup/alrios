/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */
#include "hal_private.h"
#include <dlfcn.h>
#include <stddef.h>

void *os_module_load(const char *path) {
    void *handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
#ifdef AR_DEBUG
        fprintf(stderr, "dlopen(%s): %s\n", path, dlerror());
#endif
    }
    return handle;
}

void *os_module_sym(void *handle, const char *name) {
    return dlsym(handle, name);
}

int os_module_unload(void *handle) {
    if (dlclose(handle) != 0) return -1;
    return 0;
}
