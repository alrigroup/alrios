#define _POSIX_C_SOURCE 200809L

/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/audit/block.h"

#include <errno.h>
#include <alrios/platform/fs_compat.h>
#include <fcntl.h>
#include <limits.h>
#include <openssl/evp.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifndef _WIN32
#include <unistd.h>
#endif

static const uint8_t audit_magic[8] = {
    0x41U, 0x4cU, 0x52U, 0x49U, 0x41U, 0x55U, 0x44U, 0x54U
};
static const uint8_t audit_hash_domain[21] = {
    0x41U, 0x4cU, 0x52U, 0x49U, 0x4fU, 0x53U, 0x2dU,
    0x41U, 0x55U, 0x44U, 0x49U, 0x54U, 0x2dU, 0x42U,
    0x4cU, 0x4fU, 0x43U, 0x4bU, 0x2dU, 0x56U, 0x31U
};

static uint16_t load_u16_be(const uint8_t *input) {
    return (uint16_t)(((uint16_t)input[0] << 8U) | (uint16_t)input[1]);
}

static uint32_t load_u32_be(const uint8_t *input) {
    return ((uint32_t)input[0] << 24U) | ((uint32_t)input[1] << 16U) |
           ((uint32_t)input[2] << 8U) | (uint32_t)input[3];
}

static uint64_t load_u64_be(const uint8_t *input) {
    uint64_t value = 0U;
    size_t index;

    for (index = 0U; index < 8U; ++index) {
        value = (value << 8U) | (uint64_t)input[index];
    }
    return value;
}

static void store_u16_be(uint8_t *output, uint16_t value) {
    output[0] = (uint8_t)(value >> 8U);
    output[1] = (uint8_t)value;
}

static void store_u32_be(uint8_t *output, uint32_t value) {
    output[0] = (uint8_t)(value >> 24U);
    output[1] = (uint8_t)(value >> 16U);
    output[2] = (uint8_t)(value >> 8U);
    output[3] = (uint8_t)value;
}

static void store_u64_be(uint8_t *output, uint64_t value) {
    size_t index;

    for (index = 8U; index > 0U; --index) {
        output[index - 1U] = (uint8_t)value;
        value >>= 8U;
    }
}

static int ranges_overlap(const void *first, size_t first_len,
                          const void *second, size_t second_len) {
    uintptr_t first_start;
    uintptr_t second_start;
    uintptr_t first_end;
    uintptr_t second_end;

    if (!first || !second || first_len == 0U || second_len == 0U) {
        return 0;
    }
    first_start = (uintptr_t)first;
    second_start = (uintptr_t)second;
    if (first_len > UINTPTR_MAX - first_start ||
        second_len > UINTPTR_MAX - second_start) {
        return 1;
    }
    first_end = first_start + first_len;
    second_end = second_start + second_len;
    return first_start < second_end && second_start < first_end;
}

static int sha256_parts(const uint8_t *first, size_t first_len,
                        const uint8_t *second, size_t second_len,
                        const uint8_t *third, size_t third_len,
                        uint8_t output[ALRIOS_AUDIT_BLOCK_HASH_SIZE]) {
    EVP_MD_CTX *context;
    unsigned int digest_len = 0U;
    int ok;

    context = EVP_MD_CTX_new();
    if (!context) {
        return ALRIOS_AUDIT_ERR_HASH;
    }
    ok = EVP_DigestInit_ex(context, EVP_sha256(), NULL) == 1;
    if (ok && first_len != 0U) {
        ok = first && EVP_DigestUpdate(context, first, first_len) == 1;
    }
    if (ok && second_len != 0U) {
        ok = second && EVP_DigestUpdate(context, second, second_len) == 1;
    }
    if (ok && third_len != 0U) {
        ok = third && EVP_DigestUpdate(context, third, third_len) == 1;
    }
    if (ok) {
        ok = EVP_DigestFinal_ex(context, output, &digest_len) == 1 &&
             digest_len == ALRIOS_AUDIT_BLOCK_HASH_SIZE;
    }
    EVP_MD_CTX_free(context);
    if (!ok) {
        memset(output, 0, ALRIOS_AUDIT_BLOCK_HASH_SIZE);
        return ALRIOS_AUDIT_ERR_HASH;
    }
    return ALRIOS_AUDIT_OK;
}

static int decode_header(const uint8_t *wire, size_t available,
                         size_t *out_total_len,
                         size_t *out_payload_len) {
    uint32_t total_len;
    uint32_t payload_len;
    size_t expected_total;

    if (!wire || !out_total_len || !out_payload_len) {
        return ALRIOS_AUDIT_ERR_INVALID_ARGUMENT;
    }
    if (available < ALRIOS_AUDIT_BLOCK_HEADER_SIZE_V1) {
        return ALRIOS_AUDIT_ERR_TRUNCATED;
    }
    if (memcmp(wire, audit_magic, sizeof(audit_magic)) != 0) {
        return ALRIOS_AUDIT_ERR_FORMAT;
    }
    if (load_u16_be(wire + 8U) != ALRIOS_AUDIT_BLOCK_VERSION_1) {
        return ALRIOS_AUDIT_ERR_VERSION;
    }
    if (load_u16_be(wire + 10U) != ALRIOS_AUDIT_BLOCK_HEADER_SIZE_V1 ||
        load_u32_be(wire + 20U) != 0U) {
        return ALRIOS_AUDIT_ERR_NONCANONICAL;
    }
    total_len = load_u32_be(wire + 12U);
    payload_len = load_u32_be(wire + 16U);
    if (payload_len > ALRIOS_AUDIT_BLOCK_PAYLOAD_MAX) {
        return ALRIOS_AUDIT_ERR_LENGTH;
    }
    expected_total = ALRIOS_AUDIT_BLOCK_WIRE_MIN_V1 + (size_t)payload_len;
    if ((size_t)total_len != expected_total ||
        total_len < ALRIOS_AUDIT_BLOCK_WIRE_MIN_V1 ||
        total_len > ALRIOS_AUDIT_BLOCK_WIRE_MAX_V1) {
        return ALRIOS_AUDIT_ERR_LENGTH;
    }
    *out_total_len = expected_total;
    *out_payload_len = payload_len;
    return ALRIOS_AUDIT_OK;
}

int alrios_audit_block_encode(
    const uint8_t previous_hash[ALRIOS_AUDIT_BLOCK_HASH_SIZE],
    uint64_t sequence,
    uint64_t timestamp_ns,
    const uint8_t *payload,
    size_t payload_len,
    uint8_t *out_wire,
    size_t out_capacity,
    size_t *out_written,
    alrios_audit_block_metadata_t *out_metadata) {
    uint8_t payload_hash[ALRIOS_AUDIT_BLOCK_HASH_SIZE];
    uint8_t block_hash[ALRIOS_AUDIT_BLOCK_HASH_SIZE];
    size_t wire_len;
    int status;

    if (out_written) {
        *out_written = 0U;
    }
    if (out_metadata) {
        memset(out_metadata, 0, sizeof(*out_metadata));
    }
    if (!previous_hash || (!payload && payload_len != 0U) || !out_wire ||
        !out_written || sequence == 0U) {
        return ALRIOS_AUDIT_ERR_INVALID_ARGUMENT;
    }
    if (payload_len > ALRIOS_AUDIT_BLOCK_PAYLOAD_MAX) {
        return ALRIOS_AUDIT_ERR_LENGTH;
    }
    wire_len = ALRIOS_AUDIT_BLOCK_WIRE_MIN_V1 + payload_len;
    if (out_capacity < wire_len) {
        return ALRIOS_AUDIT_ERR_CAPACITY;
    }
    if (ranges_overlap(previous_hash, ALRIOS_AUDIT_BLOCK_HASH_SIZE,
                       out_wire, wire_len) ||
        ranges_overlap(payload, payload_len, out_wire, wire_len) ||
        ranges_overlap(out_metadata, out_metadata ? sizeof(*out_metadata) : 0U,
                       out_wire, wire_len)) {
        return ALRIOS_AUDIT_ERR_OVERLAP;
    }
    status = sha256_parts(payload, payload_len, NULL, 0U, NULL, 0U,
                          payload_hash);
    if (status != ALRIOS_AUDIT_OK) {
        return status;
    }
    memset(out_wire, 0, ALRIOS_AUDIT_BLOCK_HEADER_SIZE_V1);
    memcpy(out_wire, audit_magic, sizeof(audit_magic));
    store_u16_be(out_wire + 8U, ALRIOS_AUDIT_BLOCK_VERSION_1);
    store_u16_be(out_wire + 10U, ALRIOS_AUDIT_BLOCK_HEADER_SIZE_V1);
    store_u32_be(out_wire + 12U, (uint32_t)wire_len);
    store_u32_be(out_wire + 16U, (uint32_t)payload_len);
    store_u64_be(out_wire + 24U, sequence);
    store_u64_be(out_wire + 32U, timestamp_ns);
    memcpy(out_wire + 40U, previous_hash, ALRIOS_AUDIT_BLOCK_HASH_SIZE);
    memcpy(out_wire + 72U, payload_hash, ALRIOS_AUDIT_BLOCK_HASH_SIZE);
    if (payload_len != 0U) {
        memcpy(out_wire + ALRIOS_AUDIT_BLOCK_HEADER_SIZE_V1, payload, payload_len);
    }
    status = sha256_parts(audit_hash_domain, sizeof(audit_hash_domain),
                          out_wire, ALRIOS_AUDIT_BLOCK_HEADER_SIZE_V1,
                          out_wire + ALRIOS_AUDIT_BLOCK_HEADER_SIZE_V1,
                          payload_len, block_hash);
    if (status != ALRIOS_AUDIT_OK) {
        memset(out_wire, 0, wire_len);
        memset(payload_hash, 0, sizeof(payload_hash));
        return status;
    }
    memcpy(out_wire + ALRIOS_AUDIT_BLOCK_HEADER_SIZE_V1 + payload_len,
           block_hash, sizeof(block_hash));
    *out_written = wire_len;
    if (out_metadata) {
        out_metadata->version = ALRIOS_AUDIT_BLOCK_VERSION_1;
        out_metadata->sequence = sequence;
        out_metadata->timestamp_ns = timestamp_ns;
        out_metadata->payload_len = (uint32_t)payload_len;
        memcpy(out_metadata->previous_hash, previous_hash,
               ALRIOS_AUDIT_BLOCK_HASH_SIZE);
        memcpy(out_metadata->payload_hash, payload_hash,
               ALRIOS_AUDIT_BLOCK_HASH_SIZE);
        memcpy(out_metadata->block_hash, block_hash,
               ALRIOS_AUDIT_BLOCK_HASH_SIZE);
    }
    memset(payload_hash, 0, sizeof(payload_hash));
    memset(block_hash, 0, sizeof(block_hash));
    return ALRIOS_AUDIT_OK;
}

int alrios_audit_block_verify(const uint8_t *wire, size_t wire_len,
                              alrios_audit_block_metadata_t *out_metadata) {
    uint8_t payload_hash[ALRIOS_AUDIT_BLOCK_HASH_SIZE];
    uint8_t block_hash[ALRIOS_AUDIT_BLOCK_HASH_SIZE];
    const uint8_t *payload;
    const uint8_t *stored_block_hash;
    size_t expected_total = 0U;
    size_t payload_len = 0U;
    int status;

    if (out_metadata) {
        memset(out_metadata, 0, sizeof(*out_metadata));
    }
    if (!wire) {
        return ALRIOS_AUDIT_ERR_INVALID_ARGUMENT;
    }
    if (out_metadata && ranges_overlap(wire, wire_len, out_metadata,
                                       sizeof(*out_metadata))) {
        return ALRIOS_AUDIT_ERR_OVERLAP;
    }
    status = decode_header(wire, wire_len, &expected_total, &payload_len);
    if (status != ALRIOS_AUDIT_OK) {
        return status;
    }
    if (wire_len != expected_total) {
        return wire_len < expected_total ? ALRIOS_AUDIT_ERR_TRUNCATED :
                                           ALRIOS_AUDIT_ERR_LENGTH;
    }
    if (load_u64_be(wire + 24U) == 0U) {
        return ALRIOS_AUDIT_ERR_SEQUENCE;
    }
    payload = wire + ALRIOS_AUDIT_BLOCK_HEADER_SIZE_V1;
    stored_block_hash = payload + payload_len;
    status = sha256_parts(payload, payload_len, NULL, 0U, NULL, 0U,
                          payload_hash);
    if (status != ALRIOS_AUDIT_OK) {
        return status;
    }
    if (memcmp(payload_hash, wire + 72U, sizeof(payload_hash)) != 0) {
        memset(payload_hash, 0, sizeof(payload_hash));
        return ALRIOS_AUDIT_ERR_HASH;
    }
    status = sha256_parts(audit_hash_domain, sizeof(audit_hash_domain),
                          wire, ALRIOS_AUDIT_BLOCK_HEADER_SIZE_V1,
                          payload, payload_len, block_hash);
    if (status != ALRIOS_AUDIT_OK ||
        memcmp(block_hash, stored_block_hash, sizeof(block_hash)) != 0) {
        memset(payload_hash, 0, sizeof(payload_hash));
        memset(block_hash, 0, sizeof(block_hash));
        return ALRIOS_AUDIT_ERR_HASH;
    }
    if (out_metadata) {
        out_metadata->version = ALRIOS_AUDIT_BLOCK_VERSION_1;
        out_metadata->sequence = load_u64_be(wire + 24U);
        out_metadata->timestamp_ns = load_u64_be(wire + 32U);
        out_metadata->payload_len = (uint32_t)payload_len;
        memcpy(out_metadata->previous_hash, wire + 40U,
               ALRIOS_AUDIT_BLOCK_HASH_SIZE);
        memcpy(out_metadata->payload_hash, payload_hash,
               ALRIOS_AUDIT_BLOCK_HASH_SIZE);
        memcpy(out_metadata->block_hash, block_hash,
               ALRIOS_AUDIT_BLOCK_HASH_SIZE);
    }
    memset(payload_hash, 0, sizeof(payload_hash));
    memset(block_hash, 0, sizeof(block_hash));
    return ALRIOS_AUDIT_OK;
}

#if defined(_WIN32)
static int lock_fd(int fd, short lock_type) {
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE) {
        return ALRIOS_AUDIT_ERR_IO;
    }
    OVERLAPPED ov;
    memset(&ov, 0, sizeof(ov));
    (void)lock_type;
    if (LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK, 0, MAXDWORD, MAXDWORD, &ov)) {
        return ALRIOS_AUDIT_OK;
    }
    return ALRIOS_AUDIT_ERR_BUSY;
}

static int unlock_fd(int fd) {
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE) {
        return ALRIOS_AUDIT_ERR_IO;
    }
    OVERLAPPED ov;
    memset(&ov, 0, sizeof(ov));
    if (UnlockFileEx(h, 0, MAXDWORD, MAXDWORD, &ov)) {
        return ALRIOS_AUDIT_OK;
    }
    return ALRIOS_AUDIT_ERR_IO;
}

static int fd_mode_valid(int fd, int require_write) {
    if (fd < 0) {
        return 0;
    }
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE) {
        return 0;
    }
    (void)require_write;
    return 1;
}
#else
static int lock_fd(int fd, short lock_type) {
    struct flock lock;

    memset(&lock, 0, sizeof(lock));
    lock.l_type = lock_type;
    lock.l_whence = SEEK_SET;
    for (;;) {
        if (fcntl(fd, F_SETLKW, &lock) == 0) {
            return ALRIOS_AUDIT_OK;
        }
        if (errno != EINTR) {
            return ALRIOS_AUDIT_ERR_BUSY;
        }
    }
}

static int unlock_fd(int fd) {
    struct flock lock;

    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    return fcntl(fd, F_SETLK, &lock) == 0 ? ALRIOS_AUDIT_OK :
                                           ALRIOS_AUDIT_ERR_IO;
}

static int fd_mode_valid(int fd, int require_write) {
    int flags;
    int access_mode;

    if (fd < 0) {
        return 0;
    }
    flags = fcntl(fd, F_GETFL);
    if (flags < 0) {
        return 0;
    }
    access_mode = flags & O_ACCMODE;
    if (require_write) {
        return access_mode == O_RDWR;
    }
    return access_mode == O_RDONLY || access_mode == O_RDWR;
}
#endif

static int read_exact_at(int fd, uint8_t *buffer, size_t length,
                         uint64_t offset) {
    size_t consumed = 0U;

    while (consumed < length) {
        uint64_t current_offset;
        size_t request;
        ssize_t result;

        if (offset > UINT64_MAX - consumed) {
            return ALRIOS_AUDIT_ERR_OVERFLOW;
        }
        current_offset = offset + consumed;
        if (current_offset > (uint64_t)INT64_MAX) {
            return ALRIOS_AUDIT_ERR_OVERFLOW;
        }
        request = length - consumed;
        if (request > (size_t)SSIZE_MAX) {
            request = (size_t)SSIZE_MAX;
        }
        result = pread(fd, buffer + consumed, request, (off_t)current_offset);
        if (result > 0) {
            consumed += (size_t)result;
        } else if (result == 0) {
            return ALRIOS_AUDIT_ERR_TRUNCATED;
        } else if (errno != EINTR) {
            return ALRIOS_AUDIT_ERR_IO;
        }
    }
    return ALRIOS_AUDIT_OK;
}

static int write_exact_at(int fd, const uint8_t *buffer, size_t length,
                          uint64_t offset) {
    size_t consumed = 0U;

    while (consumed < length) {
        uint64_t current_offset;
        size_t request;
        ssize_t result;

        if (offset > UINT64_MAX - consumed) {
            return ALRIOS_AUDIT_ERR_OVERFLOW;
        }
        current_offset = offset + consumed;
        if (current_offset > (uint64_t)INT64_MAX) {
            return ALRIOS_AUDIT_ERR_OVERFLOW;
        }
        request = length - consumed;
        if (request > (size_t)SSIZE_MAX) {
            request = (size_t)SSIZE_MAX;
        }
        result = pwrite(fd, buffer + consumed, request, (off_t)current_offset);
        if (result > 0) {
            consumed += (size_t)result;
        } else if (result == 0) {
            return ALRIOS_AUDIT_ERR_IO;
        } else if (errno != EINTR) {
            return ALRIOS_AUDIT_ERR_IO;
        }
    }
    return ALRIOS_AUDIT_OK;
}

static int sync_fd(int fd) {
    for (;;) {
        if (fsync(fd) == 0) {
            return ALRIOS_AUDIT_OK;
        }
        if (errno != EINTR) {
            return ALRIOS_AUDIT_ERR_IO;
        }
    }
}

static int truncate_fd(int fd, uint64_t length) {
    if (length > (uint64_t)INT64_MAX) {
        return ALRIOS_AUDIT_ERR_OVERFLOW;
    }
    for (;;) {
        if (ftruncate(fd, (off_t)length) == 0) {
            return ALRIOS_AUDIT_OK;
        }
        if (errno != EINTR) {
            return ALRIOS_AUDIT_ERR_IO;
        }
    }
}

static int magic_prefix(const uint8_t *bytes, size_t length) {
    return length <= sizeof(audit_magic) &&
           memcmp(bytes, audit_magic, length) == 0;
}

static int get_regular_file_size(int fd, uint64_t *out_size) {
    struct stat file_status;

    if (!out_size || fstat(fd, &file_status) != 0 ||
        !S_ISREG(file_status.st_mode) || file_status.st_size < 0) {
        return ALRIOS_AUDIT_ERR_IO;
    }
    if ((uintmax_t)file_status.st_size > (uintmax_t)INT64_MAX) {
        return ALRIOS_AUDIT_ERR_OVERFLOW;
    }
    *out_size = (uint64_t)file_status.st_size;
    return ALRIOS_AUDIT_OK;
}

static int scan_locked(int fd, int recover_tail,
                       alrios_audit_chain_result_t *out_result) {
    alrios_audit_chain_result_t result;
    alrios_audit_block_metadata_t metadata;
    uint8_t header[ALRIOS_AUDIT_BLOCK_HEADER_SIZE_V1];
    uint8_t previous_hash[ALRIOS_AUDIT_BLOCK_HASH_SIZE];
    uint8_t *record = NULL;
    uint64_t file_size = 0U;
    uint64_t offset = 0U;
    uint64_t expected_sequence = 1U;
    int status;

    memset(&result, 0, sizeof(result));
    memset(previous_hash, 0, sizeof(previous_hash));
    status = get_regular_file_size(fd, &file_size);
    if (status != ALRIOS_AUDIT_OK) {
        return status;
    }
    while (offset < file_size) {
        uint64_t remaining = file_size - offset;
        size_t total_len = 0U;
        size_t payload_len = 0U;

        if (remaining < sizeof(audit_magic)) {
            status = read_exact_at(fd, header, (size_t)remaining, offset);
            if (status != ALRIOS_AUDIT_OK) {
                goto cleanup;
            }
            if (!magic_prefix(header, (size_t)remaining)) {
                status = ALRIOS_AUDIT_ERR_FORMAT;
                goto cleanup;
            }
            status = ALRIOS_AUDIT_ERR_TRUNCATED;
            goto partial_tail;
        }
        status = read_exact_at(fd, header, sizeof(audit_magic), offset);
        if (status != ALRIOS_AUDIT_OK) {
            goto cleanup;
        }
        if (memcmp(header, audit_magic, sizeof(audit_magic)) != 0) {
            status = ALRIOS_AUDIT_ERR_FORMAT;
            goto cleanup;
        }
        if (remaining < ALRIOS_AUDIT_BLOCK_HEADER_SIZE_V1) {
            status = ALRIOS_AUDIT_ERR_TRUNCATED;
            goto partial_tail;
        }
        status = read_exact_at(fd, header, sizeof(header), offset);
        if (status != ALRIOS_AUDIT_OK) {
            goto cleanup;
        }
        status = decode_header(header, sizeof(header), &total_len, &payload_len);
        (void)payload_len;
        if (status != ALRIOS_AUDIT_OK) {
            goto cleanup;
        }
        if ((uint64_t)total_len > remaining) {
            status = ALRIOS_AUDIT_ERR_TRUNCATED;
            goto partial_tail;
        }
        record = (uint8_t *)malloc(total_len);
        if (!record) {
            status = ALRIOS_AUDIT_ERR_CAPACITY;
            goto cleanup;
        }
        status = read_exact_at(fd, record, total_len, offset);
        if (status != ALRIOS_AUDIT_OK) {
            goto cleanup;
        }
        status = alrios_audit_block_verify(record, total_len, &metadata);
        free(record);
        record = NULL;
        if (status != ALRIOS_AUDIT_OK) {
            goto cleanup;
        }
        if (metadata.sequence != expected_sequence) {
            status = ALRIOS_AUDIT_ERR_SEQUENCE;
            goto cleanup;
        }
        if (memcmp(metadata.previous_hash, previous_hash,
                   ALRIOS_AUDIT_BLOCK_HASH_SIZE) != 0) {
            status = ALRIOS_AUDIT_ERR_CHAIN;
            goto cleanup;
        }
        memcpy(previous_hash, metadata.block_hash, sizeof(previous_hash));
        if (offset > UINT64_MAX - total_len) {
            status = ALRIOS_AUDIT_ERR_OVERFLOW;
            goto cleanup;
        }
        offset += total_len;
        ++result.block_count;
        result.last_sequence = metadata.sequence;
        if (expected_sequence == UINT64_MAX && offset < file_size) {
            status = ALRIOS_AUDIT_ERR_SEQUENCE;
            goto cleanup;
        }
        ++expected_sequence;
    }
    status = ALRIOS_AUDIT_OK;
    goto success;

partial_tail:
    if (!recover_tail) {
        goto cleanup;
    }
    status = truncate_fd(fd, offset);
    if (status != ALRIOS_AUDIT_OK) {
        goto cleanup;
    }
    status = sync_fd(fd);
    if (status != ALRIOS_AUDIT_OK) {
        goto cleanup;
    }
    result.tail_recovered = 1U;

success:
    result.valid_bytes = offset;
    memcpy(result.last_hash, previous_hash, sizeof(result.last_hash));
    *out_result = result;
cleanup:
    free(record);
    memset(&metadata, 0, sizeof(metadata));
    memset(previous_hash, 0, sizeof(previous_hash));
    memset(header, 0, sizeof(header));
    return status;
}

static int chain_operation(int fd, int recover_tail,
                           alrios_audit_chain_result_t *out_result) {
    alrios_audit_chain_result_t result;
    short lock_type = recover_tail ? F_WRLCK : F_RDLCK;
    int status;
    int unlock_status;

    if (out_result) {
        memset(out_result, 0, sizeof(*out_result));
    }
    if (!out_result || !fd_mode_valid(fd, recover_tail)) {
        return ALRIOS_AUDIT_ERR_INVALID_ARGUMENT;
    }
    status = lock_fd(fd, lock_type);
    if (status != ALRIOS_AUDIT_OK) {
        return status;
    }
    status = scan_locked(fd, recover_tail, &result);
    unlock_status = unlock_fd(fd);
    if (status == ALRIOS_AUDIT_OK && unlock_status != ALRIOS_AUDIT_OK) {
        status = unlock_status;
    }
    if (status == ALRIOS_AUDIT_OK) {
        *out_result = result;
    }
    return status;
}

int alrios_audit_chain_verify_fd(int fd,
                                 alrios_audit_chain_result_t *out_result) {
    return chain_operation(fd, 0, out_result);
}

int alrios_audit_chain_recover_fd(int fd,
                                  alrios_audit_chain_result_t *out_result) {
    return chain_operation(fd, 1, out_result);
}

int alrios_audit_chain_append_fd(int fd, uint64_t timestamp_ns,
                                 const uint8_t *payload, size_t payload_len,
                                 alrios_audit_block_metadata_t *out_metadata) {
    alrios_audit_chain_result_t chain;
    alrios_audit_block_metadata_t metadata;
    uint8_t *wire = NULL;
    size_t wire_len = 0U;
    uint64_t sequence;
    int status;
    int unlock_status;

    if (out_metadata) {
        memset(out_metadata, 0, sizeof(*out_metadata));
    }
    if (!out_metadata || (!payload && payload_len != 0U) ||
        payload_len > ALRIOS_AUDIT_BLOCK_PAYLOAD_MAX ||
        !fd_mode_valid(fd, 1)) {
        return ALRIOS_AUDIT_ERR_INVALID_ARGUMENT;
    }
    status = lock_fd(fd, F_WRLCK);
    if (status != ALRIOS_AUDIT_OK) {
        return status;
    }
    status = scan_locked(fd, 1, &chain);
    if (status != ALRIOS_AUDIT_OK) {
        goto unlock;
    }
    if (chain.last_sequence == UINT64_MAX ||
        chain.valid_bytes > (uint64_t)INT64_MAX - ALRIOS_AUDIT_BLOCK_WIRE_MIN_V1 ||
        payload_len > (size_t)((uint64_t)INT64_MAX - chain.valid_bytes -
                               ALRIOS_AUDIT_BLOCK_WIRE_MIN_V1)) {
        status = ALRIOS_AUDIT_ERR_OVERFLOW;
        goto unlock;
    }
    sequence = chain.block_count == 0U ? 1U : chain.last_sequence + 1U;
    wire = (uint8_t *)malloc(ALRIOS_AUDIT_BLOCK_WIRE_MIN_V1 + payload_len);
    if (!wire) {
        status = ALRIOS_AUDIT_ERR_CAPACITY;
        goto unlock;
    }
    status = alrios_audit_block_encode(chain.last_hash, sequence, timestamp_ns,
                                       payload, payload_len, wire,
                                       ALRIOS_AUDIT_BLOCK_WIRE_MIN_V1 + payload_len,
                                       &wire_len, &metadata);
    if (status != ALRIOS_AUDIT_OK) {
        goto unlock;
    }
    status = write_exact_at(fd, wire, wire_len, chain.valid_bytes);
    if (status == ALRIOS_AUDIT_OK) {
        status = sync_fd(fd);
    }
    if (status != ALRIOS_AUDIT_OK) {
        int rollback_status = truncate_fd(fd, chain.valid_bytes);
        if (rollback_status == ALRIOS_AUDIT_OK) {
            rollback_status = sync_fd(fd);
        }
        if (rollback_status != ALRIOS_AUDIT_OK) {
            status = rollback_status;
        }
        goto unlock;
    }
    *out_metadata = metadata;

unlock:
    free(wire);
    memset(&metadata, 0, sizeof(metadata));
    unlock_status = unlock_fd(fd);
    if (status == ALRIOS_AUDIT_OK && unlock_status != ALRIOS_AUDIT_OK) {
        memset(out_metadata, 0, sizeof(*out_metadata));
        status = unlock_status;
    }
    return status;
}
