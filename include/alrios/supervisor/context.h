/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_SUPERVISOR_CONTEXT_H
#define ALRIOS_SUPERVISOR_CONTEXT_H

#include "alrios/supervisor/policy.h"
#include "alrios/supervisor/guest_router.h"
#include "arapp_parser.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AR_MAX_APPS 64
#define AR_MAX_DEPLOY_SLOTS 2
#define AR_DEFAULT_DRAIN_TIMEOUT_MS 2000
#define AR_MAX_MANAGED_HOTAPPS 64
#define AR_SUPERVISOR_MAX_TEMP_DIRS 4

typedef enum {
    APP_STOPPED = 0,
    APP_RUNNING,
    APP_CRASHED
} loader_app_state_t;

typedef struct loader_app {
    char name[AR_APP_NAME_MAX];
    char dir[1024];
    char package_path[1024];
    ar_app_manifest_t m;
    int pid;
    loader_app_state_t state;
    int is_native_service;
} loader_app_t;

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

typedef struct hotreload_slot {
    int                     slot_id;             /* 0 = Slot Alpha, 1 = Slot Beta */
    int                     pid;
    uint16_t                port;
    hotreload_slot_state_t  state;
    char                    staging_dir[1024];
    char                    bin_path[1024];
} hotreload_slot_t;

typedef struct hotreload_app_context {
    char             app_name[64];
    int              active_slot;                /* 0 or 1 */
    hotreload_slot_t slots[AR_MAX_DEPLOY_SLOTS];
} hotreload_app_context_t;

typedef struct ar_supervisor_config {
    char base_dir[1024];
    char apps_dir[1024];
    char run_dir[1024];
    alrios_profile_t profile;
    char trust_dir[1024];
    bool enable_router;
} ar_supervisor_config_t;

typedef struct ar_supervisor_context ar_supervisor_context_t;
typedef struct ar_supervisor_context ar_supervisor_t;

struct ar_supervisor_context {
    /* Synchronization primitives */
    void *app_mutex;
    void *hotreload_mutex;

    /* Process isolation and tracking */
    void *proc_group;

    /* Application Registry */
    loader_app_t apps[AR_MAX_APPS];
    int app_count;

    /* Autostart registration */
    char autostart_apps[AR_MAX_APPS][AR_APP_NAME_MAX];
    int autostart_count;

    /* Scan and refresh state */
    int refresh_scan;
    int force_extract;

    /* Filesystem layout */
    char base_dir[1024];
    char apps_dir[1024];
    char run_dir[1024];
    char temp_dirs[AR_SUPERVISOR_MAX_TEMP_DIRS][1024];
    int temp_dir_count;

    /* Supervisor execution policy */
    alrios_supervisor_policy_t *policy;

    /* Hot Reload slots and application states */
    hotreload_app_context_t hotapps[AR_MAX_MANAGED_HOTAPPS];
    int hotapp_count;

    /* Guest router and routing table */
    ar_guest_router_t *router;

    /* Background worker tracking */
    void *worker_thread;
    volatile int running;
};

/* Explicit Context Lifecycle */
ar_supervisor_context_t *ar_supervisor_context_create(void);
int ar_supervisor_context_create_with_config(
    const ar_supervisor_config_t *config,
    ar_supervisor_context_t **out_ctx);
void ar_supervisor_context_destroy(ar_supervisor_context_t *ctx);

/* Routing operations bound to context */
ar_guest_router_t *ar_supervisor_get_router(ar_supervisor_context_t *ctx);
int ar_supervisor_init_router(
    ar_supervisor_context_t *ctx,
    const ar_guest_router_config_t *config);

/* Policy operations bound to context */
int ar_supervisor_policy_initialize(
    ar_supervisor_context_t *ctx,
    alrios_profile_t profile,
    const char *trust_directory);
void ar_supervisor_policy_shutdown(ar_supervisor_context_t *ctx);
int ar_supervisor_policy_get_profile(
    const ar_supervisor_context_t *ctx,
    alrios_profile_t *out_profile);
int ar_supervisor_policy_set_profile(
    ar_supervisor_context_t *ctx,
    alrios_profile_t profile);
int ar_supervisor_policy_evaluate_package(
    const ar_supervisor_context_t *ctx,
    const char *package_path,
    char *reason_buf,
    size_t reason_len);

/* Hot reload operations bound to context */
int ar_supervisor_hotreload_init(ar_supervisor_context_t *ctx);
int ar_supervisor_hotreload_deploy(
    ar_supervisor_context_t *ctx,
    const char *app_name,
    const char *arapp_path,
    char *out_log,
    size_t out_log_size);
int ar_supervisor_hotreload_status(
    ar_supervisor_context_t *ctx,
    const char *app_name,
    char *out_buf,
    size_t out_buf_size);

#ifdef __cplusplus
}
#endif

#endif /* ALRIOS_SUPERVISOR_CONTEXT_H */
