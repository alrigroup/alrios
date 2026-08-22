/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef AR_LOADER_H
#define AR_LOADER_H

void loader_get_apps_dir(char *buf, int size);
void loader_get_run_dir(char *buf, int size);
void loader_get_base_dir(char *buf, int size);
void loader_scan(const char *apps_dir);
void loader_scan_phase(const char *apps_dir, int phase);
void loader_set_temp_dir(const char *base_dir);
void loader_cleanup_temp(void);
void loader_overlay_storage(const char *apps_dir, const char *app_name, const char *tmpdir);
void *loader_get_proc_group(void);

int  loader_start_app(const char *name);
int  loader_stop_app(const char *name);
void loader_stop_all(void);
int  loader_restart_app(const char *name);
int  loader_list_apps(char *out, int size);
int  loader_status_app(const char *name, char *out, int size);
int  loader_power_reload(void);
int  loader_refresh(void);
void loader_reap_apps(void);

void loader_get_autostart_path(char *buf, int size);
void loader_load_autostart(void);
int  loader_autostart_add(const char *name);
int  loader_autostart_del(const char *name);

#endif
