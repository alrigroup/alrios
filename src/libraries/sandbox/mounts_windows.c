/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/sandbox/mounts.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

int alrios_mount_validate_app_id(const char *app_id) {
    if (!app_id || app_id[0] == '\0') {
        return 0;
    }
    size_t len = 0U;
    for (const char *p = app_id; *p != '\0'; ++p, ++len) {
        if (len >= (ALRIOS_APP_ID_MAX_LEN - 1U)) {
            return 0;
        }
        const unsigned char c = (unsigned char)*p;
        if (!((c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') ||
              c == '.' || c == '-' || c == '_')) {
            return 0;
        }
    }
    return (len > 0U) ? 1 : 0;
}

static void normalize_separators(char *path) {
    if (!path) return;
    for (char *p = path; *p != '\0'; ++p) {
        if (*p == '/') {
            *p = '\\';
        }
    }
}

int alrios_mount_validate_path_containment(const char *base_dir, const char *candidate_path) {
    if (!base_dir || !candidate_path || base_dir[0] == '\0' || candidate_path[0] == '\0') {
        return ALRIOS_MOUNT_ERR_INVALID_ARG;
    }

    /* Reject dot-dot components upfront */
    if (strstr(candidate_path, "..") != NULL) {
        return ALRIOS_MOUNT_ERR_PATH_TRAVERSAL;
    }

    char base_copy[MAX_PATH];
    char cand_copy[MAX_PATH];

    if (strlen(base_dir) >= MAX_PATH || strlen(candidate_path) >= MAX_PATH) {
        return ALRIOS_MOUNT_ERR_INVALID_ARG;
    }

    memcpy(base_copy, base_dir, strlen(base_dir) + 1U);
    memcpy(cand_copy, candidate_path, strlen(candidate_path) + 1U);
    normalize_separators(base_copy);
    normalize_separators(cand_copy);

    char full_base[MAX_PATH];
    char full_cand[MAX_PATH];

    DWORD r_base = GetFullPathNameA(base_copy, MAX_PATH, full_base, NULL);
    if (r_base == 0 || r_base >= MAX_PATH) {
        return ALRIOS_MOUNT_ERR_NOT_FOUND;
    }

    DWORD r_cand = GetFullPathNameA(cand_copy, MAX_PATH, full_cand, NULL);
    if (r_cand == 0 || r_cand >= MAX_PATH) {
        return ALRIOS_MOUNT_ERR_PATH_TRAVERSAL;
    }

    size_t base_len = strlen(full_base);
    if (base_len > 0 && full_base[base_len - 1U] == '\\') {
        full_base[base_len - 1U] = '\0';
        --base_len;
    }

    size_t cand_len = strlen(full_cand);
    if (cand_len < base_len) {
        return ALRIOS_MOUNT_ERR_PATH_TRAVERSAL;
    }

    if (_strnicmp(full_base, full_cand, base_len) != 0) {
        return ALRIOS_MOUNT_ERR_PATH_TRAVERSAL;
    }

    if (full_cand[base_len] != '\0' && full_cand[base_len] != '\\') {
        return ALRIOS_MOUNT_ERR_PATH_TRAVERSAL;
    }

    return ALRIOS_MOUNT_OK;
}

int alrios_mount_get_data_path(const char *base_storage_dir,
                               const char *app_id,
                               char *out_path,
                               size_t max_len) {
    if (!base_storage_dir || !app_id || !out_path || max_len == 0U) {
        return ALRIOS_MOUNT_ERR_INVALID_ARG;
    }

    if (!alrios_mount_validate_app_id(app_id)) {
        return ALRIOS_MOUNT_ERR_APP_ID;
    }

    int n = snprintf(out_path, max_len, "%s\\%s", base_storage_dir, app_id);
    if (n < 0 || (size_t)n >= max_len) {
        return ALRIOS_MOUNT_ERR_PATH_TRAVERSAL;
    }
    normalize_separators(out_path);

    return ALRIOS_MOUNT_OK;
}

int alrios_mount_setup_private_namespace(void) {
    /* On Windows, namespace isolation is provided per-process via Job Objects */
    return ALRIOS_MOUNT_OK;
}

static int win_mkdir_p(const char *path) {
    if (!path || path[0] == '\0') {
        return -1;
    }
    char tmp[MAX_PATH];
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) {
        return -1;
    }
    memcpy(tmp, path, len + 1U);
    normalize_separators(tmp);

    for (char *p = tmp + 1; *p != '\0'; ++p) {
        if (*p == '\\') {
            *p = '\0';
            if (strlen(tmp) > 2 && tmp[strlen(tmp) - 1] != ':') {
                CreateDirectoryA(tmp, NULL);
            }
            *p = '\\';
        }
    }
    CreateDirectoryA(tmp, NULL);
    return 0;
}

int alrios_mount_unmount_path(const char *target_path) {
    (void)target_path;
    return ALRIOS_MOUNT_OK;
}

int alrios_mount_teardown_app_mounts(alrios_app_mount_context_t *ctx) {
    if (!ctx) {
        return ALRIOS_MOUNT_ERR_INVALID_ARG;
    }
    memset(ctx, 0, sizeof(*ctx));
    return ALRIOS_MOUNT_OK;
}

int alrios_mount_setup_app_mounts(const char *app_id,
                                  const char *code_dir,
                                  const char *data_dir,
                                  const char *sandbox_root,
                                  alrios_app_mount_context_t *ctx) {
    if (!app_id || !code_dir || !data_dir || !sandbox_root) {
        return ALRIOS_MOUNT_ERR_INVALID_ARG;
    }

    if (!alrios_mount_validate_app_id(app_id)) {
        return ALRIOS_MOUNT_ERR_APP_ID;
    }

    if (ctx) {
        memset(ctx, 0, sizeof(*ctx));
        (void)snprintf(ctx->app_id, sizeof(ctx->app_id), "%s", app_id);
        (void)snprintf(ctx->sandbox_root, sizeof(ctx->sandbox_root), "%s", sandbox_root);
        (void)snprintf(ctx->code_source, sizeof(ctx->code_source), "%s", code_dir);
        (void)snprintf(ctx->data_source, sizeof(ctx->data_source), "%s", data_dir);
    }

    char code_target[ALRIOS_MOUNT_PATH_MAX];
    char data_target[ALRIOS_MOUNT_PATH_MAX];

    int n = snprintf(code_target, sizeof(code_target), "%s\\app", sandbox_root);
    if (n < 0 || (size_t)n >= sizeof(code_target)) {
        return ALRIOS_MOUNT_ERR_PATH_TRAVERSAL;
    }

    n = snprintf(data_target, sizeof(data_target), "%s\\data\\%s", sandbox_root, app_id);
    if (n < 0 || (size_t)n >= sizeof(data_target)) {
        return ALRIOS_MOUNT_ERR_PATH_TRAVERSAL;
    }

    normalize_separators(code_target);
    normalize_separators(data_target);

    if (ctx) {
        (void)snprintf(ctx->code_target, sizeof(ctx->code_target), "%s", code_target);
        (void)snprintf(ctx->data_target, sizeof(ctx->data_target), "%s", data_target);
    }

    win_mkdir_p(code_dir);
    win_mkdir_p(data_dir);
    win_mkdir_p(code_target);
    win_mkdir_p(data_target);

    if (ctx) {
        ctx->active = 1U;
        ctx->entry_count = 2U;
    }

    return ALRIOS_MOUNT_OK;
}
