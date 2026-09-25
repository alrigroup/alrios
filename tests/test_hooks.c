/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#include "alrios/hooks.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    alrios_hook_inv_t inv = {
        .stage = HOOK_PRE_SWAP,
        .app_id = "com.alrigroup.core",
        .slot_path = "/opt/alrios/slots/alpha",
        .timeout_ms = 5000
    };
    assert(alrios_hook_execute(&inv) == ALRIOS_HOOK_OK);
    printf("TASK-007 (Reactive Hooks): PASS\n");
    return 0;
}
