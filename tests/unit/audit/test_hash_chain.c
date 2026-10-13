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

static int temporary_fd(void) {
    char path[] = "/tmp/alrios-audit-XXXXXX";
    int fd = mkstemp(path);

    CHECK(fd >= 0);
    CHECK(unlink(path) == 0);
    return fd;
}

static void write_all(int fd, const uint8_t *data, size_t length) {
    size_t written = 0U;

    while (written < length) {
        ssize_t result = write(fd, data + written, length - written);
        if (result > 0) {
            written += (size_t)result;
        } else if (result < 0 && errno == EINTR) {
            continue;
        } else {
            CHECK(0);
        }
    }
}

static void test_canonical_block(void) {
    static const uint8_t payload[] = {0x00U, 0x41U, 0xffU, 0x7fU};
    uint8_t previous_hash[ALRIOS_AUDIT_BLOCK_HASH_SIZE];
    uint8_t wire[ALRIOS_AUDIT_BLOCK_WIRE_MIN_V1 + sizeof(payload)];
    alrios_audit_block_metadata_t encoded;
    alrios_audit_block_metadata_t verified;
    size_t written = 0U;
    size_t index;

    for (index = 0U; index < sizeof(previous_hash); ++index) {
        previous_hash[index] = (uint8_t)index;
    }
    CHECK(alrios_audit_block_encode(previous_hash, UINT64_C(0x0102030405060708),
                                    UINT64_C(0x1112131415161718), payload,
                                    sizeof(payload), wire, sizeof(wire), &written,
                                    &encoded) == ALRIOS_AUDIT_OK);
    CHECK(written == sizeof(wire));
    CHECK(memcmp(wire, "ALRIAUDT", 8U) == 0);
    CHECK(wire[8U] == 0U && wire[9U] == ALRIOS_AUDIT_BLOCK_VERSION_1);
    CHECK(wire[10U] == 0U && wire[11U] == ALRIOS_AUDIT_BLOCK_HEADER_SIZE_V1);
    CHECK(wire[12U] == 0U && wire[13U] == 0U &&
          wire[14U] == (uint8_t)(sizeof(wire) >> 8U) &&
          wire[15U] == (uint8_t)sizeof(wire));
    CHECK(wire[16U] == 0U && wire[17U] == 0U && wire[18U] == 0U &&
          wire[19U] == sizeof(payload));
    CHECK(wire[20U] == 0U && wire[21U] == 0U &&
          wire[22U] == 0U && wire[23U] == 0U);
    CHECK(wire[24U] == 0x01U && wire[31U] == 0x08U);
    CHECK(wire[32U] == 0x11U && wire[39U] == 0x18U);
    CHECK(memcmp(wire + 40U, previous_hash, sizeof(previous_hash)) == 0);
    CHECK(memcmp(wire + ALRIOS_AUDIT_BLOCK_HEADER_SIZE_V1,
                 payload, sizeof(payload)) == 0);
    CHECK(alrios_audit_block_verify(wire, written, &verified) == ALRIOS_AUDIT_OK);
    CHECK(verified.sequence == encoded.sequence);
    CHECK(memcmp(verified.block_hash, encoded.block_hash,
                 ALRIOS_AUDIT_BLOCK_HASH_SIZE) == 0);

    wire[ALRIOS_AUDIT_BLOCK_HEADER_SIZE_V1 + 1U] ^= 1U;
    CHECK(alrios_audit_block_verify(wire, written, &verified) ==
          ALRIOS_AUDIT_ERR_HASH);
    wire[ALRIOS_AUDIT_BLOCK_HEADER_SIZE_V1 + 1U] ^= 1U;
    wire[20U] = 1U;
    CHECK(alrios_audit_block_verify(wire, written, &verified) ==
          ALRIOS_AUDIT_ERR_NONCANONICAL);
}

static void test_chain_linkage_and_mutation(void) {
    static const uint8_t first[] = "first event";
    static const uint8_t second[] = "second event";
    alrios_audit_block_metadata_t first_meta;
    alrios_audit_block_metadata_t second_meta;
    alrios_audit_chain_result_t result;
    uint8_t byte;
    int fd = temporary_fd();

    CHECK(alrios_audit_chain_append_fd(fd, 100U, first, sizeof(first) - 1U,
                                       &first_meta) == ALRIOS_AUDIT_OK);
    CHECK(first_meta.sequence == 1U);
    CHECK(alrios_audit_chain_append_fd(fd, 101U, second, sizeof(second) - 1U,
                                       &second_meta) == ALRIOS_AUDIT_OK);
    CHECK(second_meta.sequence == 2U);
    CHECK(memcmp(second_meta.previous_hash, first_meta.block_hash,
                 ALRIOS_AUDIT_BLOCK_HASH_SIZE) == 0);
    CHECK(alrios_audit_chain_verify_fd(fd, &result) == ALRIOS_AUDIT_OK);
    CHECK(result.block_count == 2U && result.last_sequence == 2U);
    CHECK(memcmp(result.last_hash, second_meta.block_hash,
                 ALRIOS_AUDIT_BLOCK_HASH_SIZE) == 0);

    CHECK(pread(fd, &byte, 1U,
                (off_t)(ALRIOS_AUDIT_BLOCK_HEADER_SIZE_V1 + 2U)) == 1);
    byte ^= 0x40U;
    CHECK(pwrite(fd, &byte, 1U,
                 (off_t)(ALRIOS_AUDIT_BLOCK_HEADER_SIZE_V1 + 2U)) == 1);
    CHECK(fsync(fd) == 0);
    CHECK(alrios_audit_chain_verify_fd(fd, &result) == ALRIOS_AUDIT_ERR_HASH);
    CHECK(alrios_audit_chain_recover_fd(fd, &result) == ALRIOS_AUDIT_ERR_HASH);
    CHECK(close(fd) == 0);
}

static void test_link_and_sequence_tampering(void) {
    static const uint8_t payload[] = "event";
    alrios_audit_block_metadata_t first_meta;
    alrios_audit_block_metadata_t ignored;
    alrios_audit_chain_result_t result;
    uint8_t zero_hash[ALRIOS_AUDIT_BLOCK_HASH_SIZE];
    uint8_t wire[ALRIOS_AUDIT_BLOCK_WIRE_MIN_V1 + sizeof(payload) - 1U];
    size_t written = 0U;
    int fd = temporary_fd();

    memset(zero_hash, 0, sizeof(zero_hash));
    CHECK(alrios_audit_chain_append_fd(fd, 1U, payload, sizeof(payload) - 1U,
                                       &first_meta) == ALRIOS_AUDIT_OK);
    CHECK(alrios_audit_block_encode(zero_hash, 2U, 2U, payload,
                                    sizeof(payload) - 1U, wire, sizeof(wire),
                                    &written, &ignored) == ALRIOS_AUDIT_OK);
    CHECK(lseek(fd, 0, SEEK_END) >= 0);
    write_all(fd, wire, written);
    CHECK(fsync(fd) == 0);
    CHECK(alrios_audit_chain_verify_fd(fd, &result) == ALRIOS_AUDIT_ERR_CHAIN);
    CHECK(ftruncate(fd, (off_t)(ALRIOS_AUDIT_BLOCK_WIRE_MIN_V1 +
                                sizeof(payload) - 1U)) == 0);

    CHECK(alrios_audit_block_encode(first_meta.block_hash, 3U, 2U, payload,
                                    sizeof(payload) - 1U, wire, sizeof(wire),
                                    &written, &ignored) == ALRIOS_AUDIT_OK);
    CHECK(lseek(fd, 0, SEEK_END) >= 0);
    write_all(fd, wire, written);
    CHECK(fsync(fd) == 0);
    CHECK(alrios_audit_chain_verify_fd(fd, &result) == ALRIOS_AUDIT_ERR_SEQUENCE);
    CHECK(close(fd) == 0);
}

static void test_partial_tail_recovery(void) {
    static const uint8_t first[] = "durable";
    static const uint8_t second[] = "interrupted";
    alrios_audit_block_metadata_t first_meta;
    alrios_audit_block_metadata_t second_meta;
    alrios_audit_block_metadata_t appended_meta;
    alrios_audit_chain_result_t result;
    uint8_t wire[ALRIOS_AUDIT_BLOCK_WIRE_MIN_V1 + sizeof(second) - 1U];
    size_t written = 0U;
    struct stat file_status;
    int fd = temporary_fd();

    CHECK(alrios_audit_chain_append_fd(fd, 10U, first, sizeof(first) - 1U,
                                       &first_meta) == ALRIOS_AUDIT_OK);
    CHECK(alrios_audit_block_encode(first_meta.block_hash, 2U, 11U, second,
                                    sizeof(second) - 1U, wire, sizeof(wire),
                                    &written, &second_meta) == ALRIOS_AUDIT_OK);
    CHECK(lseek(fd, 0, SEEK_END) >= 0);
    write_all(fd, wire, written - 7U);
    CHECK(fsync(fd) == 0);
    CHECK(alrios_audit_chain_verify_fd(fd, &result) == ALRIOS_AUDIT_ERR_TRUNCATED);
    CHECK(alrios_audit_chain_recover_fd(fd, &result) == ALRIOS_AUDIT_OK);
    CHECK(result.tail_recovered == 1U && result.block_count == 1U);
    CHECK(result.last_sequence == 1U);
    CHECK(fstat(fd, &file_status) == 0);
    CHECK((uint64_t)file_status.st_size == result.valid_bytes);

    CHECK(alrios_audit_chain_append_fd(fd, 12U, second, sizeof(second) - 1U,
                                       &appended_meta) == ALRIOS_AUDIT_OK);
    CHECK(appended_meta.sequence == 2U);
    CHECK(memcmp(appended_meta.previous_hash, first_meta.block_hash,
                 ALRIOS_AUDIT_BLOCK_HASH_SIZE) == 0);
    CHECK(alrios_audit_chain_verify_fd(fd, &result) == ALRIOS_AUDIT_OK);
    CHECK(result.block_count == 2U && result.tail_recovered == 0U);
    CHECK(close(fd) == 0);
}

static void test_partial_magic_and_garbage(void) {
    static const uint8_t partial_magic[] = {'A', 'L', 'R'};
    static const uint8_t garbage[] = {'X', 'Y'};
    alrios_audit_chain_result_t result;
    struct stat file_status;
    int fd = temporary_fd();

    write_all(fd, partial_magic, sizeof(partial_magic));
    CHECK(fsync(fd) == 0);
    CHECK(alrios_audit_chain_recover_fd(fd, &result) == ALRIOS_AUDIT_OK);
    CHECK(result.tail_recovered == 1U && result.valid_bytes == 0U);
    CHECK(fstat(fd, &file_status) == 0 && file_status.st_size == 0);
    CHECK(lseek(fd, 0, SEEK_SET) == 0);
    write_all(fd, garbage, sizeof(garbage));
    CHECK(fsync(fd) == 0);
    CHECK(alrios_audit_chain_recover_fd(fd, &result) == ALRIOS_AUDIT_ERR_FORMAT);
    CHECK(fstat(fd, &file_status) == 0 &&
          file_status.st_size == (off_t)sizeof(garbage));
    CHECK(close(fd) == 0);
}

static void test_limits_and_offsets(void) {
    uint8_t zero_hash[ALRIOS_AUDIT_BLOCK_HASH_SIZE];
    uint8_t wire[ALRIOS_AUDIT_BLOCK_WIRE_MIN_V1];
    uint8_t payload = 1U;
    alrios_audit_block_metadata_t metadata;
    alrios_audit_chain_result_t result;
    size_t written = 99U;
    int fd = temporary_fd();

    memset(zero_hash, 0, sizeof(zero_hash));
    CHECK(alrios_audit_block_encode(zero_hash, 0U, 0U, NULL, 0U, wire,
                                    sizeof(wire), &written, &metadata) ==
          ALRIOS_AUDIT_ERR_INVALID_ARGUMENT);
    CHECK(written == 0U);
    CHECK(alrios_audit_block_encode(zero_hash, 1U, 0U, &payload,
                                    ALRIOS_AUDIT_BLOCK_PAYLOAD_MAX + 1U, wire,
                                    sizeof(wire), &written, &metadata) ==
          ALRIOS_AUDIT_ERR_LENGTH);
    CHECK(lseek(fd, 7, SEEK_SET) == 7);
    CHECK(alrios_audit_chain_verify_fd(fd, &result) == ALRIOS_AUDIT_OK);
    CHECK(lseek(fd, 0, SEEK_CUR) == 7);
    CHECK(alrios_audit_chain_append_fd(fd, 1U, NULL, 0U, &metadata) ==
          ALRIOS_AUDIT_OK);
    CHECK(lseek(fd, 0, SEEK_CUR) == 7);
    CHECK(alrios_audit_chain_verify_fd(fd, &result) == ALRIOS_AUDIT_OK);
    CHECK(result.block_count == 1U);
    CHECK(close(fd) == 0);
}

int main(void) {
    test_canonical_block();
    test_chain_linkage_and_mutation();
    test_link_and_sequence_tampering();
    test_partial_tail_recovery();
    test_partial_magic_and_garbage();
    test_limits_and_offsets();
    (void)printf("MP-015 (Audit Hash Chain): PASS\n");
    return EXIT_SUCCESS;
}
