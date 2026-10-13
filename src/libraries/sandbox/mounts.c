/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "alrios/sandbox/mounts.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef MS_REC
#define MS_REC 16384
#endif
#ifndef MS_PRIVATE
#define MS_PRIVATE (1 << 18)
#endif

#ifndef MOUNT_ATTR_RDONLY
#define MOUNT_ATTR_RDONLY 0x00000001ULL
#endif
#ifndef MOUNT_ATTR_NOSUID
#define MOUNT_ATTR_NOSUID 0x00000002ULL
#endif
#ifndef MOUNT_ATTR_NODEV
#define MOUNT_ATTR_NODEV  0x00000004ULL
#endif
#ifndef MOUNT_ATTR_NOEXEC
#define MOUNT_ATTR_NOEXEC 0x00000008ULL
#endif
#ifndef AT_RECURSIVE
#define AT_RECURSIVE 0x8000
#endif

struct alrios_mount_attr {
    uint64_t attr_set;
    uint64_t attr_clr;
    uint64_t propagation;
    uint64_t userns_fd;
};

static int alrios_apply_mount_setattr(const char *target, uint64_t attr_set) {
#if defined(SYS_mount_setattr)
    struct alrios_mount_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.attr_set = attr_set;
    if (syscall(SYS_mount_setattr, AT_FDCWD, target, (unsigned int)AT_RECURSIVE, &attr, sizeof(attr)) == 0) {
        return 0;
    }
#else
    (void)target;
    (void)attr_set;
#endif
    return -1;
}

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

int alrios_mount_validate_path_containment(const char *base_dir, const char *candidate_path) {
    if (!base_dir || !candidate_path || base_dir[0] == '\0' || candidate_path[0] == '\0') {
        return ALRIOS_MOUNT_ERR_INVALID_ARG;
    }

    /* Reject dot-dot components before or after canonicalization */
    if (strstr(candidate_path, "..") != NULL) {
        return ALRIOS_MOUNT_ERR_PATH_TRAVERSAL;
    }

    char resolved_base[PATH_MAX];
    char resolved_candidate[PATH_MAX];

    if (!realpath(base_dir, resolved_base)) {
        /* Base dir must exist or be resolvable */
        return ALRIOS_MOUNT_ERR_NOT_FOUND;
    }

    if (!realpath(candidate_path, resolved_candidate)) {
        /* If candidate does not exist yet, resolve parent directory */
        char parent_buf[PATH_MAX];
        size_t cand_len = strlen(candidate_path);
        if (cand_len >= sizeof(parent_buf)) {
            return ALRIOS_MOUNT_ERR_INVALID_ARG;
        }
        memcpy(parent_buf, candidate_path, cand_len + 1U);
        char *last_slash = strrchr(parent_buf, '/');
        if (!last_slash || last_slash == parent_buf) {
            return ALRIOS_MOUNT_ERR_PATH_TRAVERSAL;
        }
        *last_slash = '\0';
        char resolved_parent[PATH_MAX];
        if (!realpath(parent_buf, resolved_parent)) {
            return ALRIOS_MOUNT_ERR_NOT_FOUND;
        }
        int written = snprintf(resolved_candidate, sizeof(resolved_candidate), "%s/%s",
                               resolved_parent, candidate_path + (last_slash - parent_buf + 1));
        if (written < 0 || (size_t)written >= sizeof(resolved_candidate)) {
            return ALRIOS_MOUNT_ERR_PATH_TRAVERSAL;
        }
    }

    size_t base_len = strlen(resolved_base);
    if (resolved_base[base_len - 1U] == '/') {
        resolved_base[base_len - 1U] = '\0';
        --base_len;
    }

    size_t cand_len = strlen(resolved_candidate);
    if (cand_len < base_len) {
        return ALRIOS_MOUNT_ERR_PATH_TRAVERSAL;
    }
    if (strncmp(resolved_base, resolved_candidate, base_len) != 0) {
        return ALRIOS_MOUNT_ERR_PATH_TRAVERSAL;
    }
    if (resolved_candidate[base_len] != '\0' && resolved_candidate[base_len] != '/') {
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

    int n = snprintf(out_path, max_len, "%s/%s", base_storage_dir, app_id);
    if (n < 0 || (size_t)n >= max_len) {
        return ALRIOS_MOUNT_ERR_PATH_TRAVERSAL;
    }

    return ALRIOS_MOUNT_OK;
}

int alrios_mount_setup_private_namespace(void) {
    if (unshare(CLONE_NEWNS) != 0) {
        if ((errno == EPERM || errno == EACCES)) {
            /* Try unsharing user namespace first if unprivileged */
            if (unshare(CLONE_NEWUSER | CLONE_NEWNS) != 0) {
                return ALRIOS_MOUNT_ERR_UNSHARE;
            }
        } else {
            return ALRIOS_MOUNT_ERR_UNSHARE;
        }
    }

    /* Prevent mount propagation outside our private namespace */
    if (mount("none", "/", NULL, MS_REC | MS_PRIVATE, NULL) != 0) {
        /* Best-effort in some environments, but log/fail if critical */
        if (errno != EINVAL && errno != EACCES) {
            return ALRIOS_MOUNT_ERR_MOUNT;
        }
    }

    return ALRIOS_MOUNT_OK;
}

static int alrios_mkdir_p(const char *path, mode_t mode) {
    if (!path || path[0] == '\0') {
        return -1;
    }
    char tmp[PATH_MAX];
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) {
        return -1;
    }
    memcpy(tmp, path, len + 1U);

    for (char *p = tmp + 1; *p != '\0'; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

int alrios_mount_unmount_path(const char *target_path) {
    if (!target_path || target_path[0] == '\0') {
        return ALRIOS_MOUNT_ERR_INVALID_ARG;
    }

    if (umount2(target_path, 0) == 0) {
        return ALRIOS_MOUNT_OK;
    }

    if (errno == ENOENT || errno == EINVAL) {
        /* Not mounted or already gone */
        return ALRIOS_MOUNT_OK;
    }

    /* Fallback to MNT_DETACH for busy or lazy cleanup */
    if (umount2(target_path, MNT_DETACH) == 0) {
        return ALRIOS_MOUNT_OK;
    }

    return (errno == ENOENT || errno == EINVAL) ? ALRIOS_MOUNT_OK : ALRIOS_MOUNT_ERR_UMOUNT;
}

int alrios_mount_teardown_app_mounts(alrios_app_mount_context_t *ctx) {
    if (!ctx) {
        return ALRIOS_MOUNT_ERR_INVALID_ARG;
    }

    int overall_status = ALRIOS_MOUNT_OK;

    /* Unmount in reverse order */
    for (size_t i = ctx->entry_count; i > 0U; --i) {
        size_t idx = i - 1U;
        if (ctx->entries[idx].is_mounted) {
            int ret = alrios_mount_unmount_path(ctx->entries[idx].target_path);
            if (ret != ALRIOS_MOUNT_OK && overall_status == ALRIOS_MOUNT_OK) {
                overall_status = ret;
            }
            ctx->entries[idx].is_mounted = 0;
        }
    }

    ctx->entry_count = 0U;
    ctx->active = 0U;
    return overall_status;
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

    /* Target mount points within sandbox */
    char code_target[ALRIOS_MOUNT_PATH_MAX];
    char data_target[ALRIOS_MOUNT_PATH_MAX];

    int n = snprintf(code_target, sizeof(code_target), "%s/app", sandbox_root);
    if (n < 0 || (size_t)n >= sizeof(code_target)) {
        return ALRIOS_MOUNT_ERR_PATH_TRAVERSAL;
    }

    n = snprintf(data_target, sizeof(data_target), "%s/data/%s", sandbox_root, app_id);
    if (n < 0 || (size_t)n >= sizeof(data_target)) {
        return ALRIOS_MOUNT_ERR_PATH_TRAVERSAL;
    }

    if (ctx) {
        (void)snprintf(ctx->code_target, sizeof(ctx->code_target), "%s", code_target);
        (void)snprintf(ctx->data_target, sizeof(ctx->data_target), "%s", data_target);
    }

    /* Ensure source dirs exist */
    if (alrios_mkdir_p(code_dir, 0755) != 0 || alrios_mkdir_p(data_dir, 0700) != 0) {
        return ALRIOS_MOUNT_ERR_MKDIR;
    }

    /* Ensure sandbox targets exist */
    if (alrios_mkdir_p(code_target, 0755) != 0 || alrios_mkdir_p(data_target, 0700) != 0) {
        return ALRIOS_MOUNT_ERR_MKDIR;
    }

    /* 1. Mount Code directory: strictly MS_RDONLY | MS_NOSUID | MS_NODEV | MS_BIND | MS_REC */
    if (mount(code_dir, code_target, "none", MS_BIND | MS_REC, NULL) != 0) {
        return ALRIOS_MOUNT_ERR_MOUNT;
    }

    if (ctx && ctx->entry_count < 4U) {
        (void)snprintf(ctx->entries[ctx->entry_count].target_path,
                       sizeof(ctx->entries[ctx->entry_count].target_path),
                       "%s", code_target);
        ctx->entries[ctx->entry_count].flags = MS_RDONLY | MS_NOSUID | MS_NODEV | MS_BIND | MS_REC;
        ctx->entries[ctx->entry_count].is_mounted = 1;
        ctx->entry_count++;
    }

    /* Remount read-only */
    unsigned long ro_flags = MS_BIND | MS_REMOUNT | MS_RDONLY | MS_NOSUID | MS_NODEV | MS_REC;
    if (mount(code_target, code_target, "none", ro_flags, NULL) != 0) {
        if (mount(NULL, code_target, NULL, ro_flags, NULL) != 0) {
            /* Try mount_setattr fallback */
            if (alrios_apply_mount_setattr(code_target, MOUNT_ATTR_RDONLY | MOUNT_ATTR_NOSUID | MOUNT_ATTR_NODEV) != 0) {
                if (ctx) {
                    (void)alrios_mount_teardown_app_mounts(ctx);
                } else {
                    (void)alrios_mount_unmount_path(code_target);
                }
                return ALRIOS_MOUNT_ERR_MOUNT;
            }
        }
    }

    /* 2. Mount Data directory: MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_BIND */
    if (mount(data_dir, data_target, "none", MS_BIND | MS_REC, NULL) != 0) {
        if (ctx) {
            (void)alrios_mount_teardown_app_mounts(ctx);
        } else {
            (void)alrios_mount_unmount_path(code_target);
        }
        return ALRIOS_MOUNT_ERR_MOUNT;
    }

    if (ctx && ctx->entry_count < 4U) {
        (void)snprintf(ctx->entries[ctx->entry_count].target_path,
                       sizeof(ctx->entries[ctx->entry_count].target_path),
                       "%s", data_target);
        ctx->entries[ctx->entry_count].flags = MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_BIND;
        ctx->entries[ctx->entry_count].is_mounted = 1;
        ctx->entry_count++;
    }

    /* Remount data with MS_NOSUID | MS_NODEV | MS_NOEXEC */
    unsigned long data_flags = MS_BIND | MS_REMOUNT | MS_NOSUID | MS_NODEV | MS_NOEXEC;
    if (mount(data_target, data_target, "none", data_flags, NULL) != 0) {
        if (mount(NULL, data_target, NULL, data_flags, NULL) != 0) {
            if (alrios_apply_mount_setattr(data_target, MOUNT_ATTR_NOSUID | MOUNT_ATTR_NODEV | MOUNT_ATTR_NOEXEC) != 0) {
                if (ctx) {
                    (void)alrios_mount_teardown_app_mounts(ctx);
                } else {
                    (void)alrios_mount_unmount_path(data_target);
                    (void)alrios_mount_unmount_path(code_target);
                }
                return ALRIOS_MOUNT_ERR_MOUNT;
            }
        }
    }

    if (ctx) {
        ctx->active = 1U;
    }

    return ALRIOS_MOUNT_OK;
}
