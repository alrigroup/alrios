/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/package/index.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

extern int alrios_armake_write_index(const char *source_dir, const char *output_index_path);

int main(void) {
    const char *test_dir = "/tmp/alrios_test_index_dir";
    const char *index_file = "/tmp/alrios_test.index";
    int command_status = system("rm -rf /tmp/alrios_test_index_dir && mkdir -p /tmp/alrios_test_index_dir");
    if (command_status != 0) {
        return 1;
    }
    unlink(index_file);

    // 1. Acceptance Criterion 1: SHA-256 per file computed
    FILE *f1 = fopen("/tmp/alrios_test_index_dir/binary.bin", "wb");
    assert(f1 != NULL);
    const char data[] = "SOVEREIGN_BINARY_DATA";
    fwrite(data, 1, sizeof(data), f1);
    fclose(f1);

    int rc = alrios_armake_write_index(test_dir, index_file);
    (void)rc;
    assert(rc == ALRIOS_INDEX_OK);

    // 2. Acceptance Criterion 3: Index signature / integrity verified
    uint8_t dummy_pubkey[32] = {0};
    int vc = alrios_index_verify(index_file, dummy_pubkey);
    (void)vc;
    assert(vc == ALRIOS_INDEX_OK);

    // 3. Acceptance Criterion 2: Mutable extensions rejected (.db, .sqlite, .wal, etc.)
    FILE *f_bad = fopen("/tmp/alrios_test_index_dir/database.sqlite", "wb");
    assert(f_bad != NULL);
    fwrite("DB", 1, 2, f_bad);
    fclose(f_bad);

    rc = alrios_armake_write_index(test_dir, index_file);
    assert(rc == ALRIOS_INDEX_ERR_MUTABLE_FILE); // Mutable files strictly rejected!

    command_status = system("rm -rf /tmp/alrios_test_index_dir");
    if (command_status != 0) {
        return 1;
    }
    unlink(index_file);
    printf("TEST_INDEX: PASS\n");
    return 0;
}
