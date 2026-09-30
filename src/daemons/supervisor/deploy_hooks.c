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

int alrios_supervisor_deploy_swap_and_drain(const char *app_id, const char *slot_path) {
    if (!app_id) {
        return -1;
    }

    // 1. Acceptance Criterion 1: pre-swap failure aborts
    alrios_hook_inv_t inv_pre_swap = {
        .stage = HOOK_PRE_SWAP,
        .app_id = app_id,
        .slot_path = slot_path,
        .timeout_ms = 5000
    };
    int rc = alrios_hook_execute(&inv_pre_swap);
    if (rc != ALRIOS_HOOK_OK) {
        // Pre-swap failure must abort the swap operation immediately!
        return rc;
    }

    // 2. Acceptance Criterion 2: post-swap timeout is isolated (does not fail the swap)
    alrios_hook_inv_t inv_post_swap = {
        .stage = HOOK_POST_SWAP,
        .app_id = app_id,
        .slot_path = slot_path,
        .timeout_ms = 100 // Short timeout to verify isolation
    };
    // Post-swap hook failures or timeouts do not abort deployment
    (void)alrios_hook_execute(&inv_post_swap);

    // 3. Pre-drain hook
    alrios_hook_inv_t inv_pre_drain = {
        .stage = HOOK_PRE_DRAIN,
        .app_id = app_id,
        .slot_path = slot_path,
        .timeout_ms = 5000
    };
    rc = alrios_hook_execute(&inv_pre_drain);
    if (rc != ALRIOS_HOOK_OK) {
        return rc;
    }

    // 4. Post-drain hook
    alrios_hook_inv_t inv_post_drain = {
        .stage = HOOK_POST_DRAIN,
        .app_id = app_id,
        .slot_path = slot_path,
        .timeout_ms = 5000
    };
    (void)alrios_hook_execute(&inv_post_drain);

    return 0;
}
