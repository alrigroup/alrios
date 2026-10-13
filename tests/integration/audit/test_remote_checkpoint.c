/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#define _POSIX_C_SOURCE 200809L

#include "alrios/audit/block.h"
#include "alrios/audit/forwarder.h"
#include "alrios/crypto_verify.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                      __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

typedef struct mock_collector {
    uint8_t key[ALRIOS_FORWARDER_KEY_SIZE];
    int tamper_receipt;
    int fail_transport;
    size_t calls;
} mock_collector_t;

static int temporary_fd(void) {
    char path[] = "/tmp/alrios-forwarder-XXXXXX";
    int fd = mkstemp(path);

    CHECK(fd >= 0);
    CHECK(unlink(path) == 0);
    return fd;
}

static void fill_key(uint8_t key[ALRIOS_FORWARDER_KEY_SIZE], uint8_t seed) {
    size_t index;

    for (index = 0U; index < ALRIOS_FORWARDER_KEY_SIZE; ++index) {
        key[index] = (uint8_t)(seed + (uint8_t)index);
    }
}

static int mock_send(const uint8_t *checkpoint_wire, size_t wire_len,
                     uint8_t *receipt_buf, size_t receipt_buf_capacity,
                     size_t *receipt_received_len, void *user_ctx) {
    mock_collector_t *collector = user_ctx;
    alrios_audit_checkpoint_t checkpoint;
    uint8_t nonce[ALRIOS_FORWARDER_RECEIPT_NONCE_SIZE];
    int status;

    CHECK(collector != NULL);
    ++collector->calls;
    if (collector->fail_transport) {
        *receipt_received_len = 0U;
        return ALRIOS_FORWARDER_ERR_TRANSPORT;
    }
    if (wire_len != ALRIOS_CHECKPOINT_WIRE_SIZE ||
        receipt_buf_capacity < ALRIOS_RECEIPT_WIRE_SIZE) {
        return ALRIOS_FORWARDER_ERR_TRANSPORT;
    }
    status = alrios_checkpoint_decode_and_verify(checkpoint_wire,
                                                  collector->key,
                                                  &checkpoint);
    if (status != ALRIOS_FORWARDER_OK) {
        return status;
    }
    memset(nonce, 0x5a, sizeof(nonce));
    status = alrios_receipt_encode(checkpoint.sequence,
                                   checkpoint.block_hash,
                                   nonce, collector->key, receipt_buf);
    if (status == ALRIOS_FORWARDER_OK) {
        if (collector->tamper_receipt) {
            receipt_buf[20U] ^= 0x80U;
        }
        *receipt_received_len = ALRIOS_RECEIPT_WIRE_SIZE;
    }
    return status;
}

static void test_checkpoint_and_receipt_authentication(void) {
    uint8_t key[ALRIOS_FORWARDER_KEY_SIZE];
    uint8_t wrong_key[ALRIOS_FORWARDER_KEY_SIZE];
    uint8_t hash[ALRIOS_AUDIT_BLOCK_HASH_SIZE];
    uint8_t nonce[ALRIOS_FORWARDER_RECEIPT_NONCE_SIZE];
    uint8_t checkpoint_wire[ALRIOS_CHECKPOINT_WIRE_SIZE];
    uint8_t receipt_wire[ALRIOS_RECEIPT_WIRE_SIZE];
    alrios_audit_checkpoint_t checkpoint;
    alrios_audit_receipt_t receipt;

    fill_key(key, 1U);
    fill_key(wrong_key, 2U);
    memset(hash, 0x33, sizeof(hash));
    memset(nonce, 0x44, sizeof(nonce));
    CHECK(alrios_checkpoint_encode(7U, 1234U, hash, key,
                                   checkpoint_wire) == ALRIOS_FORWARDER_OK);
    CHECK(alrios_checkpoint_decode_and_verify(checkpoint_wire, key,
                                               &checkpoint) == ALRIOS_FORWARDER_OK);
    CHECK(checkpoint.sequence == 7U && checkpoint.timestamp_ns == 1234U);
    CHECK(memcmp(checkpoint.block_hash, hash, sizeof(hash)) == 0);
    CHECK(alrios_checkpoint_decode_and_verify(checkpoint_wire, wrong_key,
                                               &checkpoint) == ALRIOS_FORWARDER_ERR_AUTH);
    checkpoint_wire[31U] ^= 1U;
    CHECK(alrios_checkpoint_decode_and_verify(checkpoint_wire, key,
                                               &checkpoint) == ALRIOS_FORWARDER_ERR_AUTH);
    checkpoint_wire[31U] ^= 1U;

    CHECK(alrios_receipt_encode(7U, hash, nonce, key,
                                receipt_wire) == ALRIOS_FORWARDER_OK);
    CHECK(alrios_receipt_decode_and_verify(receipt_wire, key,
                                           &receipt) == ALRIOS_FORWARDER_OK);
    CHECK(receipt.ack_sequence == 7U);
    CHECK(memcmp(receipt.ack_hash, hash, sizeof(hash)) == 0);
    receipt_wire[52U] ^= 1U;
    CHECK(alrios_receipt_decode_and_verify(receipt_wire, key,
                                           &receipt) == ALRIOS_FORWARDER_ERR_AUTH);
}

static void test_forwarding_and_backpressure(void) {
    uint8_t key[ALRIOS_FORWARDER_KEY_SIZE];
    uint8_t first_hash[ALRIOS_AUDIT_BLOCK_HASH_SIZE];
    uint8_t second_hash[ALRIOS_AUDIT_BLOCK_HASH_SIZE];
    alrios_audit_checkpoint_t queue[2];
    alrios_audit_forwarder_t forwarder;
    alrios_forwarder_state_t state;
    mock_collector_t collector;
    int state_fd = temporary_fd();

    fill_key(key, 9U);
    memset(first_hash, 0x11, sizeof(first_hash));
    memset(second_hash, 0x22, sizeof(second_hash));
    memset(&collector, 0, sizeof(collector));
    memcpy(collector.key, key, sizeof(key));
    CHECK(alrios_forwarder_init(&forwarder, queue, 2U, key, state_fd,
                                mock_send, &collector) == ALRIOS_FORWARDER_OK);

    CHECK(alrios_forwarder_enqueue(&forwarder, 1U, 10U, first_hash) ==
          ALRIOS_FORWARDER_OK);
    CHECK(alrios_forwarder_enqueue(&forwarder, 2U, 20U, second_hash) ==
          ALRIOS_FORWARDER_OK);
    CHECK(alrios_forwarder_enqueue(&forwarder, 3U, 30U, first_hash) ==
          ALRIOS_FORWARDER_ERR_QUEUE_FULL);
    CHECK(forwarder.queue_count == 2U);

    collector.fail_transport = 1;
    CHECK(alrios_forwarder_flush(&forwarder) ==
          ALRIOS_FORWARDER_ERR_TRANSPORT);
    CHECK(forwarder.queue_count == 2U);
    collector.fail_transport = 0;
    collector.tamper_receipt = 1;
    CHECK(alrios_forwarder_flush(&forwarder) == ALRIOS_FORWARDER_ERR_AUTH);
    CHECK(forwarder.queue_count == 2U);
    collector.tamper_receipt = 0;

    CHECK(alrios_forwarder_flush(&forwarder) == ALRIOS_FORWARDER_OK);
    CHECK(forwarder.queue_count == 1U);
    CHECK(alrios_forwarder_state_read(state_fd, key, &state) ==
          ALRIOS_FORWARDER_OK);
    CHECK(state.ack_sequence == 1U);
    CHECK(memcmp(state.ack_hash, first_hash, sizeof(first_hash)) == 0);
    CHECK(alrios_forwarder_flush(&forwarder) == ALRIOS_FORWARDER_OK);
    CHECK(forwarder.queue_count == 0U);
    CHECK(collector.calls == 4U);

    alrios_forwarder_destroy(&forwarder);
    CHECK(close(state_fd) == 0);
    alrios_explicit_zeroize(key, sizeof(key));
}

static void test_rollback_and_deletion_detection(void) {
    static const uint8_t first_payload[] = "one";
    static const uint8_t second_payload[] = "two";
    uint8_t key[ALRIOS_FORWARDER_KEY_SIZE];
    alrios_audit_block_metadata_t first;
    alrios_audit_block_metadata_t second;
    alrios_forwarder_state_t state;
    int audit_fd = temporary_fd();
    int state_fd = temporary_fd();
    off_t first_size;

    fill_key(key, 0xa0U);
    CHECK(alrios_audit_chain_append_fd(audit_fd, 1U, first_payload,
                                       sizeof(first_payload) - 1U,
                                       &first) == ALRIOS_AUDIT_OK);
    first_size = (off_t)(ALRIOS_AUDIT_BLOCK_WIRE_MIN_V1 +
                         sizeof(first_payload) - 1U);
    CHECK(alrios_audit_chain_append_fd(audit_fd, 2U, second_payload,
                                       sizeof(second_payload) - 1U,
                                       &second) == ALRIOS_AUDIT_OK);
    state.ack_sequence = second.sequence;
    memcpy(state.ack_hash, second.block_hash, sizeof(state.ack_hash));
    CHECK(alrios_forwarder_state_write(state_fd, key, &state) ==
          ALRIOS_FORWARDER_OK);
    CHECK(alrios_forwarder_verify_chain_not_rolled_back(audit_fd, &state) ==
          ALRIOS_FORWARDER_OK);

    CHECK(ftruncate(audit_fd, first_size) == 0);
    CHECK(fsync(audit_fd) == 0);
    CHECK(alrios_forwarder_verify_chain_not_rolled_back(audit_fd, &state) ==
          ALRIOS_FORWARDER_ERR_ROLLBACK_DETECTED);
    CHECK(ftruncate(audit_fd, 0) == 0);
    CHECK(fsync(audit_fd) == 0);
    CHECK(alrios_forwarder_verify_chain_not_rolled_back(audit_fd, &state) ==
          ALRIOS_FORWARDER_ERR_ROLLBACK_DETECTED);

    /* Test total deletion: unlink file while verified against persisted state */
    {
        char temp_del_path[] = "/tmp/alrios-audit-del-XXXXXX";
        int del_fd = mkstemp(temp_del_path);
        CHECK(del_fd >= 0);
        CHECK(close(del_fd) == 0);
        CHECK(unlink(temp_del_path) == 0);
        /* Attempting to open deleted path fails closed */
        int missing_fd = open(temp_del_path, O_RDONLY);
        CHECK(missing_fd < 0);
        /* Verifying with invalid fd fails closed */
        CHECK(alrios_forwarder_verify_chain_not_rolled_back(missing_fd, &state) ==
              ALRIOS_FORWARDER_ERR_INVALID_ARGUMENT);
    }

    CHECK(close(audit_fd) == 0);
    CHECK(close(state_fd) == 0);
    alrios_explicit_zeroize(key, sizeof(key));
}

static void test_state_tampering(void) {
    uint8_t key[ALRIOS_FORWARDER_KEY_SIZE];
    alrios_forwarder_state_t state;
    uint8_t byte;
    int fd = temporary_fd();

    fill_key(key, 0x50U);
    state.ack_sequence = 5U;
    memset(state.ack_hash, 0xef, sizeof(state.ack_hash));
    CHECK(alrios_forwarder_state_write(fd, key, &state) ==
          ALRIOS_FORWARDER_OK);
    CHECK(pread(fd, &byte, 1U, 25) == 1);
    byte ^= 1U;
    CHECK(pwrite(fd, &byte, 1U, 25) == 1);
    CHECK(fsync(fd) == 0);
    CHECK(alrios_forwarder_state_read(fd, key, &state) ==
          ALRIOS_FORWARDER_ERR_STATE_CORRUPT);
    CHECK(close(fd) == 0);
}

int main(void) {
    test_checkpoint_and_receipt_authentication();
    test_forwarding_and_backpressure();
    test_rollback_and_deletion_detection();
    test_state_tampering();
    (void)printf("MP-016 (Remote Audit Checkpoint): PASS\n");
    return EXIT_SUCCESS;
}
