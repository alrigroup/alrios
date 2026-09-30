/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/audit/block.h"
#include "alrios/audit/forwarder.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>

int main(void) {
    const char *test_path = "/tmp/alrios_audit_remote.log";
    unlink(test_path);

    alrios_audit_block_t b1;
    const uint8_t *payload = (const uint8_t *)"CHECKPOINT_TEST_EVENT";
    int rc = alrios_audit_append(test_path, 1700000000ULL, payload, strlen((const char *)payload), &b1);
    (void)rc;
    assert(rc == ALRIOS_AUDIT_OK);

    alrios_audit_checkpoint_t cp;
    rc = alrios_audit_create_checkpoint(test_path, &cp);
    assert(rc == ALRIOS_AUDIT_OK);
    assert(cp.sequence == 0);

    // Test socket forwarding / backpressure
    int sv[2] = {-1, -1};
    int sp = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    (void)sp;
    assert(sp == 0);

    rc = alrios_audit_forward_checkpoint(sv[0], &cp, 100);
    assert(rc == ALRIOS_AUDIT_OK);

    alrios_audit_checkpoint_t received_cp;
    ssize_t r = read(sv[1], &received_cp, sizeof(received_cp));
    (void)r;
    assert(r == (ssize_t)sizeof(received_cp));
    assert(received_cp.sequence == cp.sequence);
    assert(memcmp(received_cp.root_hash, cp.root_hash, ALRIOS_AUDIT_HASH_SIZE) == 0);

    close(sv[0]);
    close(sv[1]);
    unlink(test_path);

    printf("TEST_REMOTE_CHECKPOINT: PASS\n");
    return 0;
}
