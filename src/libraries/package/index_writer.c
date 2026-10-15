/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/package/index.h"
#include <alrios/platform/fs_compat.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#endif

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
#define INDEX_ENTRY_SHA256_OFFSET 16U
#define INDEX_ENTRY_RESERVED_OFFSET 48U

typedef struct build_entry {
    char *path;
    uint64_t size;
    uint32_t mode;
    uint8_t sha256[ALRIOS_PACKAGE_INDEX_SHA256_SIZE];
} build_entry_t;

typedef struct build_entries {
    build_entry_t *items;
    size_t count;
    size_t capacity;
    uint64_t total_size;
} build_entries_t;

static void write_u16_be(uint8_t *output, uint16_t value) {
    output[0] = (uint8_t)(value >> 8U);
    output[1] = (uint8_t)value;
}

static void write_u32_be(uint8_t *output, uint32_t value) {
    output[0] = (uint8_t)(value >> 24U);
    output[1] = (uint8_t)(value >> 16U);
    output[2] = (uint8_t)(value >> 8U);
    output[3] = (uint8_t)value;
}

static void write_u64_be(uint8_t *output, uint64_t value) {
    write_u32_be(output, (uint32_t)(value >> 32U));
    write_u32_be(output + 4U, (uint32_t)value);
}

static int checked_add_size(size_t left, size_t right, size_t *out) {
    if (!out || right > SIZE_MAX - left) {
        return 0;
    }
    *out = left + right;
    return 1;
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

static void free_entries(build_entries_t *entries) {
    size_t index;

    if (!entries) {
        return;
    }
    for (index = 0U; index < entries->count; ++index) {
        free(entries->items[index].path);
    }
    free(entries->items);
    memset(entries, 0, sizeof(*entries));
}

static int append_entry(build_entries_t *entries, const char *path,
                        const struct stat *file_stat, int file_fd) {
    build_entry_t *entry;
    build_entry_t *resized;
    size_t new_capacity;
    size_t path_size;
    int result;

    if (!entries || !path || !file_stat || file_fd < 0 || file_stat->st_size < 0) {
        return ALRIOS_PACKAGE_INDEX_ERR_ARGUMENT;
    }
    path_size = strlen(path);
    if (path_size == 0U || path_size >= ALRIOS_PACKAGE_INDEX_MAX_PATH_SIZE) {
        return ALRIOS_PACKAGE_INDEX_ERR_PATH;
    }
    if (alrios_package_index_path_is_mutable(path)) {
        return ALRIOS_PACKAGE_INDEX_ERR_MUTABLE;
    }
    if (entries->count >= ALRIOS_PACKAGE_INDEX_MAX_FILES) {
        return ALRIOS_PACKAGE_INDEX_ERR_LIMIT;
    }
    if ((uint64_t)file_stat->st_size > UINT64_MAX - entries->total_size) {
        return ALRIOS_PACKAGE_INDEX_ERR_LIMIT;
    }
    if (entries->count == entries->capacity) {
        new_capacity = entries->capacity == 0U ? 32U : entries->capacity * 2U;
        if (new_capacity < entries->capacity ||
            new_capacity > ALRIOS_PACKAGE_INDEX_MAX_FILES ||
            new_capacity > SIZE_MAX / sizeof(*entries->items)) {
            new_capacity = ALRIOS_PACKAGE_INDEX_MAX_FILES;
        }
        if (new_capacity <= entries->capacity) {
            return ALRIOS_PACKAGE_INDEX_ERR_LIMIT;
        }
        resized = realloc(entries->items, new_capacity * sizeof(*entries->items));
        if (!resized) {
            return ALRIOS_PACKAGE_INDEX_ERR_MEMORY;
        }
        entries->items = resized;
        entries->capacity = new_capacity;
    }
    entry = &entries->items[entries->count];
    memset(entry, 0, sizeof(*entry));
    entry->path = malloc(path_size + 1U);
    if (!entry->path) {
        return ALRIOS_PACKAGE_INDEX_ERR_MEMORY;
    }
    memcpy(entry->path, path, path_size + 1U);
    entry->size = (uint64_t)file_stat->st_size;
    entry->mode = (uint32_t)file_stat->st_mode & 0777U;
    result = sha256_fd(file_fd, entry->size, entry->sha256);
    if (result != ALRIOS_PACKAGE_INDEX_OK) {
        free(entry->path);
        entry->path = NULL;
        return result;
    }
    entries->total_size += entry->size;
    entries->count++;
    return ALRIOS_PACKAGE_INDEX_OK;
}

static int join_relative_path(const char *prefix, const char *name,
                              char **out_path) {
    size_t prefix_size = prefix ? strlen(prefix) : 0U;
    size_t name_size;
    size_t path_size;
    char *path;

    if (!name || !out_path) {
        return ALRIOS_PACKAGE_INDEX_ERR_ARGUMENT;
    }
    name_size = strlen(name);
    if (name_size == 0U || strchr(name, '/') || strchr(name, '\\') ||
        strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return ALRIOS_PACKAGE_INDEX_ERR_PATH;
    }
    if (!checked_add_size(prefix_size, prefix_size == 0U ? 0U : 1U,
                          &path_size) ||
        !checked_add_size(path_size, name_size, &path_size) ||
        path_size >= ALRIOS_PACKAGE_INDEX_MAX_PATH_SIZE) {
        return ALRIOS_PACKAGE_INDEX_ERR_PATH;
    }
    path = malloc(path_size + 1U);
    if (!path) {
        return ALRIOS_PACKAGE_INDEX_ERR_MEMORY;
    }
    if (prefix_size != 0U) {
        memcpy(path, prefix, prefix_size);
        path[prefix_size] = '/';
        memcpy(path + prefix_size + 1U, name, name_size);
    } else {
        memcpy(path, name, name_size);
    }
    path[path_size] = '\0';
    *out_path = path;
    return ALRIOS_PACKAGE_INDEX_OK;
}

static int collect_directory(int directory_fd, const char *prefix,
                             build_entries_t *entries) {
    int scan_fd;
    DIR *directory;
    struct dirent *item;
    int result = ALRIOS_PACKAGE_INDEX_OK;

    scan_fd = dup(directory_fd);
    if (scan_fd < 0) {
        return ALRIOS_PACKAGE_INDEX_ERR_IO;
    }
    directory = fdopendir(scan_fd);
    if (!directory) {
        (void)close(scan_fd);
        return ALRIOS_PACKAGE_INDEX_ERR_IO;
    }
    errno = 0;
    while ((item = readdir(directory)) != NULL) {
        char *relative_path = NULL;
        struct stat file_stat;
        int child_fd = -1;

        if (strcmp(item->d_name, ".") == 0 || strcmp(item->d_name, "..") == 0 ||
            strcmp(item->d_name, ".arapp.index") == 0) {
            continue;
        }
        result = join_relative_path(prefix, item->d_name, &relative_path);
        if (result != ALRIOS_PACKAGE_INDEX_OK) {
            break;
        }
        if (fstatat(directory_fd, item->d_name, &file_stat, AT_SYMLINK_NOFOLLOW) != 0) {
            result = ALRIOS_PACKAGE_INDEX_ERR_IO;
            free(relative_path);
            break;
        }
        if (S_ISLNK(file_stat.st_mode)) {
            result = ALRIOS_PACKAGE_INDEX_ERR_SYMLINK;
            free(relative_path);
            break;
        }
        if (S_ISDIR(file_stat.st_mode)) {
            child_fd = openat(directory_fd, item->d_name,
                              O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
            if (child_fd < 0) {
                result = errno == ELOOP ? ALRIOS_PACKAGE_INDEX_ERR_SYMLINK :
                                         ALRIOS_PACKAGE_INDEX_ERR_IO;
            } else {
                result = collect_directory(child_fd, relative_path, entries);
                (void)close(child_fd);
            }
            free(relative_path);
            if (result != ALRIOS_PACKAGE_INDEX_OK) {
                break;
            }
            continue;
        }
        if (!S_ISREG(file_stat.st_mode)) {
            result = ALRIOS_PACKAGE_INDEX_ERR_FILE_TYPE;
            free(relative_path);
            break;
        }
        child_fd = openat(directory_fd, item->d_name,
                          O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
        if (child_fd < 0) {
            result = errno == ELOOP ? ALRIOS_PACKAGE_INDEX_ERR_SYMLINK :
                                     ALRIOS_PACKAGE_INDEX_ERR_IO;
            free(relative_path);
            break;
        }
        {
            struct stat opened_stat;
            if (fstat(child_fd, &opened_stat) != 0 || !S_ISREG(opened_stat.st_mode) ||
                opened_stat.st_dev != file_stat.st_dev ||
                opened_stat.st_ino != file_stat.st_ino ||
                opened_stat.st_size != file_stat.st_size) {
                result = ALRIOS_PACKAGE_INDEX_ERR_CHANGED;
            } else {
                result = append_entry(entries, relative_path, &opened_stat, child_fd);
            }
        }
        (void)close(child_fd);
        free(relative_path);
        if (result != ALRIOS_PACKAGE_INDEX_OK) {
            break;
        }
        errno = 0;
    }
    if (result == ALRIOS_PACKAGE_INDEX_OK && errno != 0) {
        result = ALRIOS_PACKAGE_INDEX_ERR_IO;
    }
    (void)closedir(directory);
    return result;
}

static int compare_entries(const void *left, const void *right) {
    const build_entry_t *left_entry = left;
    const build_entry_t *right_entry = right;
    return strcmp(left_entry->path, right_entry->path);
}

static int encode_entries(const build_entries_t *entries, uint8_t *output,
                          size_t output_size) {
    size_t offset = 0U;
    size_t index;

    for (index = 0U; index < entries->count; ++index) {
        const build_entry_t *entry = &entries->items[index];
        size_t path_size = strlen(entry->path);
        size_t entry_size;
        uint8_t *cursor;

        if (path_size > UINT16_MAX ||
            !checked_add_size(ALRIOS_PACKAGE_INDEX_ENTRY_PREFIX_SIZE, path_size,
                              &entry_size) ||
            entry_size > output_size - offset) {
            return ALRIOS_PACKAGE_INDEX_ERR_LIMIT;
        }
        cursor = output + offset;
        memset(cursor, 0, ALRIOS_PACKAGE_INDEX_ENTRY_PREFIX_SIZE);
        write_u16_be(cursor, (uint16_t)path_size);
        write_u32_be(cursor + 4U, entry->mode);
        write_u64_be(cursor + 8U, entry->size);
        memcpy(cursor + INDEX_ENTRY_SHA256_OFFSET, entry->sha256,
               ALRIOS_PACKAGE_INDEX_SHA256_SIZE);
        write_u32_be(cursor + INDEX_ENTRY_RESERVED_OFFSET, 0U);
        memcpy(cursor + ALRIOS_PACKAGE_INDEX_ENTRY_PREFIX_SIZE,
               entry->path, path_size);
        offset += entry_size;
    }
    return offset == output_size ? ALRIOS_PACKAGE_INDEX_OK :
                                   ALRIOS_PACKAGE_INDEX_ERR_FORMAT;
}

static int sign_wire(uint8_t *wire, size_t wire_size,
                     const uint8_t private_key[32]) {
    EVP_PKEY *key = NULL;
    EVP_MD_CTX *context = NULL;
    uint8_t *message = NULL;
    size_t message_size;
    size_t signature_size = ALRIOS_PACKAGE_INDEX_ED25519_SIGNATURE_SIZE;
    int result = ALRIOS_PACKAGE_INDEX_ERR_CRYPTO;

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
    key = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, private_key, 32U);
    context = EVP_MD_CTX_new();
    if (key && context && EVP_DigestSignInit(context, NULL, NULL, NULL, key) == 1 &&
        EVP_DigestSign(context, wire + INDEX_SIGNATURE_OFFSET, &signature_size,
                       message, message_size) == 1 &&
        signature_size == ALRIOS_PACKAGE_INDEX_ED25519_SIGNATURE_SIZE) {
        result = ALRIOS_PACKAGE_INDEX_OK;
    }
    OPENSSL_cleanse(message, message_size);
    free(message);
    EVP_MD_CTX_free(context);
    EVP_PKEY_free(key);
    return result;
}

int alrios_package_index_build_directory(
    const char *root_directory,
    const uint8_t private_key[ALRIOS_PACKAGE_INDEX_ED25519_PRIVATE_KEY_SIZE],
    uint8_t **out_wire,
    size_t *out_wire_size
) {
    build_entries_t entries;
    int root_fd;
    size_t entries_size = 0U;
    size_t wire_size;
    uint8_t *wire = NULL;
    size_t index;
    int result;

    if (!root_directory || root_directory[0] == '\0' || !private_key ||
        !out_wire || !out_wire_size) {
        return ALRIOS_PACKAGE_INDEX_ERR_ARGUMENT;
    }
    *out_wire = NULL;
    *out_wire_size = 0U;
    memset(&entries, 0, sizeof(entries));
    root_fd = open(root_directory, O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
    if (root_fd < 0) {
        return errno == ELOOP ? ALRIOS_PACKAGE_INDEX_ERR_SYMLINK :
                               ALRIOS_PACKAGE_INDEX_ERR_IO;
    }
    result = collect_directory(root_fd, "", &entries);
    (void)close(root_fd);
    if (result != ALRIOS_PACKAGE_INDEX_OK) {
        free_entries(&entries);
        return result;
    }
    qsort(entries.items, entries.count, sizeof(*entries.items), compare_entries);
    for (index = 0U; index < entries.count; ++index) {
        size_t entry_size;
        if (!checked_add_size(ALRIOS_PACKAGE_INDEX_ENTRY_PREFIX_SIZE,
                              strlen(entries.items[index].path), &entry_size) ||
            !checked_add_size(entries_size, entry_size, &entries_size)) {
            free_entries(&entries);
            return ALRIOS_PACKAGE_INDEX_ERR_LIMIT;
        }
        if (index > 0U &&
            strcmp(entries.items[index - 1U].path, entries.items[index].path) >= 0) {
            free_entries(&entries);
            return ALRIOS_PACKAGE_INDEX_ERR_PATH;
        }
    }
    if (!checked_add_size(ALRIOS_PACKAGE_INDEX_HEADER_SIZE, entries_size,
                          &wire_size) ||
        wire_size > ALRIOS_PACKAGE_INDEX_MAX_WIRE_SIZE ||
        entries.count > UINT32_MAX) {
        free_entries(&entries);
        return ALRIOS_PACKAGE_INDEX_ERR_LIMIT;
    }
    wire = calloc(wire_size == 0U ? 1U : wire_size, 1U);
    if (!wire) {
        free_entries(&entries);
        return ALRIOS_PACKAGE_INDEX_ERR_MEMORY;
    }
    memcpy(wire, ALRIOS_PACKAGE_INDEX_MAGIC, ALRIOS_PACKAGE_INDEX_MAGIC_SIZE);
    write_u16_be(wire + 16U, ALRIOS_PACKAGE_INDEX_VERSION);
    write_u16_be(wire + 18U, ALRIOS_PACKAGE_INDEX_HEADER_SIZE);
    write_u32_be(wire + 20U, ALRIOS_PACKAGE_INDEX_FLAG_ED25519);
    write_u32_be(wire + 24U, (uint32_t)entries.count);
    write_u64_be(wire + 32U, entries.total_size);
    result = encode_entries(&entries, wire + ALRIOS_PACKAGE_INDEX_HEADER_SIZE,
                            entries_size);
    if (result == ALRIOS_PACKAGE_INDEX_OK) {
        result = sha256_memory(wire + ALRIOS_PACKAGE_INDEX_HEADER_SIZE,
                               entries_size, wire + 40U);
    }
    if (result == ALRIOS_PACKAGE_INDEX_OK) {
        result = sign_wire(wire, wire_size, private_key);
    }
    free_entries(&entries);
    if (result != ALRIOS_PACKAGE_INDEX_OK) {
        OPENSSL_cleanse(wire, wire_size);
        free(wire);
        return result;
    }
    *out_wire = wire;
    *out_wire_size = wire_size;
    return ALRIOS_PACKAGE_INDEX_OK;
}

static int write_all(int fd, const uint8_t *data, size_t size) {
    size_t offset = 0U;

    while (offset < size) {
        ssize_t written = write(fd, data + offset, size - offset);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return ALRIOS_PACKAGE_INDEX_ERR_IO;
        }
        if (written == 0) {
            return ALRIOS_PACKAGE_INDEX_ERR_IO;
        }
        offset += (size_t)written;
    }
    return ALRIOS_PACKAGE_INDEX_OK;
}

int alrios_package_index_write_directory(
    const char *root_directory,
    const char *output_path,
    const uint8_t private_key[ALRIOS_PACKAGE_INDEX_ED25519_PRIVATE_KEY_SIZE]
) {
    uint8_t *wire = NULL;
    size_t wire_size = 0U;
    char *temporary_path = NULL;
    size_t output_size;
    int output_fd = -1;
    int result;

    if (!output_path || output_path[0] == '\0') {
        return ALRIOS_PACKAGE_INDEX_ERR_ARGUMENT;
    }
    result = alrios_package_index_build_directory(root_directory, private_key,
                                                  &wire, &wire_size);
    if (result != ALRIOS_PACKAGE_INDEX_OK) {
        return result;
    }
    output_size = strlen(output_path);
    if (!checked_add_size(output_size, sizeof(".tmp.XXXXXX"), &output_size)) {
        result = ALRIOS_PACKAGE_INDEX_ERR_LIMIT;
        goto done;
    }
    temporary_path = malloc(output_size);
    if (!temporary_path) {
        result = ALRIOS_PACKAGE_INDEX_ERR_MEMORY;
        goto done;
    }
    (void)snprintf(temporary_path, output_size, "%s.tmp.XXXXXX", output_path);
    output_fd = mkstemp(temporary_path);
    if (output_fd < 0 || fchmod(output_fd, 0644) != 0) {
        result = ALRIOS_PACKAGE_INDEX_ERR_IO;
        goto done;
    }
    result = write_all(output_fd, wire, wire_size);
    if (result == ALRIOS_PACKAGE_INDEX_OK && fsync(output_fd) != 0) {
        result = ALRIOS_PACKAGE_INDEX_ERR_IO;
    }
    if (close(output_fd) != 0 && result == ALRIOS_PACKAGE_INDEX_OK) {
        result = ALRIOS_PACKAGE_INDEX_ERR_IO;
    }
    output_fd = -1;
    if (result == ALRIOS_PACKAGE_INDEX_OK && rename(temporary_path, output_path) != 0) {
        result = ALRIOS_PACKAGE_INDEX_ERR_IO;
    }

done:
    if (output_fd >= 0) {
        (void)close(output_fd);
    }
    if (result != ALRIOS_PACKAGE_INDEX_OK && temporary_path) {
        (void)unlink(temporary_path);
    }
    free(temporary_path);
    OPENSSL_cleanse(wire, wire_size);
    free(wire);
    return result;
}

static int hex_char_to_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int alrios_package_index_load_private_key_file(
    const char *key_path,
    uint8_t out_private_key[ALRIOS_PACKAGE_INDEX_ED25519_PRIVATE_KEY_SIZE]
) {
    FILE *file;
    long file_size;
    uint8_t *buffer = NULL;
    size_t read_bytes;
    EVP_PKEY *pkey = NULL;
    int result = ALRIOS_PACKAGE_INDEX_ERR_CRYPTO;

    if (!key_path || key_path[0] == '\0' || !out_private_key) {
        return ALRIOS_PACKAGE_INDEX_ERR_ARGUMENT;
    }
    file = fopen(key_path, "rb");
    if (!file) {
        return ALRIOS_PACKAGE_INDEX_ERR_IO;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        (void)fclose(file);
        return ALRIOS_PACKAGE_INDEX_ERR_IO;
    }
    file_size = ftell(file);
    if (file_size < 0 || file_size > 65536) {
        (void)fclose(file);
        return ALRIOS_PACKAGE_INDEX_ERR_LIMIT;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        (void)fclose(file);
        return ALRIOS_PACKAGE_INDEX_ERR_IO;
    }
    buffer = malloc((size_t)file_size > 0 ? (size_t)file_size : 1U);
    if (!buffer) {
        (void)fclose(file);
        return ALRIOS_PACKAGE_INDEX_ERR_MEMORY;
    }
    read_bytes = fread(buffer, 1U, (size_t)file_size, file);
    (void)fclose(file);
    if (read_bytes != (size_t)file_size) {
        OPENSSL_cleanse(buffer, (size_t)file_size);
        free(buffer);
        return ALRIOS_PACKAGE_INDEX_ERR_IO;
    }

    if (read_bytes == 32U) {
        memcpy(out_private_key, buffer, 32U);
        result = ALRIOS_PACKAGE_INDEX_OK;
        goto cleanup;
    }

    if (read_bytes == 64U) {
        /* Could be 64 raw bytes (seed+pub or raw priv) or 64 hex chars */
        int is_hex = 1;
        size_t i;
        for (i = 0U; i < 64U; ++i) {
            if (hex_char_to_val((char)buffer[i]) < 0) {
                is_hex = 0;
                break;
            }
        }
        if (is_hex) {
            for (i = 0U; i < 32U; ++i) {
                int hi = hex_char_to_val((char)buffer[i * 2U]);
                int lo = hex_char_to_val((char)buffer[i * 2U + 1U]);
                out_private_key[i] = (uint8_t)((hi << 4) | lo);
            }
            result = ALRIOS_PACKAGE_INDEX_OK;
            goto cleanup;
        } else {
            memcpy(out_private_key, buffer, 32U);
            result = ALRIOS_PACKAGE_INDEX_OK;
            goto cleanup;
        }
    }

    /* Try PEM parsing */
    {
        BIO *bio = BIO_new_mem_buf(buffer, (int)read_bytes);
        if (bio) {
            pkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
            BIO_free(bio);
        }
    }
    if (pkey) {
        size_t key_len = ALRIOS_PACKAGE_INDEX_ED25519_PRIVATE_KEY_SIZE;
        if (EVP_PKEY_get_raw_private_key(pkey, out_private_key, &key_len) == 1 &&
            key_len == ALRIOS_PACKAGE_INDEX_ED25519_PRIVATE_KEY_SIZE) {
            result = ALRIOS_PACKAGE_INDEX_OK;
        }
        EVP_PKEY_free(pkey);
    }

cleanup:
    OPENSSL_cleanse(buffer, (size_t)file_size);
    free(buffer);
    return result;
}

int alrios_package_index_load_public_key_file(
    const char *key_path,
    uint8_t out_public_key[ALRIOS_PACKAGE_INDEX_ED25519_PUBLIC_KEY_SIZE]
) {
    FILE *file;
    long file_size;
    uint8_t *buffer = NULL;
    size_t read_bytes;
    EVP_PKEY *pkey = NULL;
    int result = ALRIOS_PACKAGE_INDEX_ERR_CRYPTO;

    if (!key_path || key_path[0] == '\0' || !out_public_key) {
        return ALRIOS_PACKAGE_INDEX_ERR_ARGUMENT;
    }
    file = fopen(key_path, "rb");
    if (!file) {
        return ALRIOS_PACKAGE_INDEX_ERR_IO;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        (void)fclose(file);
        return ALRIOS_PACKAGE_INDEX_ERR_IO;
    }
    file_size = ftell(file);
    if (file_size < 0 || file_size > 65536) {
        (void)fclose(file);
        return ALRIOS_PACKAGE_INDEX_ERR_LIMIT;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        (void)fclose(file);
        return ALRIOS_PACKAGE_INDEX_ERR_IO;
    }
    buffer = malloc((size_t)file_size > 0 ? (size_t)file_size : 1U);
    if (!buffer) {
        (void)fclose(file);
        return ALRIOS_PACKAGE_INDEX_ERR_MEMORY;
    }
    read_bytes = fread(buffer, 1U, (size_t)file_size, file);
    (void)fclose(file);
    if (read_bytes != (size_t)file_size) {
        free(buffer);
        return ALRIOS_PACKAGE_INDEX_ERR_IO;
    }

    if (read_bytes == 32U) {
        memcpy(out_public_key, buffer, 32U);
        result = ALRIOS_PACKAGE_INDEX_OK;
        goto cleanup;
    }

    if (read_bytes == 64U) {
        int is_hex = 1;
        size_t i;
        for (i = 0U; i < 64U; ++i) {
            if (hex_char_to_val((char)buffer[i]) < 0) {
                is_hex = 0;
                break;
            }
        }
        if (is_hex) {
            for (i = 0U; i < 32U; ++i) {
                int hi = hex_char_to_val((char)buffer[i * 2U]);
                int lo = hex_char_to_val((char)buffer[i * 2U + 1U]);
                out_public_key[i] = (uint8_t)((hi << 4) | lo);
            }
            result = ALRIOS_PACKAGE_INDEX_OK;
            goto cleanup;
        }
    }

    {
        BIO *bio = BIO_new_mem_buf(buffer, (int)read_bytes);
        if (bio) {
            pkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
            BIO_free(bio);
        }
    }
    if (pkey) {
        size_t key_len = ALRIOS_PACKAGE_INDEX_ED25519_PUBLIC_KEY_SIZE;
        if (EVP_PKEY_get_raw_public_key(pkey, out_public_key, &key_len) == 1 &&
            key_len == ALRIOS_PACKAGE_INDEX_ED25519_PUBLIC_KEY_SIZE) {
            result = ALRIOS_PACKAGE_INDEX_OK;
        }
        EVP_PKEY_free(pkey);
    }

cleanup:
    free(buffer);
    return result;
}

int alrios_package_index_public_key_from_private(
    const uint8_t private_key[ALRIOS_PACKAGE_INDEX_ED25519_PRIVATE_KEY_SIZE],
    uint8_t out_public_key[ALRIOS_PACKAGE_INDEX_ED25519_PUBLIC_KEY_SIZE]
) {
    EVP_PKEY *key;
    size_t pub_len = ALRIOS_PACKAGE_INDEX_ED25519_PUBLIC_KEY_SIZE;
    int result = ALRIOS_PACKAGE_INDEX_ERR_CRYPTO;

    if (!private_key || !out_public_key) {
        return ALRIOS_PACKAGE_INDEX_ERR_ARGUMENT;
    }
    key = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, private_key,
                                      ALRIOS_PACKAGE_INDEX_ED25519_PRIVATE_KEY_SIZE);
    if (key) {
        if (EVP_PKEY_get_raw_public_key(key, out_public_key, &pub_len) == 1 &&
            pub_len == ALRIOS_PACKAGE_INDEX_ED25519_PUBLIC_KEY_SIZE) {
            result = ALRIOS_PACKAGE_INDEX_OK;
        }
        EVP_PKEY_free(key);
    }
    return result;
}
