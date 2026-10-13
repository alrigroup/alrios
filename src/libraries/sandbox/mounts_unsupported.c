/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/sandbox/mounts.h"

#include <string.h>

int alrios_mount_validate_app_id(const char *app_id) {
    size_t length = 0U;
    if (!app_id || app_id[0] == '\0') return 0;
    for (const char *p = app_id; *p != '\0'; ++p, ++length) {
        unsigned char c = (unsigned char)*p;
        if (length >= ALRIOS_APP_ID_MAX_LEN - 1U ||
            !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_')) {
            return 0;
        }
    }
    return 1;
}

int alrios_mount_validate_path_containment(const char *base_dir, const char *candidate_path) {
    (void)base_dir;
    (void)candidate_path;
    return ALRIOS_MOUNT_ERR_UNSHARE;
}

int alrios_mount_get_data_path(const char *base_storage_dir,
                               const char *app_id,
                               char *out_path,
                               size_t max_len) {
    (void)base_storage_dir;
    (void)app_id;
    (void)out_path;
    (void)max_len;
    return ALRIOS_MOUNT_ERR_UNSHARE;
}

int alrios_mount_setup_private_namespace(void) {
    return ALRIOS_MOUNT_ERR_UNSHARE;
}

int alrios_mount_setup_app_mounts(const char *app_id,
                                  const char *code_dir,
                                  const char *data_dir,
                                  const char *sandbox_root,
                                  alrios_app_mount_context_t *ctx) {
    (void)app_id;
    (void)code_dir;
    (void)data_dir;
    (void)sandbox_root;
    if (ctx) memset(ctx, 0, sizeof(*ctx));
    return ALRIOS_MOUNT_ERR_UNSHARE;
}

int alrios_mount_teardown_app_mounts(alrios_app_mount_context_t *ctx) {
    if (!ctx) return ALRIOS_MOUNT_ERR_INVALID_ARG;
    memset(ctx, 0, sizeof(*ctx));
    return ALRIOS_MOUNT_OK;
}

int alrios_mount_unmount_path(const char *target_path) {
    (void)target_path;
    return ALRIOS_MOUNT_ERR_UNSHARE;
}
