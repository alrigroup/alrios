/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/audit/block.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int main(void) {
    const char *test_path = "/tmp/alrios_audit_test.log";
    unlink(test_path);

    alrios_audit_block_t b1, b2;
    const uint8_t *payload1 = (const uint8_t *)"AUDIT_EVENT_1";
    const uint8_t *payload2 = (const uint8_t *)"AUDIT_EVENT_2";

    // 1. Append first block
    int rc = alrios_audit_append(test_path, 1700000000ULL, payload1, strlen((const char *)payload1), &b1);
    (void)rc;
    assert(rc == ALRIOS_AUDIT_OK);
    assert(b1.sequence == 0);
    for (int i = 0; i < 32; i++) {
        assert(b1.previous_hash[i] == 0);
    }

    // 2. Append second block
    rc = alrios_audit_append(test_path, 1700000001ULL, payload2, strlen((const char *)payload2), &b2);
    assert(rc == ALRIOS_AUDIT_OK);
    assert(b2.sequence == 1);
    assert(memcmp(b2.previous_hash, b1.hash, ALRIOS_AUDIT_HASH_SIZE) == 0);

    // 3. Verify valid chain
    alrios_audit_verify_result_t res;
    memset(&res, 0, sizeof(res));
    rc = alrios_audit_verify(test_path, &res);
    assert(rc == ALRIOS_AUDIT_OK);
    assert(res.block_count == 2);
    assert(res.has_incomplete_tail == 0);
    assert(memcmp(res.last_hash, b2.hash, ALRIOS_AUDIT_HASH_SIZE) == 0);

    // 4. Test mutation detection (corrupt file content)
    int fd = open(test_path, O_RDWR);
    assert(fd >= 0);
    off_t pos = (off_t)(ALRIOS_AUDIT_RECORD_HEADER_SIZE + 2);
    (void)pos;
    assert(lseek(fd, pos, SEEK_SET) == pos);
    unsigned char bad_byte = 0xFF;
    ssize_t wr = write(fd, &bad_byte, 1);
    (void)wr;
    assert(wr == 1);
    close(fd);

    // Verify should fail with hash mismatch
    memset(&res, 0, sizeof(res));
    rc = alrios_audit_verify(test_path, &res);
    assert(rc == ALRIOS_AUDIT_ERR_HASH_MISMATCH);

    unlink(test_path);
    printf("TEST_HASH_CHAIN: PASS\n");
    return 0;
}
