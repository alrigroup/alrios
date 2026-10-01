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

#include "alrios/storage/bindings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

int alrios_storage_mount_code_readonly(const char *path) {
    if (!path) {
        return -1;
    }
    struct stat st;
    if (stat(path, &st) != 0) {
        return -2;
    }
    if (chmod(path, st.st_mode & ~(S_IWUSR | S_IWGRP | S_IWOTH)) != 0) {
        return -3;
    }
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return -4;
    }
    char test_buf = 0;
    ssize_t w = write(fd, &test_buf, 1);
    if (w >= 0) {
        close(fd);
        return -5;
    }
    close(fd);
    return 0;
}

int alrios_storage_bind_persistent_data(const char *app_id, const char *base_data_dir, char *out_mount_path, size_t max_len) {
    if (!app_id || !base_data_dir || !out_mount_path || max_len == 0) {
        return -1;
    }
    int ret = snprintf(out_mount_path, max_len, "%s/%s", base_data_dir, app_id);
    if (ret < 0 || (size_t)ret >= max_len) {
        return -2;
    }
    struct stat st;
    if (stat(out_mount_path, &st) != 0) {
        if (mkdir(out_mount_path, 0755) != 0 && errno != EEXIST) {
            return -3;
        }
    }
    return 0;
}

int alrios_storage_verify_config_isolated(const char *arapp_path, const char *config_key) {
    if (!arapp_path || !config_key) {
        return -1;
    }
    int fd = open(arapp_path, O_RDONLY);
    if (fd < 0) {
        return -2;
    }
    off_t size = lseek(fd, 0, SEEK_END);
    if (size < 0) {
        close(fd);
        return -3;
    }
    if (lseek(fd, 0, SEEK_SET) < 0) {
        close(fd);
        return -4;
    }
    size_t len = (size_t)size;
    char *buf = (char *)malloc(len > 0 ? len : 1);
    if (!buf) {
        close(fd);
        return -5;
    }
    ssize_t r = read(fd, buf, len);
    close(fd);
    if (r < 0 || (size_t)r != len) {
        free(buf);
        return -6;
    }
    void *found = memmem(buf, len, config_key, strlen(config_key));
    free(buf);
    if (found != NULL) {
        return -7;
    }
    return 0;
}
