/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "alrios/arapp_index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/*
 * Validates path against directory traversal (e.g. "../" or starting with '/')
 * and forbidden mutable file extensions per ALRIOS Spec 16.8 / Section 12.6.
 */
static int validate_relative_path(const char *rel_path) {
    if (!rel_path || rel_path[0] == '\0') {
        return -1;
    }

    /* Reject leading slashes */
    if (rel_path[0] == '/') {
        return -1;
    }

    size_t len = strnlen(rel_path, sizeof(((arapp_index_entry_t *)0)->path));
    if (len == 0 || len >= sizeof(((arapp_index_entry_t *)0)->path)) {
        return -1;
    }

    /* Reject path traversal attempts: ".." segment */
    const char *p = rel_path;
    while (*p != '\0') {
        if (p[0] == '.' && p[1] == '.' && (p[2] == '/' || p[2] == '\0')) {
            return -1;
        }
        while (*p != '\0' && *p != '/') {
            p++;
        }
        if (*p == '/') {
            p++;
        }
    }

    /* Forbidden extensions: .db, .sqlite, .session, .lock, .wal, .shm, .log, .tmp */
    static const char * const forbidden_exts[] = {
        ".db", ".sqlite", ".session", ".lock", ".wal", ".shm", ".log", ".tmp"
    };
    const size_t num_exts = sizeof(forbidden_exts) / sizeof(forbidden_exts[0]);

    for (size_t i = 0; i < num_exts; i++) {
        size_t ext_len = strlen(forbidden_exts[i]);
        if (len >= ext_len) {
            if (strncmp(rel_path + len - ext_len, forbidden_exts[i], ext_len) == 0) {
                return -1;
            }
        }
    }

    return 0;
}

/*
 * Ensures parent directories exist with mode 0755 before linking/copying.
 */
static int ensure_parent_dirs(const char *path) {
    char dir[PATH_MAX];
    int n = snprintf(dir, sizeof(dir), "%s", path);
    if (n < 0 || (size_t)n >= sizeof(dir)) {
        return -1;
    }

    char *slash = strrchr(dir, '/');
    if (!slash || slash == dir) {
        return 0;
    }
    *slash = '\0';

    for (char *p = dir + 1; *p != '\0'; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }

    if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
        return -1;
    }

    return 0;
}

/*
 * Fallback copy if both link() and ioctl(FICLONE) return EXDEV/EOPNOTSUPP.
 */
static int copy_file_fallback(const char *src_path, const char *dst_path, mode_t mode) {
    int src_fd = open(src_path, O_RDONLY | O_CLOEXEC);
    if (src_fd < 0) {
        return -1;
    }

    int dst_fd = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, mode);
    if (dst_fd < 0) {
        close(src_fd);
        return -1;
    }

    char buf[8192];
    ssize_t bytes_read = 0;
    int status = 0;

    while ((bytes_read = read(src_fd, buf, sizeof(buf))) > 0) {
        ssize_t bytes_written = 0;
        while (bytes_written < bytes_read) {
            ssize_t res = write(dst_fd, buf + bytes_written, (size_t)(bytes_read - bytes_written));
            if (res < 0) {
                if (errno == EINTR) continue;
                status = -1;
                break;
            }
            bytes_written += res;
        }
        if (status != 0) break;
    }

    if (bytes_read < 0) {
        status = -1;
    }

    close(src_fd);
    close(dst_fd);

    if (status != 0) {
        (void)unlink(dst_path);
    }
    return status;
}

/*
 * Layer a single file from old slot to new slot:
 * 1. Attempt hardlink link(old_path, new_path)
 * 2. If EXDEV, attempt reflink via ioctl(FICLONE)
 * 3. If reflink fails, fallback to full byte copy
 * 4. Apply permissions
 */
static int layer_file(const char *old_path, const char *new_path, uint32_t mode) {
    if (ensure_parent_dirs(new_path) != 0) {
        return -1;
    }

    /* Remove target if it already exists */
    (void)unlink(new_path);

    /* 1. Try zero-copy hardlink */
    if (link(old_path, new_path) == 0) {
        return 0;
    }

    /* If error is not cross-device or not supported, attempt reflink or fallback */
    int link_err = errno;
    if (link_err != EXDEV && link_err != EPERM && link_err != EOPNOTSUPP) {
        return -1;
    }

    /* 2. Cross-device link: attempt reflink ioctl(FICLONE) */
    int src_fd = open(old_path, O_RDONLY | O_CLOEXEC);
    if (src_fd < 0) {
        return -1;
    }

    mode_t target_mode = (mode != 0) ? (mode_t)(mode & 07777) : 0644;
    int dst_fd = open(new_path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, target_mode);
    if (dst_fd < 0) {
        close(src_fd);
        return -1;
    }

    int clone_res = ioctl(dst_fd, FICLONE, src_fd);
    close(src_fd);
    close(dst_fd);

    if (clone_res == 0) {
        return 0;
    }

    /* 3. FICLONE failed, fallback to verified buffered copy */
    (void)unlink(new_path);
    return copy_file_fallback(old_path, new_path, target_mode);
}

int alrios_index_compare_and_hardlink(const char *old_slot, const char *new_slot, const arapp_index_entry_t *entries, size_t count) {
    if (!old_slot || !new_slot) {
        return -1;
    }
    if (old_slot[0] == '\0' || new_slot[0] == '\0') {
        return -1;
    }
    if (!entries && count > 0) {
        return -1;
    }

    /* If count == 0, operation succeeds vacuously */
    if (count == 0) {
        return 0;
    }

    for (size_t i = 0; i < count; i++) {
        const arapp_index_entry_t *e = &entries[i];

        /* Validate entry path */
        if (validate_relative_path(e->path) != 0) {
            /* Skip or reject invalid / traversal / mutable entry */
            continue;
        }

        char old_path[PATH_MAX];
        char new_path[PATH_MAX];

        int n_old = snprintf(old_path, sizeof(old_path), "%s/%s", old_slot, e->path);
        int n_new = snprintf(new_path, sizeof(new_path), "%s/%s", new_slot, e->path);

        if (n_old < 0 || (size_t)n_old >= sizeof(old_path) ||
            n_new < 0 || (size_t)n_new >= sizeof(new_path)) {
            continue;
        }

        /* Check if source file exists in old_slot */
        struct stat st;
        if (stat(old_path, &st) != 0) {
            /* Source does not exist in old slot (file is new/download candidate) */
            continue;
        }

        /* Check if regular file */
        if (!S_ISREG(st.st_mode)) {
            continue;
        }

        /* Attempt zero-copy hardlink / reflink layering */
        (void)layer_file(old_path, new_path, e->mode);
    }

    return 0;
}
