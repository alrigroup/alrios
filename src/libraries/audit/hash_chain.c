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
#include "alrios/audit/block.h"
#include "alrios/crypto_verify.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>

#define AUDIT_MAGIC 0x41554454U /* "AUDT" */
#define AUDIT_TRAILER_MAGIC 0x54445541U /* "TDUA" */

#pragma pack(push, 1)
typedef struct audit_record_header {
    uint32_t magic;
    uint32_t version;
    uint64_t sequence;
    uint64_t timestamp;
    uint32_t payload_size;
    uint8_t previous_hash[ALRIOS_AUDIT_HASH_SIZE];
    uint8_t hash[ALRIOS_AUDIT_HASH_SIZE];
    uint8_t reserved[4];
} audit_record_header_t;

typedef struct audit_record_trailer {
    uint32_t trailer_magic;
    uint32_t checksum_type;
    uint8_t padding[32];
} audit_record_trailer_t;
#pragma pack(pop)

_Static_assert(sizeof(audit_record_header_t) == ALRIOS_AUDIT_RECORD_HEADER_SIZE, "Header size mismatch");
_Static_assert(sizeof(audit_record_trailer_t) == ALRIOS_AUDIT_RECORD_TRAILER_SIZE, "Trailer size mismatch");

int alrios_audit_compute_hash(uint64_t sequence,
                              uint64_t timestamp,
                              const uint8_t previous_hash[ALRIOS_AUDIT_HASH_SIZE],
                              const uint8_t *payload,
                              size_t payload_size,
                              uint8_t out_hash[ALRIOS_AUDIT_HASH_SIZE]) {
    if (!previous_hash || !out_hash) {
        return ALRIOS_AUDIT_ERR_INVALID_ARGUMENT;
    }
    if (payload_size > ALRIOS_AUDIT_MAX_PAYLOAD_SIZE) {
        return ALRIOS_AUDIT_ERR_OVERFLOW;
    }

    alrios_sha512_ctx_t ctx;
    if (alrios_sha512_init(&ctx) != ALRIOS_CRYPTO_OK) {
        return ALRIOS_AUDIT_ERR_UNAVAILABLE;
    }
    if (alrios_sha512_update(&ctx, (const uint8_t *)&sequence, sizeof(sequence)) != ALRIOS_CRYPTO_OK ||
        alrios_sha512_update(&ctx, (const uint8_t *)&timestamp, sizeof(timestamp)) != ALRIOS_CRYPTO_OK ||
        alrios_sha512_update(&ctx, previous_hash, ALRIOS_AUDIT_HASH_SIZE) != ALRIOS_CRYPTO_OK) {
        return ALRIOS_AUDIT_ERR_UNAVAILABLE;
    }
    if (payload && payload_size > 0) {
        if (alrios_sha512_update(&ctx, payload, payload_size) != ALRIOS_CRYPTO_OK) {
            return ALRIOS_AUDIT_ERR_UNAVAILABLE;
        }
    }
    uint8_t full_hash[64];
    if (alrios_sha512_final(&ctx, full_hash) != ALRIOS_CRYPTO_OK) {
        return ALRIOS_AUDIT_ERR_UNAVAILABLE;
    }
    memcpy(out_hash, full_hash, ALRIOS_AUDIT_HASH_SIZE);
    return ALRIOS_AUDIT_OK;
}

int alrios_audit_append(const char *path,
                        uint64_t timestamp,
                        const uint8_t *payload,
                        size_t payload_size,
                        alrios_audit_block_t *out_block) {
    if (!path || (payload_size > 0 && !payload)) {
        return ALRIOS_AUDIT_ERR_INVALID_ARGUMENT;
    }
    if (payload_size > ALRIOS_AUDIT_MAX_PAYLOAD_SIZE) {
        return ALRIOS_AUDIT_ERR_OVERFLOW;
    }

    int fd = open(path, O_RDWR | O_CREAT, 0600);
    if (fd < 0) {
        return ALRIOS_AUDIT_ERR_IO;
    }

    if (flock(fd, LOCK_EX) < 0) {
        close(fd);
        return ALRIOS_AUDIT_ERR_LOCK;
    }

    alrios_audit_verify_result_t verify_res;
    memset(&verify_res, 0, sizeof(verify_res));
    
    // Read existing file to determine last sequence and hash
    uint64_t next_seq = 0;
    uint8_t prev_hash[ALRIOS_AUDIT_HASH_SIZE];
    memset(prev_hash, 0, ALRIOS_AUDIT_HASH_SIZE);

    off_t file_len = lseek(fd, 0, SEEK_END);
    if (file_len < 0) {
        flock(fd, LOCK_UN);
        close(fd);
        return ALRIOS_AUDIT_ERR_IO;
    }

    if (file_len > 0) {
        // Verify existing chain to get correct tail
        lseek(fd, 0, SEEK_SET);
        audit_record_header_t hdr;
        while (read(fd, &hdr, sizeof(hdr)) == (ssize_t)sizeof(hdr)) {
            if (hdr.magic != AUDIT_MAGIC || hdr.version != ALRIOS_AUDIT_FORMAT_VERSION) {
                flock(fd, LOCK_UN);
                close(fd);
                return ALRIOS_AUDIT_ERR_FORMAT;
            }
            next_seq = hdr.sequence + 1;
            memcpy(prev_hash, hdr.hash, ALRIOS_AUDIT_HASH_SIZE);
            if (hdr.payload_size > ALRIOS_AUDIT_MAX_PAYLOAD_SIZE) {
                flock(fd, LOCK_UN);
                close(fd);
                return ALRIOS_AUDIT_ERR_FORMAT;
            }
            if (lseek(fd, (off_t)hdr.payload_size + ALRIOS_AUDIT_RECORD_TRAILER_SIZE, SEEK_CUR) == (off_t)-1) {
                // Incomplete tail or truncated
                break;
            }
        }
    }

    uint8_t block_hash[ALRIOS_AUDIT_HASH_SIZE];
    int hash_rc = alrios_audit_compute_hash(next_seq, timestamp, prev_hash, payload, payload_size, block_hash);
    if (hash_rc != ALRIOS_AUDIT_OK) {
        flock(fd, LOCK_UN);
        close(fd);
        return hash_rc;
    }

    audit_record_header_t new_hdr;
    memset(&new_hdr, 0, sizeof(new_hdr));
    new_hdr.magic = AUDIT_MAGIC;
    new_hdr.version = ALRIOS_AUDIT_FORMAT_VERSION;
    new_hdr.sequence = next_seq;
    new_hdr.timestamp = timestamp;
    new_hdr.payload_size = (uint32_t)payload_size;
    memcpy(new_hdr.previous_hash, prev_hash, ALRIOS_AUDIT_HASH_SIZE);
    memcpy(new_hdr.hash, block_hash, ALRIOS_AUDIT_HASH_SIZE);

    audit_record_trailer_t new_trl;
    memset(&new_trl, 0, sizeof(new_trl));
    new_trl.trailer_magic = AUDIT_TRAILER_MAGIC;
    new_trl.checksum_type = 1;

    if (lseek(fd, 0, SEEK_END) == (off_t)-1) {
        flock(fd, LOCK_UN);
        close(fd);
        return ALRIOS_AUDIT_ERR_IO;
    }

    if (write(fd, &new_hdr, sizeof(new_hdr)) != (ssize_t)sizeof(new_hdr) ||
        (payload_size > 0 && write(fd, payload, payload_size) != (ssize_t)payload_size) ||
        write(fd, &new_trl, sizeof(new_trl)) != (ssize_t)sizeof(new_trl)) {
        flock(fd, LOCK_UN);
        close(fd);
        return ALRIOS_AUDIT_ERR_IO;
    }

    if (fsync(fd) < 0) {
        flock(fd, LOCK_UN);
        close(fd);
        return ALRIOS_AUDIT_ERR_IO;
    }

    flock(fd, LOCK_UN);
    close(fd);

    if (out_block) {
        out_block->sequence = next_seq;
        out_block->timestamp = timestamp;
        out_block->payload_size = (uint32_t)payload_size;
        memcpy(out_block->previous_hash, prev_hash, ALRIOS_AUDIT_HASH_SIZE);
        memcpy(out_block->hash, block_hash, ALRIOS_AUDIT_HASH_SIZE);
    }

    return ALRIOS_AUDIT_OK;
}

int alrios_audit_verify(const char *path,
                        alrios_audit_verify_result_t *out_result) {
    if (!path || !out_result) {
        return ALRIOS_AUDIT_ERR_INVALID_ARGUMENT;
    }

    memset(out_result, 0, sizeof(*out_result));

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        if (errno == ENOENT) {
            return ALRIOS_AUDIT_OK;
        }
        return ALRIOS_AUDIT_ERR_IO;
    }

    if (flock(fd, LOCK_SH) < 0) {
        close(fd);
        return ALRIOS_AUDIT_ERR_LOCK;
    }

    struct stat st;
    if (fstat(fd, &st) < 0 || !S_ISREG(st.st_mode)) {
        flock(fd, LOCK_UN);
        close(fd);
        return ALRIOS_AUDIT_ERR_NOT_REGULAR;
    }

    uint64_t block_count = 0;
    uint64_t valid_bytes = 0;
    uint64_t expected_seq = 0;
    uint8_t expected_prev_hash[ALRIOS_AUDIT_HASH_SIZE];
    memset(expected_prev_hash, 0, ALRIOS_AUDIT_HASH_SIZE);
    int has_incomplete_tail = 0;

    audit_record_header_t hdr;
    ssize_t r;
    while ((r = read(fd, &hdr, sizeof(hdr))) > 0) {
        if (r < (ssize_t)sizeof(hdr)) {
            has_incomplete_tail = 1;
            break;
        }
        if (hdr.magic != AUDIT_MAGIC || hdr.version != ALRIOS_AUDIT_FORMAT_VERSION) {
            flock(fd, LOCK_UN);
            close(fd);
            return ALRIOS_AUDIT_ERR_FORMAT;
        }
        if (hdr.sequence != expected_seq) {
            flock(fd, LOCK_UN);
            close(fd);
            return ALRIOS_AUDIT_ERR_SEQUENCE;
        }
        if (block_count > 0 && memcmp(hdr.previous_hash, expected_prev_hash, ALRIOS_AUDIT_HASH_SIZE) != 0) {
            flock(fd, LOCK_UN);
            close(fd);
            return ALRIOS_AUDIT_ERR_HASH_MISMATCH;
        }

        // Read payload into temporary buffer or validate
        if (hdr.payload_size > ALRIOS_AUDIT_MAX_PAYLOAD_SIZE) {
            flock(fd, LOCK_UN);
            close(fd);
            return ALRIOS_AUDIT_ERR_OVERFLOW;
        }

        uint8_t *payload_buf = NULL;
        if (hdr.payload_size > 0) {
            payload_buf = (uint8_t *)malloc(hdr.payload_size);
            if (!payload_buf) {
                flock(fd, LOCK_UN);
                close(fd);
                return ALRIOS_AUDIT_ERR_UNAVAILABLE;
            }
            ssize_t pr = read(fd, payload_buf, hdr.payload_size);
            if (pr != (ssize_t)hdr.payload_size) {
                free(payload_buf);
                has_incomplete_tail = 1;
                break;
            }
        }

        audit_record_trailer_t trl;
        ssize_t tr = read(fd, &trl, sizeof(trl));
        if (tr != (ssize_t)sizeof(trl) || trl.trailer_magic != AUDIT_TRAILER_MAGIC) {
            if (payload_buf) free(payload_buf);
            has_incomplete_tail = 1;
            break;
        }

        // Recompute hash
        uint8_t computed_hash[ALRIOS_AUDIT_HASH_SIZE];
        int ch_rc = alrios_audit_compute_hash(hdr.sequence, hdr.timestamp, hdr.previous_hash, payload_buf, hdr.payload_size, computed_hash);
        if (payload_buf) {
            explicit_bzero(payload_buf, hdr.payload_size);
            free(payload_buf);
        }

        if (ch_rc != ALRIOS_AUDIT_OK || memcmp(computed_hash, hdr.hash, ALRIOS_AUDIT_HASH_SIZE) != 0) {
            flock(fd, LOCK_UN);
            close(fd);
            return ALRIOS_AUDIT_ERR_HASH_MISMATCH;
        }

        block_count++;
        valid_bytes += sizeof(audit_record_header_t) + hdr.payload_size + sizeof(audit_record_trailer_t);
        expected_seq = hdr.sequence + 1;
        memcpy(expected_prev_hash, hdr.hash, ALRIOS_AUDIT_HASH_SIZE);
        memcpy(out_result->last_hash, hdr.hash, ALRIOS_AUDIT_HASH_SIZE);
    }

    flock(fd, LOCK_UN);
    close(fd);

    out_result->block_count = block_count;
    out_result->valid_bytes = valid_bytes;
    out_result->has_incomplete_tail = has_incomplete_tail;

    return ALRIOS_AUDIT_OK;
}
