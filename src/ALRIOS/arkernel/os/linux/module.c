/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

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
