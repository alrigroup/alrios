/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */
#ifndef AR_LOADER_H
#define AR_LOADER_H

#include "arapp_parser.h"

#define AR_MAX_APPS 64

typedef enum {
    APP_STOPPED,
    APP_RUNNING,
    APP_CRASHED
} loader_app_state_t;

typedef struct {
    char name[AR_APP_NAME_MAX];
    char dir[1024];
    ar_app_manifest_t m;
    int pid;
    loader_app_state_t state;
    int is_native_service;
} loader_app_t;

typedef struct ar_supervisor {
    loader_app_t apps[AR_MAX_APPS];
    int app_count;
    void *app_mutex;
    void *proc_group;
    int refresh_scan;
    char autostart_apps[AR_MAX_APPS][AR_APP_NAME_MAX];
    int autostart_count;
} ar_supervisor_t;

ar_supervisor_t *loader_get_supervisor_context(void);

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
int  loader_update_app_process(const char *name, int new_pid, const char *new_dir);
int  loader_power_reload(void);
int  loader_refresh(void);
void loader_reap_apps(void);

void loader_get_autostart_path(char *buf, int size);
void loader_load_autostart(void);
void loader_spawn_autostart_priority(void);
int  loader_autostart_add(const char *name);
int  loader_autostart_del(const char *name);

#endif
