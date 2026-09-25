/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */
#include <windows.h>
#include <stddef.h>

static void *os_mem_alloc(size_t size) {
    return HeapAlloc(GetProcessHeap(), 0, size);
}

static void os_mem_free(void *ptr) {
    HeapFree(GetProcessHeap(), 0, ptr);
}
