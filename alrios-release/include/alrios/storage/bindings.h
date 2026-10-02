/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_STORAGE_BINDINGS_H
#define ALRIOS_STORAGE_BINDINGS_H

#include <stddef.h>
#include <sys/types.h>

int alrios_storage_mount_code_readonly(const char *path);
int alrios_storage_bind_persistent_data(const char *app_id, const char *base_data_dir, char *out_mount_path, size_t max_len);
int alrios_storage_verify_config_isolated(const char *arapp_path, const char *config_key);

int supervisor_mounts_init(void);
int supervisor_setup_app_mounts(const char *app_id, const char *code_path, const char *persistent_base);
int supervisor_verify_code_readonly(const char *code_path);
int supervisor_verify_data_persistence(const char *app_id, const char *persistent_base);
int supervisor_verify_config_separation(const char *arapp_path);

#endif /* ALRIOS_STORAGE_BINDINGS_H */
