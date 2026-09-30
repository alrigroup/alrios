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

#include "alrios/hooks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int alrios_armake_run_pipeline(const char *app_id, const char *slot_path) {
    if (!app_id) {
        return -1;
    }

    alrios_hook_inv_t inv_pre = {
        .stage = HOOK_PRE_BUILD,
        .app_id = app_id,
        .slot_path = slot_path,
        .timeout_ms = 5000
    };

    int rc = alrios_hook_execute(&inv_pre);
    if (rc != ALRIOS_HOOK_OK) {
        return rc;
    }

    alrios_hook_inv_t inv_post = {
        .stage = HOOK_POST_BUILD,
        .app_id = app_id,
        .slot_path = slot_path,
        .timeout_ms = 5000
    };

    return alrios_hook_execute(&inv_post);
}
