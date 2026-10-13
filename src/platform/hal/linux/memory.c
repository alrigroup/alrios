/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */
#include "hal_private.h"
#include <stdlib.h>

void *os_mem_alloc(size_t size) {
    return malloc(size);
}

void os_mem_free(void *ptr) {
    free(ptr);
}
