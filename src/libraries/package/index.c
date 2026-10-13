/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/package/index.h"

#include <errno.h>
#include <fcntl.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

#define INDEX_SIGNED_HEADER_SIZE 80U
#define INDEX_SIGNATURE_OFFSET 80U
#define INDEX_ENTRY_PATH_LENGTH_OFFSET 0U
#define INDEX_ENTRY_MODE_OFFSET 4U
#define INDEX_ENTRY_FILE_SIZE_OFFSET 8U
#define INDEX_ENTRY_SHA256_OFFSET 16U
#define INDEX_ENTRY_RESERVED_OFFSET 48U

static uint16_t read_u16_be(const uint8_t *input) {
    return (uint16_t)(((uint16_t)input[0] << 8U) | input[1]);
}

static uint32_t read_u32_be(const uint8_t *input) {
    return ((uint32_t)input[0] << 24U) |
           ((uint32_t)input[1] << 16U) |
           ((uint32_t)input[2] << 8U) |
           (uint32_t)input[3];
}

static uint64_t read_u64_be(const uint8_t *input) {
    return ((uint64_t)read_u32_be(input) << 32U) |
           read_u32_be(input + 4U);
}

static int checked_add_size(size_t left, size_t right, size_t *out) {
    if (!out || right > SIZE_MAX - left) {
        return 0;
    }
    *out = left + right;
    return 1;
}

static int all_zero(const uint8_t *data, size_t size) {
    uint8_t accumulator = 0U;
    size_t index;

    for (index = 0U; index < size; ++index) {
        accumulator |= data[index];
    }
    return accumulator == 0U;
}

static int valid_path(const char *path, size_t path_size) {
    size_t segment_start = 0U;
    size_t index;

    if (!path || path_size == 0U || path_size >= ALRIOS_PACKAGE_INDEX_MAX_PATH_SIZE ||
        path[0] == '/' || path[0] == '\\' || path[path_size - 1U] == '/' ||
        path[path_size - 1U] == '\\' || memchr(path, '\0', path_size) != NULL) {
        return 0;
    }
    for (index = 0U; index <= path_size; ++index) {
        unsigned char value = index == path_size ? (unsigned char)'/' :
                                                    (unsigned char)path[index];
        if (value == '\\' || value < 0x20U || value == 0x7fU) {
            return 0;
        }
        if (value == '/') {
            size_t segment_size = index - segment_start;
            if (segment_size == 0U ||
                (segment_size == 1U && path[segment_start] == '.') ||
                (segment_size == 2U && path[segment_start] == '.' &&
                 path[segment_start + 1U] == '.')) {
                return 0;
            }
            segment_start = index + 1U;
        }
    }
    return 1;
}

static int ascii_equal_case_insensitive(const char *left, const char *right,
                                        size_t size) {
    size_t index;

    for (index = 0U; index < size; ++index) {
        unsigned char a = (unsigned char)left[index];
        unsigned char b = (unsigned char)right[index];
        if (a >= 'A' && a <= 'Z') {
            a = (unsigned char)(a + ('a' - 'A'));
        }
        if (b >= 'A' && b <= 'Z') {
            b = (unsigned char)(b + ('a' - 'A'));
        }
        if (a != b) {
            return 0;
        }
    }
    return 1;
}

int alrios_package_index_path_is_mutable(const char *normalized_path) {
    static const char *const extensions[] = {
        ".db", ".sqlite", ".session", ".lock", ".wal", ".shm", ".log", ".tmp"
    };
    size_t path_size;
    size_t index;

    if (!normalized_path) {
        return 1;
    }
    path_size = strlen(normalized_path);
    if (!valid_path(normalized_path, path_size)) {
        return 1;
    }
    for (index = 0U; index < sizeof(extensions) / sizeof(extensions[0]); ++index) {
        size_t extension_size = strlen(extensions[index]);
        if (path_size >= extension_size &&
            ascii_equal_case_insensitive(normalized_path + path_size - extension_size,
                                         extensions[index], extension_size)) {
            return 1;
        }
    }
    return 0;
}

static int sha256_fd(int fd, uint64_t expected_size,
                     uint8_t output[ALRIOS_PACKAGE_INDEX_SHA256_SIZE]) {
    EVP_MD_CTX *context = NULL;
    uint8_t buffer[16384];
    uint64_t total = 0U;
    unsigned int digest_size = 0U;
    int result = ALRIOS_PACKAGE_INDEX_ERR_CRYPTO;

    context = EVP_MD_CTX_new();
    if (!context || EVP_DigestInit_ex(context, EVP_sha256(), NULL) != 1) {
        goto done;
    }
    for (;;) {
        ssize_t received = read(fd, buffer, sizeof(buffer));
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            result = ALRIOS_PACKAGE_INDEX_ERR_IO;
            goto done;
        }
        if (received == 0) {
            break;
        }
        if ((uint64_t)received > UINT64_MAX - total) {
            result = ALRIOS_PACKAGE_INDEX_ERR_LIMIT;
            goto done;
        }
        total += (uint64_t)received;
        if (total > expected_size ||
            EVP_DigestUpdate(context, buffer, (size_t)received) != 1) {
            result = total > expected_size ? ALRIOS_PACKAGE_INDEX_ERR_CHANGED :
                                             ALRIOS_PACKAGE_INDEX_ERR_CRYPTO;
            goto done;
        }
    }
    if (total != expected_size) {
        result = ALRIOS_PACKAGE_INDEX_ERR_CHANGED;
        goto done;
    }
    if (EVP_DigestFinal_ex(context, output, &digest_size) != 1 ||
        digest_size != ALRIOS_PACKAGE_INDEX_SHA256_SIZE) {
        goto done;
    }
    result = ALRIOS_PACKAGE_INDEX_OK;

done:
    OPENSSL_cleanse(buffer, sizeof(buffer));
    EVP_MD_CTX_free(context);
    return result;
}

static int sha256_memory(const uint8_t *data, size_t size,
                         uint8_t output[ALRIOS_PACKAGE_INDEX_SHA256_SIZE]) {
    EVP_MD_CTX *context;
    unsigned int digest_size = 0U;
    int result = ALRIOS_PACKAGE_INDEX_ERR_CRYPTO;

    if ((!data && size != 0U) || !output) {
        return ALRIOS_PACKAGE_INDEX_ERR_ARGUMENT;
    }
    context = EVP_MD_CTX_new();
    if (!context) {
        return ALRIOS_PACKAGE_INDEX_ERR_MEMORY;
    }
    if (EVP_DigestInit_ex(context, EVP_sha256(), NULL) == 1 &&
        (size == 0U || EVP_DigestUpdate(context, data, size) == 1) &&
        EVP_DigestFinal_ex(context, output, &digest_size) == 1 &&
        digest_size == ALRIOS_PACKAGE_INDEX_SHA256_SIZE) {
        result = ALRIOS_PACKAGE_INDEX_OK;
    }
    EVP_MD_CTX_free(context);
    return result;
}

static int verify_signature(const uint8_t public_key[32],
                            const uint8_t *wire, size_t wire_size) {
    EVP_PKEY *key = NULL;
    EVP_MD_CTX *context = NULL;
    uint8_t *message = NULL;
    size_t message_size;
    int result = ALRIOS_PACKAGE_INDEX_ERR_SIGNATURE;

    if (!checked_add_size(INDEX_SIGNED_HEADER_SIZE,
                          wire_size - ALRIOS_PACKAGE_INDEX_HEADER_SIZE,
                          &message_size)) {
        return ALRIOS_PACKAGE_INDEX_ERR_LIMIT;
    }
    message = malloc(message_size == 0U ? 1U : message_size);
    if (!message) {
        return ALRIOS_PACKAGE_INDEX_ERR_MEMORY;
    }
    memcpy(message, wire, INDEX_SIGNED_HEADER_SIZE);
    if (wire_size > ALRIOS_PACKAGE_INDEX_HEADER_SIZE) {
        memcpy(message + INDEX_SIGNED_HEADER_SIZE,
               wire + ALRIOS_PACKAGE_INDEX_HEADER_SIZE,
               wire_size - ALRIOS_PACKAGE_INDEX_HEADER_SIZE);
    }

    key = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, public_key, 32U);
    context = EVP_MD_CTX_new();
    if (key && context && EVP_DigestVerifyInit(context, NULL, NULL, NULL, key) == 1 &&
        EVP_DigestVerify(context, wire + INDEX_SIGNATURE_OFFSET, 64U,
                         message, message_size) == 1) {
        result = ALRIOS_PACKAGE_INDEX_OK;
    }
    OPENSSL_cleanse(message, message_size);
    free(message);
    EVP_MD_CTX_free(context);
    EVP_PKEY_free(key);
    return result;
}

void alrios_package_index_free(alrios_package_index_t *index) {
    uint32_t entry_index;

    if (!index) {
        return;
    }
    for (entry_index = 0U; entry_index < index->file_count; ++entry_index) {
        free(index->entries[entry_index].path);
    }
    free(index->entries);
    memset(index, 0, sizeof(*index));
}

int alrios_package_index_parse_and_verify(
    const uint8_t *wire,
    size_t wire_size,
    const uint8_t expected_public_key[ALRIOS_PACKAGE_INDEX_ED25519_PUBLIC_KEY_SIZE],
    alrios_package_index_t *out_index
) {
    uint16_t version;
    uint16_t header_size;
    uint32_t flags;
    uint32_t file_count;
    uint64_t total_size;
    uint64_t computed_total = 0U;
    uint8_t entry_digest[ALRIOS_PACKAGE_INDEX_SHA256_SIZE];
    const uint8_t *cursor;
    size_t remaining;
    uint32_t entry_index;
    int result;

    if (!wire || !expected_public_key || !out_index) {
        return ALRIOS_PACKAGE_INDEX_ERR_ARGUMENT;
    }
    memset(out_index, 0, sizeof(*out_index));
    if (wire_size < ALRIOS_PACKAGE_INDEX_HEADER_SIZE ||
        wire_size > ALRIOS_PACKAGE_INDEX_MAX_WIRE_SIZE ||
        memcmp(wire, ALRIOS_PACKAGE_INDEX_MAGIC,
               ALRIOS_PACKAGE_INDEX_MAGIC_SIZE) != 0) {
        return ALRIOS_PACKAGE_INDEX_ERR_FORMAT;
    }
    version = read_u16_be(wire + 16U);
    header_size = read_u16_be(wire + 18U);
    flags = read_u32_be(wire + 20U);
    file_count = read_u32_be(wire + 24U);
    total_size = read_u64_be(wire + 32U);
    if (version != ALRIOS_PACKAGE_INDEX_VERSION ||
        header_size != ALRIOS_PACKAGE_INDEX_HEADER_SIZE ||
        flags != ALRIOS_PACKAGE_INDEX_FLAG_ED25519 ||
        file_count > ALRIOS_PACKAGE_INDEX_MAX_FILES ||
        !all_zero(wire + 28U, 4U) || !all_zero(wire + 72U, 8U)) {
        return ALRIOS_PACKAGE_INDEX_ERR_UNSUPPORTED;
    }

    result = sha256_memory(wire + ALRIOS_PACKAGE_INDEX_HEADER_SIZE,
                           wire_size - ALRIOS_PACKAGE_INDEX_HEADER_SIZE,
                           entry_digest);
    if (result != ALRIOS_PACKAGE_INDEX_OK ||
        CRYPTO_memcmp(entry_digest, wire + 40U,
                      ALRIOS_PACKAGE_INDEX_SHA256_SIZE) != 0) {
        OPENSSL_cleanse(entry_digest, sizeof(entry_digest));
        return result == ALRIOS_PACKAGE_INDEX_OK ? ALRIOS_PACKAGE_INDEX_ERR_HASH : result;
    }
    OPENSSL_cleanse(entry_digest, sizeof(entry_digest));
    result = verify_signature(expected_public_key, wire, wire_size);
    if (result != ALRIOS_PACKAGE_INDEX_OK) {
        return result;
    }

    if (file_count > 0U) {
        out_index->entries = calloc((size_t)file_count, sizeof(*out_index->entries));
        if (!out_index->entries) {
            return ALRIOS_PACKAGE_INDEX_ERR_MEMORY;
        }
    }
    out_index->file_count = file_count;
    out_index->total_uncompressed_size = total_size;
    cursor = wire + ALRIOS_PACKAGE_INDEX_HEADER_SIZE;
    remaining = wire_size - ALRIOS_PACKAGE_INDEX_HEADER_SIZE;

    for (entry_index = 0U; entry_index < file_count; ++entry_index) {
        uint16_t path_size;
        uint32_t unix_mode;
        uint64_t file_size;
        alrios_package_index_entry_t *entry = &out_index->entries[entry_index];

        if (remaining < ALRIOS_PACKAGE_INDEX_ENTRY_PREFIX_SIZE) {
            result = ALRIOS_PACKAGE_INDEX_ERR_FORMAT;
            goto fail;
        }
        path_size = read_u16_be(cursor + INDEX_ENTRY_PATH_LENGTH_OFFSET);
        unix_mode = read_u32_be(cursor + INDEX_ENTRY_MODE_OFFSET);
        file_size = read_u64_be(cursor + INDEX_ENTRY_FILE_SIZE_OFFSET);
        if (read_u16_be(cursor + 2U) != 0U ||
            read_u32_be(cursor + INDEX_ENTRY_RESERVED_OFFSET) != 0U ||
            path_size == 0U || path_size >= ALRIOS_PACKAGE_INDEX_MAX_PATH_SIZE ||
            remaining - ALRIOS_PACKAGE_INDEX_ENTRY_PREFIX_SIZE < path_size) {
            result = ALRIOS_PACKAGE_INDEX_ERR_FORMAT;
            goto fail;
        }
        if ((unix_mode & ~0777U) != 0U) {
            result = ALRIOS_PACKAGE_INDEX_ERR_FORMAT;
            goto fail;
        }
        cursor += ALRIOS_PACKAGE_INDEX_ENTRY_PREFIX_SIZE;
        remaining -= ALRIOS_PACKAGE_INDEX_ENTRY_PREFIX_SIZE;
        if (!valid_path((const char *)cursor, path_size)) {
            result = ALRIOS_PACKAGE_INDEX_ERR_PATH;
            goto fail;
        }
        entry->path = malloc((size_t)path_size + 1U);
        if (!entry->path) {
            result = ALRIOS_PACKAGE_INDEX_ERR_MEMORY;
            goto fail;
        }
        memcpy(entry->path, cursor, path_size);
        entry->path[path_size] = '\0';
        if (alrios_package_index_path_is_mutable(entry->path)) {
            result = ALRIOS_PACKAGE_INDEX_ERR_MUTABLE;
            goto fail;
        }
        if (entry_index > 0U &&
            strcmp(out_index->entries[entry_index - 1U].path, entry->path) >= 0) {
            result = ALRIOS_PACKAGE_INDEX_ERR_FORMAT;
            goto fail;
        }
        entry->unix_mode = unix_mode;
        entry->file_size = file_size;
        memcpy(entry->sha256,
               cursor - ALRIOS_PACKAGE_INDEX_ENTRY_PREFIX_SIZE +
                   INDEX_ENTRY_SHA256_OFFSET,
               ALRIOS_PACKAGE_INDEX_SHA256_SIZE);
        if (file_size > UINT64_MAX - computed_total) {
            result = ALRIOS_PACKAGE_INDEX_ERR_LIMIT;
            goto fail;
        }
        computed_total += file_size;
        cursor += path_size;
        remaining -= path_size;
    }
    if (remaining != 0U || computed_total != total_size) {
        result = ALRIOS_PACKAGE_INDEX_ERR_FORMAT;
        goto fail;
    }
    return ALRIOS_PACKAGE_INDEX_OK;

fail:
    alrios_package_index_free(out_index);
    return result;
}

static int open_relative_regular_file(int root_fd, const char *path,
                                      struct stat *out_stat) {
    char *copy;
    char *cursor;
    int directory_fd;
    int result_fd = -1;

    if (!path || !out_stat) {
        return -1;
    }
    copy = strdup(path);
    if (!copy) {
        return -1;
    }
    directory_fd = dup(root_fd);
    if (directory_fd < 0) {
        free(copy);
        return -1;
    }
    cursor = copy;
    for (;;) {
        char *slash = strchr(cursor, '/');
        int next_fd;
        if (slash) {
            *slash = '\0';
            next_fd = openat(directory_fd, cursor,
                             O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
            if (next_fd < 0) {
                break;
            }
            (void)close(directory_fd);
            directory_fd = next_fd;
            cursor = slash + 1;
            continue;
        }
        result_fd = openat(directory_fd, cursor, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
        if (result_fd < 0 || fstat(result_fd, out_stat) != 0 ||
            !S_ISREG(out_stat->st_mode)) {
            if (result_fd >= 0) {
                (void)close(result_fd);
            }
            result_fd = -1;
        }
        break;
    }
    (void)close(directory_fd);
    free(copy);
    return result_fd;
}

int alrios_package_index_verify_directory(
    const alrios_package_index_t *index,
    const char *root_directory
) {
    int root_fd;
    uint32_t entry_index;

    if (!index || !root_directory || root_directory[0] == '\0' ||
        (!index->entries && index->file_count != 0U)) {
        return ALRIOS_PACKAGE_INDEX_ERR_ARGUMENT;
    }
    root_fd = open(root_directory, O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
    if (root_fd < 0) {
        return errno == ELOOP ? ALRIOS_PACKAGE_INDEX_ERR_SYMLINK :
                               ALRIOS_PACKAGE_INDEX_ERR_IO;
    }
    for (entry_index = 0U; entry_index < index->file_count; ++entry_index) {
        const alrios_package_index_entry_t *entry = &index->entries[entry_index];
        struct stat file_stat;
        uint8_t digest[ALRIOS_PACKAGE_INDEX_SHA256_SIZE];
        int file_fd;
        int result;

        if (!entry->path || alrios_package_index_path_is_mutable(entry->path)) {
            (void)close(root_fd);
            return ALRIOS_PACKAGE_INDEX_ERR_MUTABLE;
        }
        file_fd = open_relative_regular_file(root_fd, entry->path, &file_stat);
        if (file_fd < 0) {
            (void)close(root_fd);
            return errno == ELOOP ? ALRIOS_PACKAGE_INDEX_ERR_SYMLINK :
                                   ALRIOS_PACKAGE_INDEX_ERR_IO;
        }
        if (file_stat.st_size < 0 || (uint64_t)file_stat.st_size != entry->file_size ||
            ((uint32_t)file_stat.st_mode & 0777U) != entry->unix_mode) {
            (void)close(file_fd);
            (void)close(root_fd);
            return ALRIOS_PACKAGE_INDEX_ERR_CHANGED;
        }
        result = sha256_fd(file_fd, entry->file_size, digest);
        (void)close(file_fd);
        if (result != ALRIOS_PACKAGE_INDEX_OK ||
            CRYPTO_memcmp(digest, entry->sha256, sizeof(digest)) != 0) {
            OPENSSL_cleanse(digest, sizeof(digest));
            (void)close(root_fd);
            return result == ALRIOS_PACKAGE_INDEX_OK ?
                       ALRIOS_PACKAGE_INDEX_ERR_HASH : result;
        }
        OPENSSL_cleanse(digest, sizeof(digest));
    }
    (void)close(root_fd);
    return ALRIOS_PACKAGE_INDEX_OK;
}

const char *alrios_package_index_error_string(int code) {
    switch (code) {
    case ALRIOS_PACKAGE_INDEX_OK:
        return "success";
    case ALRIOS_PACKAGE_INDEX_ERR_ARGUMENT:
        return "invalid argument";
    case ALRIOS_PACKAGE_INDEX_ERR_IO:
        return "I/O error";
    case ALRIOS_PACKAGE_INDEX_ERR_MEMORY:
        return "out of memory";
    case ALRIOS_PACKAGE_INDEX_ERR_LIMIT:
        return "size or file limit exceeded";
    case ALRIOS_PACKAGE_INDEX_ERR_PATH:
        return "invalid or non-canonical file path";
    case ALRIOS_PACKAGE_INDEX_ERR_MUTABLE:
        return "forbidden mutable file extension";
    case ALRIOS_PACKAGE_INDEX_ERR_SYMLINK:
        return "symlink encountered or symlink loop detected";
    case ALRIOS_PACKAGE_INDEX_ERR_FILE_TYPE:
        return "unsupported file type";
    case ALRIOS_PACKAGE_INDEX_ERR_CRYPTO:
        return "cryptographic operation failed";
    case ALRIOS_PACKAGE_INDEX_ERR_FORMAT:
        return "malformed index format or wire corruption";
    case ALRIOS_PACKAGE_INDEX_ERR_SIGNATURE:
        return "Ed25519 signature verification failed";
    case ALRIOS_PACKAGE_INDEX_ERR_HASH:
        return "content hash mismatch";
    case ALRIOS_PACKAGE_INDEX_ERR_UNSUPPORTED:
        return "unsupported version or flags";
    case ALRIOS_PACKAGE_INDEX_ERR_CHANGED:
        return "file modified during index creation or validation";
    default:
        return "unknown index error";
    }
}
