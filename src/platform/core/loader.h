/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef AR_LOADER_H
#define AR_LOADER_H

#include "arapp_parser.h"
#include "alrios/supervisor/policy.h"
#include "alrios/supervisor/context.h"
#include "alrios/supervisor/loader.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

ar_supervisor_context_t *loader_get_supervisor_context(void);

void loader_get_apps_dir(char *buf, int size);
void loader_get_run_dir(char *buf, int size);
void loader_get_base_dir(char *buf, int size);
void loader_get_autostart_path(char *buf, int size);

void loader_scan(const char *apps_dir);
void loader_scan_phase(const char *apps_dir, int phase);
void loader_set_temp_dir(const char *base_dir);
void loader_cleanup_temp(void);
void loader_overlay_storage(const char *apps_dir, const char *app_name, const char *tmpdir);
void *loader_get_proc_group(void);

loader_app_t *loader_find_app(const char *name);
int  loader_update_app_process(const char *name, int new_pid, const char *new_dir);

int  loader_start_app(const char *name);
int  loader_stop_app(const char *name);
void loader_stop_all(void);
int  loader_restart_app(const char *name);
int  loader_list_apps(char *out, int size);
int  loader_status_app(const char *name, char *out, int size);
int  loader_power_reload(void);
int  loader_refresh(void);
void loader_reap_apps(void);

void loader_load_autostart(void);
void loader_spawn_autostart_priority(void);
int  loader_autostart_add(const char *name);
int  loader_autostart_del(const char *name);

int  loader_policy_initialize(alrios_profile_t profile, const char *trust_directory);
void loader_policy_shutdown(void);
int  loader_policy_get_profile(alrios_profile_t *out_profile);
int  loader_policy_set_profile(alrios_profile_t profile);
int  loader_policy_evaluate_package(const char *package_path,
                                    char *reason_buf,
                                    size_t reason_len);

#ifdef __cplusplus
}
#endif

#endif /* AR_LOADER_H */
