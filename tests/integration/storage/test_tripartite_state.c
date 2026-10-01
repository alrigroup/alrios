/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/storage/bindings.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

int main(void) {
    char code_tmp[] = "/tmp/alrios_test_code_XXXXXX";
    int code_fd = mkstemp(code_tmp);
    assert(code_fd >= 0);
    assert(write(code_fd, "IMMUTABLE_CODE_BINARY", strlen("IMMUTABLE_CODE_BINARY")) == (ssize_t)strlen("IMMUTABLE_CODE_BINARY"));
    close(code_fd);

    assert(supervisor_verify_code_readonly(code_tmp) == 0);
    assert(open(code_tmp, O_WRONLY) < 0);
    (void)printf("[PASS] MP-012: Code mount read-only verified successfully\n");

    char data_tmp[] = "/tmp/alrios_test_data_XXXXXX";
    assert(mkdtemp(data_tmp) != NULL);

    assert(supervisor_verify_data_persistence("test_app", data_tmp) == 0);

    char persisted_file[1024];
    snprintf(persisted_file, sizeof(persisted_file), "%s/test_app/persistence.test", data_tmp);
    int pfd = open(persisted_file, O_RDONLY);
    assert(pfd >= 0);
    char pbuf[64];
    ssize_t pr = read(pfd, pbuf, sizeof(pbuf) - 1);
    close(pfd);
    assert(pr > 0);
    pbuf[pr] = '\0';
    assert(strstr(pbuf, "PERSISTENT_DATA_SURVIVES_UPGRADE") != NULL);

    (void)printf("[PASS] MP-012: Data survives upgrades verified successfully\n");

    char arapp_tmp[] = "/tmp/alrios_test_arapp_XXXXXX";
    int arapp_fd = mkstemp(arapp_tmp);
    assert(arapp_fd >= 0);
    assert(write(arapp_fd, "{\"app_id\":\"test\",\"version\":\"1.0.0\"}", strlen("{\"app_id\":\"test\",\"version\":\"1.0.0\"}")) == (ssize_t)strlen("{\"app_id\":\"test\",\"version\":\"1.0.0\"}"));
    close(arapp_fd);

    assert(supervisor_verify_config_separation(arapp_tmp) == 0);

    unlink(code_tmp);
    unlink(persisted_file);
    char dir_path[1024];
    snprintf(dir_path, sizeof(dir_path), "%s/test_app", data_tmp);
    rmdir(dir_path);
    rmdir(data_tmp);
    unlink(arapp_tmp);

    (void)printf("[PASS] MP-012: Config not copied to app package verified successfully\n");
    return 0;
}
