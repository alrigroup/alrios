/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#ifndef ALRIOS_HOOKS_H
#define ALRIOS_HOOKS_H

#include <stdint.h>
#include <stddef.h>

#define ALRIOS_HOOK_OK           0
#define ALRIOS_HOOK_ERR_TIMEOUT -301
#define ALRIOS_HOOK_ERR_FAIL    -302

typedef enum alrios_hook_stage {
    HOOK_PRE_BUILD  = 0x01,
    HOOK_POST_BUILD = 0x02,
    HOOK_PRE_SWAP   = 0x03,
    HOOK_POST_SWAP  = 0x04,
    HOOK_PRE_DRAIN  = 0x05,
    HOOK_POST_DRAIN = 0x06
} alrios_hook_stage_t;

typedef struct alrios_hook_inv {
    alrios_hook_stage_t stage;
    const char *app_id;
    const char *slot_path;
    uint32_t timeout_ms;
} alrios_hook_inv_t;

int alrios_hook_execute(const alrios_hook_inv_t *inv);

#endif /* ALRIOS_HOOKS_H */
