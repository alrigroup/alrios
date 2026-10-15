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
#include <stddef.h>

void *os_mem_alloc(size_t size) {
    return HeapAlloc(GetProcessHeap(), 0, size);
}

void os_mem_free(void *ptr) {
    HeapFree(GetProcessHeap(), 0, ptr);
}
