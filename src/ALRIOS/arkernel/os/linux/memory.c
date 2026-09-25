/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */
#include <stdlib.h>

static void *os_mem_alloc(size_t size) {
    return malloc(size);
}

static void os_mem_free(void *ptr) {
    free(ptr);
}
