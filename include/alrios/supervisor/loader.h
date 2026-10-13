/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_SUPERVISOR_LOADER_H
#define ALRIOS_SUPERVISOR_LOADER_H

#include "alrios/supervisor/context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Directory accessors bound to supervisor context */
void ar_supervisor_get_apps_dir(const ar_supervisor_context_t *ctx, char *buf, int size);
void ar_supervisor_get_run_dir(const ar_supervisor_context_t *ctx, char *buf, int size);
void ar_supervisor_get_base_dir(const ar_supervisor_context_t *ctx, char *buf, int size);
void ar_supervisor_get_autostart_path(const ar_supervisor_context_t *ctx, char *buf, int size);

/* Temp directory management */
void ar_supervisor_set_temp_dir(ar_supervisor_context_t *ctx, const char *base_dir);
void ar_supervisor_cleanup_temp(ar_supervisor_context_t *ctx);
void *ar_supervisor_get_proc_group(const ar_supervisor_context_t *ctx);

/* Storage overlay */
void ar_supervisor_overlay_storage(
    ar_supervisor_context_t *ctx,
    const char *apps_dir,
    const char *app_name,
    const char *tmpdir);

/* App registry queries and lifecycle */
loader_app_t *ar_supervisor_find_app(ar_supervisor_context_t *ctx, const char *name);
loader_app_t *ar_supervisor_register_app(
    ar_supervisor_context_t *ctx,
    const ar_app_manifest_t *m,
    const char *app_dir,
    const char *package_path);
int ar_supervisor_update_app_process(
    ar_supervisor_context_t *ctx,
    const char *name,
    int new_pid,
    const char *new_dir);

int  ar_supervisor_start_app(ar_supervisor_context_t *ctx, const char *name);
int  ar_supervisor_stop_app(ar_supervisor_context_t *ctx, const char *name);
void ar_supervisor_stop_all(ar_supervisor_context_t *ctx);
int  ar_supervisor_restart_app(ar_supervisor_context_t *ctx, const char *name);
int  ar_supervisor_list_apps(ar_supervisor_context_t *ctx, char *out, int size);
int  ar_supervisor_status_app(ar_supervisor_context_t *ctx, const char *name, char *out, int size);
int  ar_supervisor_power_reload(ar_supervisor_context_t *ctx);
int  ar_supervisor_refresh(ar_supervisor_context_t *ctx);
void ar_supervisor_reap_apps(ar_supervisor_context_t *ctx);

/* Scanning */
void ar_supervisor_scan(ar_supervisor_context_t *ctx, const char *apps_dir);
void ar_supervisor_scan_phase(ar_supervisor_context_t *ctx, const char *apps_dir, int phase);

/* Autostart */
void ar_supervisor_load_autostart(ar_supervisor_context_t *ctx);
void ar_supervisor_spawn_autostart_priority(ar_supervisor_context_t *ctx);
int  ar_supervisor_autostart_add(ar_supervisor_context_t *ctx, const char *name);
int  ar_supervisor_autostart_del(ar_supervisor_context_t *ctx, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* ALRIOS_SUPERVISOR_LOADER_H */
