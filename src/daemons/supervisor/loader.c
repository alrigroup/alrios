/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "alrios/supervisor/context.h"
#include <stdio.h>
#include <stdlib.h>

// Simulated loader integrating supervisor context natively (No file-scope registries allowed)
int alrios_supervisor_loader_reload(alrios_supervisor_ctx_t *ctx, const char *app_id) {
    if (!ctx || !app_id) {
        return ALRIOS_SUPERVISOR_ERR_INVALID;
    }
    // Simulation logic to represent atomic swap using injected context
    size_t count = alrios_supervisor_get_app_count(ctx);
    if (count == 0U) {
        return ALRIOS_SUPERVISOR_ERR_NOT_FOUND;
    }
    return ALRIOS_SUPERVISOR_OK;
}
