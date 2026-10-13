/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_SANDBOX_MOUNTS_H
#define ALRIOS_SANDBOX_MOUNTS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALRIOS_MOUNT_OK                0
#define ALRIOS_MOUNT_ERR_INVALID_ARG  -5201
#define ALRIOS_MOUNT_ERR_APP_ID       -5202
#define ALRIOS_MOUNT_ERR_PATH_TRAVERSAL -5203
#define ALRIOS_MOUNT_ERR_NOT_FOUND    -5204
#define ALRIOS_MOUNT_ERR_MKDIR        -5205
#define ALRIOS_MOUNT_ERR_UNSHARE      -5206
#define ALRIOS_MOUNT_ERR_MOUNT        -5207
#define ALRIOS_MOUNT_ERR_UMOUNT       -5208
#define ALRIOS_MOUNT_ERR_SECURITY     -5209

#define ALRIOS_APP_ID_MAX_LEN         128U
#define ALRIOS_MOUNT_PATH_MAX         4096U

typedef struct alrios_mount_entry {
    char target_path[ALRIOS_MOUNT_PATH_MAX];
    uint32_t flags;
    uint8_t is_mounted;
} alrios_mount_entry_t;

typedef struct alrios_app_mount_context {
    char app_id[ALRIOS_APP_ID_MAX_LEN];
    char sandbox_root[ALRIOS_MOUNT_PATH_MAX];
    char code_source[ALRIOS_MOUNT_PATH_MAX];
    char code_target[ALRIOS_MOUNT_PATH_MAX];
    char data_source[ALRIOS_MOUNT_PATH_MAX];
    char data_target[ALRIOS_MOUNT_PATH_MAX];
    alrios_mount_entry_t entries[4];
    size_t entry_count;
    uint8_t active;
} alrios_app_mount_context_t;

/**
 * Validates an app_id for alphanumeric chars, dots, hyphens, and underscores.
 * Must not be empty, must not exceed ALRIOS_APP_ID_MAX_LEN - 1.
 * Returns 1 if valid, 0 if invalid.
 */
int alrios_mount_validate_app_id(const char *app_id);

/**
 * Validates that candidate_path does not contain traversal segments ("..")
 * and is strictly contained within base_dir.
 * Returns 0 if valid and contained, negative error code otherwise.
 */
int alrios_mount_validate_path_containment(const char *base_dir, const char *candidate_path);

/**
 * Resolves the persistent data host storage path for an app_id.
 * Formats: "<base_storage_dir>/<app_id>"
 * Validates app_id and containment.
 */
int alrios_mount_get_data_path(const char *base_storage_dir,
                               const char *app_id,
                               char *out_path,
                               size_t max_len);

/**
 * Creates an unshared mount namespace with private propagation on root.
 * Returns ALRIOS_MOUNT_OK on success, or negative error code.
 */
int alrios_mount_setup_private_namespace(void);

/**
 * Prepares and mounts per-app isolated filesystem mounts into a sandbox target:
 *  - Code directory: mounted strictly read-only (MS_RDONLY | MS_NOSUID | MS_NODEV | MS_BIND | MS_REC)
 *  - Persistent data directory: mounted read-write with strict isolation (MS_NOSUID | MS_NODEV)
 *
 * If ctx is non-NULL, tracks mounted paths for deterministic teardown.
 */
int alrios_mount_setup_app_mounts(const char *app_id,
                                  const char *code_dir,
                                  const char *data_dir,
                                  const char *sandbox_root,
                                  alrios_app_mount_context_t *ctx);

/**
 * Unmounts all mounts tracked in ctx in reverse order.
 * Uses umount2(target, 0) with MNT_DETACH fallback if busy.
 * Safe to call on exit or crash handler.
 */
int alrios_mount_teardown_app_mounts(alrios_app_mount_context_t *ctx);

/**
 * Detaches or unmounts a single mount path with MNT_DETACH fallback.
 */
int alrios_mount_unmount_path(const char *target_path);

#ifdef __cplusplus
}
#endif

#endif /* ALRIOS_SANDBOX_MOUNTS_H */
