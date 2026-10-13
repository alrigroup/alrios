/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */

#ifndef ALRI_HOTRELOAD_H
#define ALRI_HOTRELOAD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define AR_MAX_DEPLOY_SLOTS 2
#define AR_DEFAULT_DRAIN_TIMEOUT_MS 2000
#define AR_MAX_MANAGED_HOTAPPS 64

typedef enum {
    HOTRELOAD_IDLE = 0,
    HOTRELOAD_STAGING,
    HOTRELOAD_BOOTING,
    HOTRELOAD_PROBING,
    HOTRELOAD_ACTIVE,
    HOTRELOAD_DRAINING,
    HOTRELOAD_TERMINATED,
    HOTRELOAD_ABORTED
} hotreload_slot_state_t;

typedef struct {
    int                     slot_id;             /* 0 = Slot Alpha, 1 = Slot Beta */
    int                     pid;
    uint16_t                port;
    hotreload_slot_state_t  state;
    char                    staging_dir[1024];
    char                    bin_path[1024];
} hotreload_slot_t;

typedef struct {
    char             app_name[64];
    int              active_slot;                /* 0 or 1 */
    hotreload_slot_t slots[AR_MAX_DEPLOY_SLOTS];
} hotreload_app_context_t;

int ar_hotreload_init(void);
int ar_hotreload_deploy(const char *app_name, const char *arapp_path, char *out_log, size_t out_log_size);
int ar_hotreload_status(const char *app_name, char *out_buf, size_t out_buf_size);

#endif /* ALRI_HOTRELOAD_H */
