/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_AUDIT_BLOCK_H
#define ALRIOS_AUDIT_BLOCK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALRIOS_AUDIT_BLOCK_VERSION_1          1U
#define ALRIOS_AUDIT_BLOCK_HEADER_SIZE_V1   104U
#define ALRIOS_AUDIT_BLOCK_HASH_SIZE         32U
#define ALRIOS_AUDIT_BLOCK_TRAILER_SIZE_V1   32U
#define ALRIOS_AUDIT_BLOCK_WIRE_MIN_V1      136U
#define ALRIOS_AUDIT_BLOCK_PAYLOAD_MAX  1048576U
#define ALRIOS_AUDIT_BLOCK_WIRE_MAX_V1  1048712U

#define ALRIOS_AUDIT_OK                       0
#define ALRIOS_AUDIT_ERR_INVALID_ARGUMENT -7001
#define ALRIOS_AUDIT_ERR_CAPACITY         -7002
#define ALRIOS_AUDIT_ERR_FORMAT           -7003
#define ALRIOS_AUDIT_ERR_VERSION          -7004
#define ALRIOS_AUDIT_ERR_LENGTH           -7005
#define ALRIOS_AUDIT_ERR_NONCANONICAL     -7006
#define ALRIOS_AUDIT_ERR_HASH             -7007
#define ALRIOS_AUDIT_ERR_CHAIN            -7008
#define ALRIOS_AUDIT_ERR_SEQUENCE         -7009
#define ALRIOS_AUDIT_ERR_TRUNCATED        -7010
#define ALRIOS_AUDIT_ERR_IO               -7011
#define ALRIOS_AUDIT_ERR_OVERFLOW         -7012
#define ALRIOS_AUDIT_ERR_BUSY             -7013
#define ALRIOS_AUDIT_ERR_OVERLAP          -7014

typedef struct alrios_audit_block_metadata {
    uint16_t version;
    uint64_t sequence;
    uint64_t timestamp_ns;
    uint32_t payload_len;
    uint8_t previous_hash[ALRIOS_AUDIT_BLOCK_HASH_SIZE];
    uint8_t payload_hash[ALRIOS_AUDIT_BLOCK_HASH_SIZE];
    uint8_t block_hash[ALRIOS_AUDIT_BLOCK_HASH_SIZE];
} alrios_audit_block_metadata_t;

typedef struct alrios_audit_chain_result {
    uint64_t block_count;
    uint64_t valid_bytes;
    uint64_t last_sequence;
    uint8_t last_hash[ALRIOS_AUDIT_BLOCK_HASH_SIZE];
    uint8_t tail_recovered;
} alrios_audit_chain_result_t;

/*
 * V1 wire records are canonical, packed byte streams; native structures are
 * never persisted. Integer fields are unsigned big-endian and laid out as:
 * magic[8], version:u16, header_size:u16, total_size:u32, payload_size:u32,
 * flags:u32 (zero), sequence:u64, timestamp_ns:u64, previous_hash[32],
 * payload_hash[32], payload[payload_size], block_hash[32]. The block hash is
 * SHA-256("ALRIOS-AUDIT-BLOCK-V1" || header || payload).
 */
int alrios_audit_block_encode(
    const uint8_t previous_hash[ALRIOS_AUDIT_BLOCK_HASH_SIZE],
    uint64_t sequence,
    uint64_t timestamp_ns,
    const uint8_t *payload,
    size_t payload_len,
    uint8_t *out_wire,
    size_t out_capacity,
    size_t *out_written,
    alrios_audit_block_metadata_t *out_metadata);

/* Validates exactly one record, including canonical encoding and both hashes. */
int alrios_audit_block_verify(const uint8_t *wire,
                              size_t wire_len,
                              alrios_audit_block_metadata_t *out_metadata);

/*
 * These descriptor APIs preserve the descriptor's current file offset and use
 * advisory whole-file locks. The descriptor must be open for reading; recovery
 * and append additionally require write access. Verification never modifies
 * the file. Recovery removes only a recognizable, incomplete final V1 record;
 * malformed complete records and unrecognized garbage fail closed.
 */
int alrios_audit_chain_verify_fd(int fd,
                                 alrios_audit_chain_result_t *out_result);
int alrios_audit_chain_recover_fd(int fd,
                                  alrios_audit_chain_result_t *out_result);

/*
 * Append first validates the complete chain, safely recovers a partial tail,
 * writes one record, and calls fsync before success. A failed write is rolled
 * back to the previously valid length. Durability of a newly-created file name
 * still requires the creator to fsync its parent directory.
 */
int alrios_audit_chain_append_fd(int fd,
                                 uint64_t timestamp_ns,
                                 const uint8_t *payload,
                                 size_t payload_len,
                                 alrios_audit_block_metadata_t *out_metadata);

#ifdef __cplusplus
}
#endif

#endif /* ALRIOS_AUDIT_BLOCK_H */
