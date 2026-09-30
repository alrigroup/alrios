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
#include <limits.h>
#include <openssl/evp.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sched.h>
#include <unistd.h>
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

#define AUDIT_HEADER_MAGIC_SIZE 8U
#define AUDIT_TRAILER_MAGIC_SIZE 8U
#define AUDIT_HEADER_PAYLOAD_SIZE_OFFSET 12U
#define AUDIT_HEADER_SEQUENCE_OFFSET 16U
#define AUDIT_HEADER_TIMESTAMP_OFFSET 24U
#define AUDIT_HEADER_PREVIOUS_HASH_OFFSET 32U
#define AUDIT_HEADER_HASH_OFFSET 64U
#define AUDIT_TRAILER_HASH_OFFSET 8U
#define AUDIT_IO_CHUNK_SIZE 8192U

#ifdef _WIN32
typedef int audit_fd_t;
typedef __int64 audit_offset_t;
#define audit_close _close
#define audit_fstat _fstat64
#define audit_open _open
#define audit_read _read
#define audit_seek _lseeki64
#define audit_stat_t struct _stat64
#define audit_write _write
#else
typedef int audit_fd_t;
typedef off_t audit_offset_t;
#define audit_close close
#define audit_fstat fstat
#define audit_open open
#define audit_read read
#define audit_seek lseek
#define audit_stat_t struct stat
#define audit_write write
#endif

static const uint8_t audit_header_magic[AUDIT_HEADER_MAGIC_SIZE] = {
    'A', 'L', 'R', 'I', 'A', 'U', 'D', '1'
};
static const uint8_t audit_trailer_magic[AUDIT_TRAILER_MAGIC_SIZE] = {
    'A', 'L', 'R', 'I', 'C', 'M', 'T', '1'
};
static const uint8_t audit_hash_domain[] = "ALRIOS-AUDIT-BLOCK-V1";

static atomic_flag audit_process_lock = ATOMIC_FLAG_INIT;

static int process_lock_acquire(void) {
    while (atomic_flag_test_and_set_explicit(&audit_process_lock,
                                              memory_order_acquire)) {
#ifdef _WIN32
        Sleep(0U);
#else
        (void)sched_yield();
#endif
    }
    return ALRIOS_AUDIT_OK;
}

static int process_lock_release(void) {
    atomic_flag_clear_explicit(&audit_process_lock, memory_order_release);
    return ALRIOS_AUDIT_OK;
}

static uint16_t load_u16_be(const uint8_t *bytes) {
    return (uint16_t)(((uint16_t)bytes[0] << 8U) | (uint16_t)bytes[1]);
}

static uint32_t load_u32_be(const uint8_t *bytes) {
    return ((uint32_t)bytes[0] << 24U) |
           ((uint32_t)bytes[1] << 16U) |
           ((uint32_t)bytes[2] << 8U) |
           (uint32_t)bytes[3];
}

static uint64_t load_u64_be(const uint8_t *bytes) {
    uint64_t value = 0U;
    size_t index;

    for (index = 0U; index < 8U; ++index) {
        value = (value << 8U) | (uint64_t)bytes[index];
    }
    return value;
}

static void store_u16_be(uint8_t *bytes, uint16_t value) {
    bytes[0] = (uint8_t)(value >> 8U);
    bytes[1] = (uint8_t)value;
}

static void store_u32_be(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)(value >> 24U);
    bytes[1] = (uint8_t)(value >> 16U);
    bytes[2] = (uint8_t)(value >> 8U);
    bytes[3] = (uint8_t)value;
}

static void store_u64_be(uint8_t *bytes, uint64_t value) {
    size_t index;

    for (index = 8U; index > 0U; --index) {
        bytes[index - 1U] = (uint8_t)value;
        value >>= 8U;
    }
}

static int hashes_equal(const uint8_t *left, const uint8_t *right) {
    uint8_t difference = 0U;
    size_t index;

    for (index = 0U; index < ALRIOS_AUDIT_HASH_SIZE; ++index) {
        difference |= (uint8_t)(left[index] ^ right[index]);
    }
    return difference == 0U;
}

static int offset_from_u64(uint64_t value, audit_offset_t *out_offset) {
    audit_offset_t converted;

    if (!out_offset) {
        return ALRIOS_AUDIT_ERR_INVALID_ARGUMENT;
    }
    converted = (audit_offset_t)value;
    if (converted < 0 || (uint64_t)converted != value) {
        return ALRIOS_AUDIT_ERR_OVERFLOW;
    }
    *out_offset = converted;
    return ALRIOS_AUDIT_OK;
}

static int lock_fd(audit_fd_t fd, int exclusive) {
#ifdef _WIN32
    HANDLE handle = (HANDLE)_get_osfhandle(fd);
    OVERLAPPED overlapped;
    DWORD flags = exclusive != 0 ? LOCKFILE_EXCLUSIVE_LOCK : 0U;

    if (handle == INVALID_HANDLE_VALUE) {
        return ALRIOS_AUDIT_ERR_LOCK;
    }
    memset(&overlapped, 0, sizeof(overlapped));
    return LockFileEx(handle, flags, 0U, MAXDWORD, MAXDWORD, &overlapped) != 0
               ? ALRIOS_AUDIT_OK
               : ALRIOS_AUDIT_ERR_LOCK;
#else
    struct flock lock;
    int result;

    memset(&lock, 0, sizeof(lock));
    lock.l_type = exclusive != 0 ? F_WRLCK : F_RDLCK;
    lock.l_whence = SEEK_SET;
    do {
        result = fcntl(fd, F_SETLKW, &lock);
    } while (result < 0 && errno == EINTR);
    return result == 0 ? ALRIOS_AUDIT_OK : ALRIOS_AUDIT_ERR_LOCK;
#endif
}

static int unlock_fd(audit_fd_t fd) {
#ifdef _WIN32
    HANDLE handle = (HANDLE)_get_osfhandle(fd);
    OVERLAPPED overlapped;

    if (handle == INVALID_HANDLE_VALUE) {
        return ALRIOS_AUDIT_ERR_LOCK;
    }
    memset(&overlapped, 0, sizeof(overlapped));
    return UnlockFileEx(handle, 0U, MAXDWORD, MAXDWORD, &overlapped) != 0
               ? ALRIOS_AUDIT_OK
               : ALRIOS_AUDIT_ERR_LOCK;
#else
    struct flock lock;
    int result;

    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    do {
        result = fcntl(fd, F_SETLK, &lock);
    } while (result < 0 && errno == EINTR);
    return result == 0 ? ALRIOS_AUDIT_OK : ALRIOS_AUDIT_ERR_LOCK;
#endif
}

static int seek_fd(audit_fd_t fd, uint64_t offset) {
    audit_offset_t converted;
    int status;

    status = offset_from_u64(offset, &converted);
    if (status != ALRIOS_AUDIT_OK) {
        return status;
    }
    if (audit_seek(fd, converted, SEEK_SET) < 0) {
        return ALRIOS_AUDIT_ERR_IO;
    }
    return ALRIOS_AUDIT_OK;
}

static int read_exact_at(audit_fd_t fd,
                         uint64_t offset,
                         uint8_t *buffer,
                         size_t length) {
    size_t consumed = 0U;
    int status;

    status = seek_fd(fd, offset);
    if (status != ALRIOS_AUDIT_OK) {
        return status;
    }
    while (consumed < length) {
#ifdef _WIN32
        size_t request_size = length - consumed;
        int count;

        if (request_size > (size_t)INT_MAX) {
            request_size = (size_t)INT_MAX;
        }
        count = audit_read(fd, buffer + consumed, (unsigned int)request_size);
#else
        ssize_t count = audit_read(fd, buffer + consumed, length - consumed);
#endif
        if (count == 0) {
            return ALRIOS_AUDIT_ERR_TRUNCATED;
        }
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            return ALRIOS_AUDIT_ERR_IO;
        }
        consumed += (size_t)count;
    }
    return ALRIOS_AUDIT_OK;
}

static int write_exact(audit_fd_t fd, const uint8_t *buffer, size_t length) {
    size_t written = 0U;

    while (written < length) {
#ifdef _WIN32
        size_t request_size = length - written;
        int count;

        if (request_size > (size_t)INT_MAX) {
            request_size = (size_t)INT_MAX;
        }
        count = audit_write(fd, buffer + written, (unsigned int)request_size);
#else
        ssize_t count = audit_write(fd, buffer + written, length - written);
#endif
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            return ALRIOS_AUDIT_ERR_IO;
        }
        if (count == 0) {
            return ALRIOS_AUDIT_ERR_IO;
        }
        written += (size_t)count;
    }
    return ALRIOS_AUDIT_OK;
}

static int truncate_fd(audit_fd_t fd, uint64_t length) {
    audit_offset_t converted;
    int status;

    status = offset_from_u64(length, &converted);
    if (status != ALRIOS_AUDIT_OK) {
        return status;
    }
#ifdef _WIN32
    return _chsize_s(fd, converted) == 0
               ? ALRIOS_AUDIT_OK
               : ALRIOS_AUDIT_ERR_IO;
#else
    return ftruncate(fd, converted) == 0
               ? ALRIOS_AUDIT_OK
               : ALRIOS_AUDIT_ERR_IO;
#endif
}

static int sync_fd(audit_fd_t fd) {
#ifdef _WIN32
    HANDLE handle;

    if (_commit(fd) != 0) {
        return ALRIOS_AUDIT_ERR_IO;
    }
    handle = (HANDLE)_get_osfhandle(fd);
    if (handle == INVALID_HANDLE_VALUE || FlushFileBuffers(handle) == 0) {
        return ALRIOS_AUDIT_ERR_IO;
    }
    return ALRIOS_AUDIT_OK;
#else
    return fsync(fd) == 0 ? ALRIOS_AUDIT_OK : ALRIOS_AUDIT_ERR_IO;
#endif
}

static int fd_size(audit_fd_t fd, uint64_t *out_size) {
    audit_stat_t file_status;

    if (audit_fstat(fd, &file_status) != 0 || file_status.st_size < 0) {
        return ALRIOS_AUDIT_ERR_IO;
    }
#ifdef _WIN32
    if ((file_status.st_mode & _S_IFREG) == 0) {
        return ALRIOS_AUDIT_ERR_NOT_REGULAR;
    }
#else
    if (!S_ISREG(file_status.st_mode)) {
        return ALRIOS_AUDIT_ERR_NOT_REGULAR;
    }
#endif
    *out_size = (uint64_t)file_status.st_size;
    return ALRIOS_AUDIT_OK;
}

static int digest_update(EVP_MD_CTX *context, const void *data, size_t length) {
    if (length == 0U) {
        return ALRIOS_AUDIT_OK;
    }
    return EVP_DigestUpdate(context, data, length) == 1
               ? ALRIOS_AUDIT_OK
               : ALRIOS_AUDIT_ERR_UNAVAILABLE;
}

static int digest_header(EVP_MD_CTX *context,
                         const uint8_t header[ALRIOS_AUDIT_RECORD_HEADER_SIZE]) {
    return digest_update(context, header, AUDIT_HEADER_HASH_OFFSET);
}

static int compute_hash_parts(
    const uint8_t header[ALRIOS_AUDIT_RECORD_HEADER_SIZE],
    const uint8_t *payload,
    size_t payload_size,
    uint8_t out_hash[ALRIOS_AUDIT_HASH_SIZE]) {
    EVP_MD_CTX *context;
    unsigned int digest_size = 0U;
    int status = ALRIOS_AUDIT_ERR_UNAVAILABLE;

    context = EVP_MD_CTX_new();
    if (!context) {
        return ALRIOS_AUDIT_ERR_UNAVAILABLE;
    }
    if (EVP_DigestInit_ex(context, EVP_sha256(), NULL) != 1 ||
        digest_update(context, audit_hash_domain, sizeof(audit_hash_domain) - 1U) !=
            ALRIOS_AUDIT_OK ||
        digest_header(context, header) != ALRIOS_AUDIT_OK ||
        digest_update(context, payload, payload_size) != ALRIOS_AUDIT_OK ||
        EVP_DigestFinal_ex(context, out_hash, &digest_size) != 1 ||
        digest_size != ALRIOS_AUDIT_HASH_SIZE) {
        memset(out_hash, 0, ALRIOS_AUDIT_HASH_SIZE);
        goto cleanup;
    }
    status = ALRIOS_AUDIT_OK;

cleanup:
    EVP_MD_CTX_free(context);
    return status;
}

static void encode_header(uint8_t header[ALRIOS_AUDIT_RECORD_HEADER_SIZE],
                          uint64_t sequence,
                          uint64_t timestamp,
                          uint32_t payload_size,
                          const uint8_t previous_hash[ALRIOS_AUDIT_HASH_SIZE]) {
    memset(header, 0, ALRIOS_AUDIT_RECORD_HEADER_SIZE);
    memcpy(header, audit_header_magic, AUDIT_HEADER_MAGIC_SIZE);
    store_u16_be(header + 8U, ALRIOS_AUDIT_FORMAT_VERSION);
    store_u16_be(header + 10U, ALRIOS_AUDIT_RECORD_HEADER_SIZE);
    store_u32_be(header + AUDIT_HEADER_PAYLOAD_SIZE_OFFSET, payload_size);
    store_u64_be(header + AUDIT_HEADER_SEQUENCE_OFFSET, sequence);
    store_u64_be(header + AUDIT_HEADER_TIMESTAMP_OFFSET, timestamp);
    memcpy(header + AUDIT_HEADER_PREVIOUS_HASH_OFFSET,
           previous_hash,
           ALRIOS_AUDIT_HASH_SIZE);
}

static int validate_partial_header(audit_fd_t fd,
                                   uint64_t offset,
                                   uint64_t length) {
    uint8_t partial[ALRIOS_AUDIT_RECORD_HEADER_SIZE];
    uint8_t expected_prefix[12U];
    size_t compare_size;
    int status;

    if (length == 0U || length >= ALRIOS_AUDIT_RECORD_HEADER_SIZE) {
        return ALRIOS_AUDIT_ERR_FORMAT;
    }
    status = read_exact_at(fd, offset, partial, (size_t)length);
    if (status != ALRIOS_AUDIT_OK) {
        return status;
    }
    memcpy(expected_prefix, audit_header_magic, AUDIT_HEADER_MAGIC_SIZE);
    store_u16_be(expected_prefix + 8U, ALRIOS_AUDIT_FORMAT_VERSION);
    store_u16_be(expected_prefix + 10U, ALRIOS_AUDIT_RECORD_HEADER_SIZE);
    compare_size = (size_t)length < sizeof(expected_prefix)
                       ? (size_t)length
                       : sizeof(expected_prefix);
    return memcmp(partial, expected_prefix, compare_size) == 0
               ? ALRIOS_AUDIT_OK
               : ALRIOS_AUDIT_ERR_FORMAT;
}

static int validate_header(
    const uint8_t header[ALRIOS_AUDIT_RECORD_HEADER_SIZE],
    uint32_t *payload_size,
    uint64_t *sequence,
    uint8_t previous_hash[ALRIOS_AUDIT_HASH_SIZE]) {
    uint32_t decoded_payload_size;

    if (memcmp(header, audit_header_magic, AUDIT_HEADER_MAGIC_SIZE) != 0 ||
        load_u16_be(header + 8U) != ALRIOS_AUDIT_FORMAT_VERSION ||
        load_u16_be(header + 10U) != ALRIOS_AUDIT_RECORD_HEADER_SIZE) {
        return ALRIOS_AUDIT_ERR_FORMAT;
    }
    decoded_payload_size = load_u32_be(header + AUDIT_HEADER_PAYLOAD_SIZE_OFFSET);
    if (decoded_payload_size > ALRIOS_AUDIT_MAX_PAYLOAD_SIZE) {
        return ALRIOS_AUDIT_ERR_FORMAT;
    }
    *payload_size = decoded_payload_size;
    *sequence = load_u64_be(header + AUDIT_HEADER_SEQUENCE_OFFSET);
    memcpy(previous_hash,
           header + AUDIT_HEADER_PREVIOUS_HASH_OFFSET,
           ALRIOS_AUDIT_HASH_SIZE);
    return ALRIOS_AUDIT_OK;
}

static int verify_fd(audit_fd_t fd, alrios_audit_verify_result_t *out_result) {
    uint8_t header[ALRIOS_AUDIT_RECORD_HEADER_SIZE];
    uint8_t trailer[ALRIOS_AUDIT_RECORD_TRAILER_SIZE];
    uint8_t expected_previous_hash[ALRIOS_AUDIT_HASH_SIZE] = {0};
    uint8_t computed_hash[ALRIOS_AUDIT_HASH_SIZE];
    uint8_t payload_buffer[AUDIT_IO_CHUNK_SIZE];
    uint64_t file_size;
    uint64_t offset = 0U;
    uint64_t block_count = 0U;
    int status;

    memset(out_result, 0, sizeof(*out_result));
    status = fd_size(fd, &file_size);
    if (status != ALRIOS_AUDIT_OK) {
        return status;
    }

    while (offset < file_size) {
        uint8_t previous_hash[ALRIOS_AUDIT_HASH_SIZE];
        uint32_t payload_size;
        uint64_t sequence;
        uint64_t record_size;
        uint64_t payload_offset;
        uint64_t trailer_offset;
        size_t remaining;
        EVP_MD_CTX *context;
        unsigned int digest_size = 0U;

        if (file_size - offset < ALRIOS_AUDIT_RECORD_HEADER_SIZE) {
            status = validate_partial_header(fd, offset, file_size - offset);
            if (status != ALRIOS_AUDIT_OK) {
                return status;
            }
            out_result->has_incomplete_tail = 1;
            break;
        }
        status = read_exact_at(fd, offset, header, sizeof(header));
        if (status != ALRIOS_AUDIT_OK) {
            return status;
        }
        status = validate_header(header, &payload_size, &sequence, previous_hash);
        if (status != ALRIOS_AUDIT_OK) {
            return status;
        }
        if (sequence != block_count ||
            !hashes_equal(previous_hash, expected_previous_hash)) {
            return ALRIOS_AUDIT_ERR_SEQUENCE;
        }

        record_size = (uint64_t)ALRIOS_AUDIT_RECORD_OVERHEAD +
                      (uint64_t)payload_size;
        if (record_size > UINT64_MAX - offset) {
            return ALRIOS_AUDIT_ERR_OVERFLOW;
        }
        if (record_size > file_size - offset) {
            uint64_t available_after_header =
                file_size - offset - ALRIOS_AUDIT_RECORD_HEADER_SIZE;

            if (available_after_header >= (uint64_t)payload_size) {
                trailer_offset = offset + ALRIOS_AUDIT_RECORD_HEADER_SIZE +
                                 (uint64_t)payload_size;
                status = read_exact_at(fd,
                                       trailer_offset,
                                       trailer,
                                       (size_t)(file_size - trailer_offset));
                if (status != ALRIOS_AUDIT_OK) {
                    return status;
                }
                if (memcmp(trailer,
                           audit_trailer_magic,
                           (size_t)(file_size - trailer_offset)) != 0) {
                    return ALRIOS_AUDIT_ERR_FORMAT;
                }
            }
            out_result->has_incomplete_tail = 1;
            break;
        }

        context = EVP_MD_CTX_new();
        if (!context) {
            return ALRIOS_AUDIT_ERR_UNAVAILABLE;
        }
        if (EVP_DigestInit_ex(context, EVP_sha256(), NULL) != 1 ||
            digest_update(context, audit_hash_domain,
                          sizeof(audit_hash_domain) - 1U) != ALRIOS_AUDIT_OK ||
            digest_header(context, header) != ALRIOS_AUDIT_OK) {
            EVP_MD_CTX_free(context);
            return ALRIOS_AUDIT_ERR_UNAVAILABLE;
        }

        payload_offset = offset + ALRIOS_AUDIT_RECORD_HEADER_SIZE;
        remaining = (size_t)payload_size;
        while (remaining > 0U) {
            size_t chunk_size = remaining < sizeof(payload_buffer)
                                    ? remaining
                                    : sizeof(payload_buffer);
            status = read_exact_at(fd, payload_offset, payload_buffer, chunk_size);
            if (status != ALRIOS_AUDIT_OK ||
                digest_update(context, payload_buffer, chunk_size) !=
                    ALRIOS_AUDIT_OK) {
                EVP_MD_CTX_free(context);
                return status != ALRIOS_AUDIT_OK
                           ? status
                           : ALRIOS_AUDIT_ERR_UNAVAILABLE;
            }
            payload_offset += (uint64_t)chunk_size;
            remaining -= chunk_size;
        }
        if (EVP_DigestFinal_ex(context, computed_hash, &digest_size) != 1 ||
            digest_size != ALRIOS_AUDIT_HASH_SIZE) {
            EVP_MD_CTX_free(context);
            return ALRIOS_AUDIT_ERR_UNAVAILABLE;
        }
        EVP_MD_CTX_free(context);

        trailer_offset = offset + ALRIOS_AUDIT_RECORD_HEADER_SIZE +
                         (uint64_t)payload_size;
        status = read_exact_at(fd, trailer_offset, trailer, sizeof(trailer));
        if (status != ALRIOS_AUDIT_OK) {
            return status;
        }
        if (memcmp(trailer, audit_trailer_magic, AUDIT_TRAILER_MAGIC_SIZE) != 0) {
            return ALRIOS_AUDIT_ERR_FORMAT;
        }
        if (!hashes_equal(computed_hash, header + AUDIT_HEADER_HASH_OFFSET) ||
            !hashes_equal(computed_hash, trailer + AUDIT_TRAILER_HASH_OFFSET)) {
            return ALRIOS_AUDIT_ERR_HASH_MISMATCH;
        }

        memcpy(expected_previous_hash, computed_hash, ALRIOS_AUDIT_HASH_SIZE);
        offset += record_size;
        block_count++;
        out_result->block_count = block_count;
        out_result->valid_bytes = offset;
        memcpy(out_result->last_hash, computed_hash, ALRIOS_AUDIT_HASH_SIZE);
    }
    return ALRIOS_AUDIT_OK;
}

static int open_append_file(const char *path, audit_fd_t *out_fd) {
    audit_fd_t fd;

#ifdef _WIN32
    errno_t open_status = _sopen_s(&fd,
                                   path,
                                   _O_RDWR | _O_CREAT | _O_BINARY,
                                   _SH_DENYNO,
                                   _S_IREAD | _S_IWRITE);
    if (open_status != 0 || fd < 0) {
        return ALRIOS_AUDIT_ERR_IO;
    }
#else
    fd = audit_open(path, O_RDWR | O_CREAT | O_CLOEXEC, (mode_t)0600);
    if (fd < 0) {
        return ALRIOS_AUDIT_ERR_IO;
    }
#endif
    *out_fd = fd;
    return ALRIOS_AUDIT_OK;
}

static int open_verify_file(const char *path, audit_fd_t *out_fd) {
    audit_fd_t fd;

#ifdef _WIN32
    errno_t open_status = _sopen_s(&fd,
                                   path,
                                   _O_RDONLY | _O_BINARY,
                                   _SH_DENYNO,
                                   _S_IREAD);
    if (open_status != 0 || fd < 0) {
        return ALRIOS_AUDIT_ERR_IO;
    }
#else
    fd = audit_open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return ALRIOS_AUDIT_ERR_IO;
    }
#endif
    *out_fd = fd;
    return ALRIOS_AUDIT_OK;
}

static int close_locked_file(audit_fd_t fd, int status) {
    int unlock_status = unlock_fd(fd);

    if (status == ALRIOS_AUDIT_OK && unlock_status != ALRIOS_AUDIT_OK) {
        status = unlock_status;
    }
    if (audit_close(fd) != 0 && status == ALRIOS_AUDIT_OK) {
        status = ALRIOS_AUDIT_ERR_IO;
    }
    return status;
}

int alrios_audit_compute_hash(uint64_t sequence,
                              uint64_t timestamp,
                              const uint8_t previous_hash[ALRIOS_AUDIT_HASH_SIZE],
                              const uint8_t *payload,
                              size_t payload_size,
                              uint8_t out_hash[ALRIOS_AUDIT_HASH_SIZE]) {
    uint8_t header[ALRIOS_AUDIT_RECORD_HEADER_SIZE];

    if (!previous_hash || !out_hash || (payload_size > 0U && !payload)) {
        return ALRIOS_AUDIT_ERR_INVALID_ARGUMENT;
    }
    if (payload_size > ALRIOS_AUDIT_MAX_PAYLOAD_SIZE || payload_size > UINT32_MAX) {
        return ALRIOS_AUDIT_ERR_OVERFLOW;
    }
    encode_header(header, sequence, timestamp, (uint32_t)payload_size,
                  previous_hash);
    return compute_hash_parts(header, payload, payload_size, out_hash);
}

int alrios_audit_append(const char *path,
                        uint64_t timestamp,
                        const uint8_t *payload,
                        size_t payload_size,
                        alrios_audit_block_t *out_block) {
    uint8_t header[ALRIOS_AUDIT_RECORD_HEADER_SIZE];
    uint8_t trailer[ALRIOS_AUDIT_RECORD_TRAILER_SIZE];
    uint8_t *record = NULL;
    alrios_audit_verify_result_t verification;
    alrios_audit_block_t block;
    audit_fd_t fd = -1;
    uint64_t record_size_u64;
    size_t record_size;
    int status;
    int process_unlock_status;

    if (!path || path[0] == '\0' || (payload_size > 0U && !payload)) {
        return ALRIOS_AUDIT_ERR_INVALID_ARGUMENT;
    }
    if (payload_size > ALRIOS_AUDIT_MAX_PAYLOAD_SIZE || payload_size > UINT32_MAX) {
        return ALRIOS_AUDIT_ERR_OVERFLOW;
    }
    if (payload_size > SIZE_MAX - ALRIOS_AUDIT_RECORD_OVERHEAD) {
        return ALRIOS_AUDIT_ERR_OVERFLOW;
    }
    record_size = ALRIOS_AUDIT_RECORD_OVERHEAD + payload_size;
    record_size_u64 = (uint64_t)record_size;

    status = process_lock_acquire();
    if (status != ALRIOS_AUDIT_OK) {
        return status;
    }
    status = open_append_file(path, &fd);
    if (status != ALRIOS_AUDIT_OK) {
        goto process_cleanup;
    }
    status = lock_fd(fd, 1);
    if (status != ALRIOS_AUDIT_OK) {
        if (audit_close(fd) != 0) {
            status = ALRIOS_AUDIT_ERR_IO;
        }
        goto process_cleanup;
    }

    status = verify_fd(fd, &verification);
    if (status != ALRIOS_AUDIT_OK) {
        goto file_cleanup;
    }
    if (verification.has_incomplete_tail) {
        status = truncate_fd(fd, verification.valid_bytes);
        if (status != ALRIOS_AUDIT_OK) {
            goto file_cleanup;
        }
        status = sync_fd(fd);
        if (status != ALRIOS_AUDIT_OK) {
            goto file_cleanup;
        }
    }
    if (verification.block_count == UINT64_MAX ||
        verification.valid_bytes > UINT64_MAX - record_size_u64) {
        status = ALRIOS_AUDIT_ERR_OVERFLOW;
        goto file_cleanup;
    }

    memset(&block, 0, sizeof(block));
    block.sequence = verification.block_count;
    block.timestamp = timestamp;
    block.payload_size = (uint32_t)payload_size;
    memcpy(block.previous_hash, verification.last_hash, ALRIOS_AUDIT_HASH_SIZE);
    encode_header(header, block.sequence, block.timestamp, block.payload_size,
                  block.previous_hash);
    status = compute_hash_parts(header, payload, payload_size, block.hash);
    if (status != ALRIOS_AUDIT_OK) {
        goto file_cleanup;
    }
    memcpy(header + AUDIT_HEADER_HASH_OFFSET, block.hash,
           ALRIOS_AUDIT_HASH_SIZE);
    memcpy(trailer, audit_trailer_magic, AUDIT_TRAILER_MAGIC_SIZE);
    memcpy(trailer + AUDIT_TRAILER_HASH_OFFSET, block.hash,
           ALRIOS_AUDIT_HASH_SIZE);

    record = (uint8_t *)malloc(record_size);
    if (!record) {
        status = ALRIOS_AUDIT_ERR_UNAVAILABLE;
        goto file_cleanup;
    }
    memcpy(record, header, sizeof(header));
    if (payload_size > 0U) {
        memcpy(record + sizeof(header), payload, payload_size);
    }
    memcpy(record + sizeof(header) + payload_size, trailer, sizeof(trailer));

    status = seek_fd(fd, verification.valid_bytes);
    if (status == ALRIOS_AUDIT_OK) {
        status = write_exact(fd, record, record_size);
    }
    if (status != ALRIOS_AUDIT_OK) {
        int truncate_status = truncate_fd(fd, verification.valid_bytes);
        if (truncate_status == ALRIOS_AUDIT_OK) {
            truncate_status = sync_fd(fd);
        }
        if (truncate_status != ALRIOS_AUDIT_OK) {
            status = truncate_status;
        }
        goto file_cleanup;
    }
    status = sync_fd(fd);
    if (status == ALRIOS_AUDIT_OK && out_block) {
        *out_block = block;
    }

file_cleanup:
    free(record);
    status = close_locked_file(fd, status);
process_cleanup:
    process_unlock_status = process_lock_release();
    if (status == ALRIOS_AUDIT_OK &&
        process_unlock_status != ALRIOS_AUDIT_OK) {
        status = process_unlock_status;
    }
    return status;
}

int alrios_audit_verify(const char *path,
                        alrios_audit_verify_result_t *out_result) {
    audit_fd_t fd = -1;
    int status;
    int process_unlock_status;

    if (!path || path[0] == '\0' || !out_result) {
        return ALRIOS_AUDIT_ERR_INVALID_ARGUMENT;
    }
    memset(out_result, 0, sizeof(*out_result));
    status = process_lock_acquire();
    if (status != ALRIOS_AUDIT_OK) {
        return status;
    }
    status = open_verify_file(path, &fd);
    if (status != ALRIOS_AUDIT_OK) {
        goto process_cleanup;
    }
    status = lock_fd(fd, 0);
    if (status != ALRIOS_AUDIT_OK) {
        if (audit_close(fd) != 0) {
            status = ALRIOS_AUDIT_ERR_IO;
        }
        goto process_cleanup;
    }
    status = verify_fd(fd, out_result);
    status = close_locked_file(fd, status);

process_cleanup:
    process_unlock_status = process_lock_release();
    if (status == ALRIOS_AUDIT_OK &&
        process_unlock_status != ALRIOS_AUDIT_OK) {
        status = process_unlock_status;
    }
    return status;
}
