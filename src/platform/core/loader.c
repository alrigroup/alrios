/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */
#include "loader.h"
#include "alrios/supervisor/context.h"
#include "alrios/supervisor/loader.h"

#include <stddef.h>

static ar_supervisor_context_t *g_default_supervisor = NULL;

ar_supervisor_context_t *loader_get_supervisor_context(void) {
    if (!g_default_supervisor) {
        g_default_supervisor = ar_supervisor_context_create();
    }
    return g_default_supervisor;
}

void loader_get_apps_dir(char *buf, int size) {
    ar_supervisor_get_apps_dir(loader_get_supervisor_context(), buf, size);
}

void loader_get_run_dir(char *buf, int size) {
    ar_supervisor_get_run_dir(loader_get_supervisor_context(), buf, size);
}

void loader_get_base_dir(char *buf, int size) {
    ar_supervisor_get_base_dir(loader_get_supervisor_context(), buf, size);
}

void loader_get_autostart_path(char *buf, int size) {
    ar_supervisor_get_autostart_path(loader_get_supervisor_context(), buf, size);
}

void loader_scan(const char *apps_dir) {
    ar_supervisor_scan(loader_get_supervisor_context(), apps_dir);
}

void loader_scan_phase(const char *apps_dir, int phase) {
    ar_supervisor_scan_phase(loader_get_supervisor_context(), apps_dir, phase);
}

void loader_set_temp_dir(const char *base_dir) {
    ar_supervisor_set_temp_dir(loader_get_supervisor_context(), base_dir);
}

void loader_cleanup_temp(void) {
    ar_supervisor_cleanup_temp(loader_get_supervisor_context());
}

void loader_overlay_storage(const char *apps_dir, const char *app_name, const char *tmpdir) {
    ar_supervisor_overlay_storage(loader_get_supervisor_context(), apps_dir, app_name, tmpdir);
}

void *loader_get_proc_group(void) {
    return ar_supervisor_get_proc_group(loader_get_supervisor_context());
}

loader_app_t *loader_find_app(const char *name) {
    return ar_supervisor_find_app(loader_get_supervisor_context(), name);
}

int loader_update_app_process(const char *name, int new_pid, const char *new_dir) {
    return ar_supervisor_update_app_process(loader_get_supervisor_context(), name, new_pid, new_dir);
}

int loader_start_app(const char *name) {
    return ar_supervisor_start_app(loader_get_supervisor_context(), name);
}

int loader_stop_app(const char *name) {
    return ar_supervisor_stop_app(loader_get_supervisor_context(), name);
}

void loader_stop_all(void) {
    ar_supervisor_stop_all(loader_get_supervisor_context());
}

int loader_restart_app(const char *name) {
    return ar_supervisor_restart_app(loader_get_supervisor_context(), name);
}

int loader_list_apps(char *out, int size) {
    return ar_supervisor_list_apps(loader_get_supervisor_context(), out, size);
}

int loader_status_app(const char *name, char *out, int size) {
    return ar_supervisor_status_app(loader_get_supervisor_context(), name, out, size);
}

int loader_power_reload(void) {
    return ar_supervisor_power_reload(loader_get_supervisor_context());
}

int loader_refresh(void) {
    return ar_supervisor_refresh(loader_get_supervisor_context());
}

void loader_reap_apps(void) {
    ar_supervisor_reap_apps(loader_get_supervisor_context());
}

void loader_load_autostart(void) {
    ar_supervisor_load_autostart(loader_get_supervisor_context());
}

void loader_spawn_autostart_priority(void) {
    ar_supervisor_spawn_autostart_priority(loader_get_supervisor_context());
}

int loader_autostart_add(const char *name) {
    return ar_supervisor_autostart_add(loader_get_supervisor_context(), name);
}

int loader_autostart_del(const char *name) {
    return ar_supervisor_autostart_del(loader_get_supervisor_context(), name);
}

int loader_policy_initialize(alrios_profile_t profile, const char *trust_directory) {
    return ar_supervisor_policy_initialize(loader_get_supervisor_context(), profile, trust_directory);
}

void loader_policy_shutdown(void) {
    ar_supervisor_policy_shutdown(loader_get_supervisor_context());
}

int loader_policy_get_profile(alrios_profile_t *out_profile) {
    return ar_supervisor_policy_get_profile(loader_get_supervisor_context(), out_profile);
}

int loader_policy_set_profile(alrios_profile_t profile) {
    return ar_supervisor_policy_set_profile(loader_get_supervisor_context(), profile);
}

int loader_policy_evaluate_package(const char *package_path,
                                   char *reason_buf,
                                   size_t reason_len) {
    return ar_supervisor_policy_evaluate_package(
        loader_get_supervisor_context(), package_path, reason_buf, reason_len);
}
