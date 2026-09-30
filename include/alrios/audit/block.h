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

#define ALRIOS_AUDIT_FORMAT_VERSION 1U
#define ALRIOS_AUDIT_HASH_SIZE 32U
#define ALRIOS_AUDIT_MAX_PAYLOAD_SIZE (16U * 1024U * 1024U)
#define ALRIOS_AUDIT_RECORD_HEADER_SIZE 96U
#define ALRIOS_AUDIT_RECORD_TRAILER_SIZE 40U
#define ALRIOS_AUDIT_RECORD_OVERHEAD \
    (ALRIOS_AUDIT_RECORD_HEADER_SIZE + ALRIOS_AUDIT_RECORD_TRAILER_SIZE)

#define ALRIOS_AUDIT_OK 0
#define ALRIOS_AUDIT_ERR_INVALID_ARGUMENT -7001
#define ALRIOS_AUDIT_ERR_IO -7002
#define ALRIOS_AUDIT_ERR_FORMAT -7003
#define ALRIOS_AUDIT_ERR_TRUNCATED -7004
#define ALRIOS_AUDIT_ERR_HASH_MISMATCH -7005
#define ALRIOS_AUDIT_ERR_SEQUENCE -7006
#define ALRIOS_AUDIT_ERR_OVERFLOW -7007
#define ALRIOS_AUDIT_ERR_UNAVAILABLE -7008
#define ALRIOS_AUDIT_ERR_LOCK -7009
#define ALRIOS_AUDIT_ERR_NOT_REGULAR -7010

typedef struct alrios_audit_block {
    uint64_t sequence;
    uint64_t timestamp;
    uint32_t payload_size;
    uint8_t previous_hash[ALRIOS_AUDIT_HASH_SIZE];
    uint8_t hash[ALRIOS_AUDIT_HASH_SIZE];
} alrios_audit_block_t;

typedef struct alrios_audit_verify_result {
    uint64_t block_count;
    uint64_t valid_bytes;
    uint8_t last_hash[ALRIOS_AUDIT_HASH_SIZE];
    int has_incomplete_tail;
} alrios_audit_verify_result_t;

int alrios_audit_compute_hash(uint64_t sequence,
                              uint64_t timestamp,
                              const uint8_t previous_hash[ALRIOS_AUDIT_HASH_SIZE],
                              const uint8_t *payload,
                              size_t payload_size,
                              uint8_t out_hash[ALRIOS_AUDIT_HASH_SIZE]);

int alrios_audit_append(const char *path,
                        uint64_t timestamp,
                        const uint8_t *payload,
                        size_t payload_size,
                        alrios_audit_block_t *out_block);

int alrios_audit_verify(const char *path,
                        alrios_audit_verify_result_t *out_result);

#ifdef __cplusplus
}
#endif

#endif
