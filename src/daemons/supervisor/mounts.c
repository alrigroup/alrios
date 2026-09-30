/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/storage/bindings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int supervisor_mounts_init(void) {
    return 0;
}

int supervisor_setup_app_mounts(const char *app_id, const char *code_path, const char *persistent_base) {
    if (!app_id || !code_path || !persistent_base) {
        return -1;
    }
    if (alrios_storage_mount_code_readonly(code_path) != 0) {
        return -2;
    }
    char data_path[1024];
    if (alrios_storage_bind_persistent_data(app_id, persistent_base, data_path, sizeof(data_path)) != 0) {
        return -3;
    }
    return 0;
}

int supervisor_verify_code_readonly(const char *code_path) {
    return alrios_storage_mount_code_readonly(code_path);
}

int supervisor_verify_data_persistence(const char *app_id, const char *persistent_base) {
    char data_path[1024];
    if (alrios_storage_bind_persistent_data(app_id, persistent_base, data_path, sizeof(data_path)) != 0) {
        return -1;
    }
    char test_file[1152];
    int ret = snprintf(test_file, sizeof(test_file), "%s/persistence.test", data_path);
    if (ret < 0 || (size_t)ret >= sizeof(test_file)) {
        return -2;
    }
    int fd = open(test_file, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        return -3;
    }
    const char *data = "PERSISTENT_DATA_SURVIVES_UPGRADE";
    ssize_t w = write(fd, data, strlen(data));
    close(fd);
    if (w < 0) {
        return -4;
    }
    return 0;
}

int supervisor_verify_config_separation(const char *arapp_path) {
    return alrios_storage_verify_config_isolated(arapp_path, "CONFIG_SECRET_VAULT_KEY");
}
