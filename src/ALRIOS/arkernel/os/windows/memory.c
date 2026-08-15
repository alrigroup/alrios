/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include <windows.h>
#include <stddef.h>

static void *os_mem_alloc(size_t size) {
    return HeapAlloc(GetProcessHeap(), 0, size);
}

static void os_mem_free(void *ptr) {
    HeapFree(GetProcessHeap(), 0, ptr);
}
