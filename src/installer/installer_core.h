/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef AR_INSTALLER_CORE_H
#define AR_INSTALLER_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define AR_PATH_SEP '\\'
#define AR_PATH_SEP_STR "\\"
#else
#define AR_PATH_SEP '/'
#define AR_PATH_SEP_STR "/"
#endif

typedef struct {
    char dest_dir[1024];
    char self_exe[1024];
    int  add_to_path;
    int  install_gcc;
    int  install_node;
    int  install_python;
    int  is_unattended;
    int  is_cli;
    int  dry_run;
} installer_config_t;

typedef struct {
    void (*on_log)(const char *message, void *user_data);
    void (*on_progress)(int step, int total_steps, int percent, const char *step_name, void *user_data);
    void (*on_complete)(int success, const char *summary, void *user_data);
} installer_callbacks_t;

/* System information and defaults */
void installer_get_default_paths(char *dest_out, size_t dest_size, int *is_privileged);
int  installer_is_privileged(void);
int  installer_has_embedded_payload(const char *self_exe);
int  installer_check_tool_exists(const char *tool_name);

/* Installation actions */
int  installer_run(const installer_config_t *cfg, const installer_callbacks_t *cb, void *user_data);
int  installer_extract_payload(const installer_config_t *cfg, const installer_callbacks_t *cb, void *user_data);
int  installer_register_path(const char *dest_dir, const installer_callbacks_t *cb, void *user_data);
int  installer_provision_gcc(const char *dest_dir, const installer_callbacks_t *cb, void *user_data);
int  installer_provision_runtime(const char *dest_dir, const char *rt_name, const installer_callbacks_t *cb, void *user_data);
void installer_cleanup(const char *dest_dir);

#endif /* AR_INSTALLER_CORE_H */
