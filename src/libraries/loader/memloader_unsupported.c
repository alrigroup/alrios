/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/memloader.h"

int alrios_memfd_create_sealed(const char *name,
                               const uint8_t *payload,
                               size_t size) {
    (void)name;
    (void)payload;
    (void)size;
    return ALRIOS_MEMLOADER_ERR_EXEC_DENIED;
}
