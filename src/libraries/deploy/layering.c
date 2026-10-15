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

#include "alrios/deploy/layering.h"
#include <alrios/platform/fs_compat.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#if defined(__linux__)
#include <linux/fs.h>
#include <sys/ioctl.h>
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

#define LAYERING_COPY_BUFFER_SIZE 65536U

typedef enum transfer_kind {
    TRANSFER_HARDLINK,
    TRANSFER_REFLINK,
    TRANSFER_COPY
} transfer_kind_t;

static mode_t readonly_mode(uint32_t indexed_mode) {
    return (indexed_mode & 0111U) != 0U ? (mode_t)0555 : (mode_t)0444;
}

static int valid_index(const alrios_package_index_t *index) {
    uint32_t position;

    if (!index || (!index->entries && index->file_count != 0U)) {
        return 0;
    }
    for (position = 0U; position < index->file_count; ++position) {
        const alrios_package_index_entry_t *entry = &index->entries[position];
        if (!entry->path || alrios_package_index_path_is_mutable(entry->path)) {
            return 0;
        }
        if (position != 0U &&
            strcmp(index->entries[position - 1U].path, entry->path) >= 0) {
            return 0;
        }
    }
    return 1;
}

static const alrios_package_index_entry_t *find_entry(
    const alrios_package_index_t *index,
    const alrios_package_index_entry_t *wanted
) {
    uint32_t low = 0U;
    uint32_t high;

    if (!index) {
        return NULL;
    }
    high = index->file_count;
    while (low < high) {
        uint32_t middle = low + ((high - low) / 2U);
        const alrios_package_index_entry_t *candidate = &index->entries[middle];
        int comparison = strcmp(candidate->path, wanted->path);
        if (comparison < 0) {
            low = middle + 1U;
        } else if (comparison > 0) {
            high = middle;
        } else if (candidate->file_size == wanted->file_size &&
                   CRYPTO_memcmp(candidate->sha256, wanted->sha256,
                                 ALRIOS_PACKAGE_INDEX_SHA256_SIZE) == 0) {
            return candidate;
        } else {
            return NULL;
        }
    }
    return NULL;
}

static int open_parent_directory(int root_fd, const char *relative_path,
                                 int create, int *out_parent_fd,
                                 char **out_filename) {
    char *path_copy;
    char *cursor;
    int current_fd;

    if (root_fd < 0 || !relative_path || !out_parent_fd || !out_filename ||
        alrios_package_index_path_is_mutable(relative_path)) {
        return ALRIOS_LAYERING_ERR_PATH;
    }
    path_copy = strdup(relative_path);
    if (!path_copy) {
        return ALRIOS_LAYERING_ERR_IO;
    }
    current_fd = dup(root_fd);
    if (current_fd < 0) {
        free(path_copy);
        return ALRIOS_LAYERING_ERR_IO;
    }
    cursor = path_copy;
    for (;;) {
        char *slash = strchr(cursor, '/');
        int next_fd;
        if (!slash) {
            break;
        }
        *slash = '\0';
        if (create && mkdirat(current_fd, cursor, 0700) != 0 && errno != EEXIST) {
            (void)close(current_fd);
            free(path_copy);
            return ALRIOS_LAYERING_ERR_IO;
        }
        next_fd = openat(current_fd, cursor,
                         O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
        if (next_fd < 0) {
            (void)close(current_fd);
            free(path_copy);
            return ALRIOS_LAYERING_ERR_PATH;
        }
        (void)close(current_fd);
        current_fd = next_fd;
        cursor = slash + 1;
    }
    *out_filename = strdup(cursor);
    if (!*out_filename) {
        (void)close(current_fd);
        free(path_copy);
        return ALRIOS_LAYERING_ERR_IO;
    }
    *out_parent_fd = current_fd;
    free(path_copy);
    return ALRIOS_LAYERING_OK;
}

static int open_indexed_file(int root_fd,
                             const alrios_package_index_entry_t *entry,
                             int *out_fd) {
    char *filename = NULL;
    struct stat file_stat;
    int parent_fd = -1;
    int file_fd;
    int result;

    result = open_parent_directory(root_fd, entry->path, 0,
                                   &parent_fd, &filename);
    if (result != ALRIOS_LAYERING_OK) {
        return result;
    }
    file_fd = openat(parent_fd, filename, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    free(filename);
    (void)close(parent_fd);
    if (file_fd < 0 || fstat(file_fd, &file_stat) != 0 ||
        !S_ISREG(file_stat.st_mode)) {
        if (file_fd >= 0) {
            (void)close(file_fd);
        }
        return ALRIOS_LAYERING_ERR_IO;
    }
    *out_fd = file_fd;
    return ALRIOS_LAYERING_OK;
}

static int hash_fd(int fd, uint64_t expected_size,
                   uint8_t output[ALRIOS_PACKAGE_INDEX_SHA256_SIZE]) {
    EVP_MD_CTX *context = NULL;
    uint8_t buffer[LAYERING_COPY_BUFFER_SIZE];
    uint64_t total_size = 0U;
    unsigned int digest_size = 0U;
    int result = ALRIOS_LAYERING_ERR_HASH;

    if (lseek(fd, 0, SEEK_SET) < 0) {
        return ALRIOS_LAYERING_ERR_IO;
    }
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
            result = ALRIOS_LAYERING_ERR_IO;
            goto done;
        }
        if (received == 0) {
            break;
        }
        if ((uint64_t)received > expected_size - total_size ||
            EVP_DigestUpdate(context, buffer, (size_t)received) != 1) {
            goto done;
        }
        total_size += (uint64_t)received;
    }
    if (total_size != expected_size ||
        EVP_DigestFinal_ex(context, output, &digest_size) != 1 ||
        digest_size != ALRIOS_PACKAGE_INDEX_SHA256_SIZE) {
        goto done;
    }
    result = ALRIOS_LAYERING_OK;

done:
    OPENSSL_cleanse(buffer, sizeof(buffer));
    EVP_MD_CTX_free(context);
    return result;
}

static int verify_open_file(int fd,
                            const alrios_package_index_entry_t *entry) {
    struct stat file_stat;
    uint8_t digest[ALRIOS_PACKAGE_INDEX_SHA256_SIZE];
    int result;

    if (fstat(fd, &file_stat) != 0 || !S_ISREG(file_stat.st_mode) ||
        file_stat.st_size < 0 || (uint64_t)file_stat.st_size != entry->file_size) {
        return ALRIOS_LAYERING_ERR_HASH;
    }
    result = hash_fd(fd, entry->file_size, digest);
    if (result == ALRIOS_LAYERING_OK &&
        CRYPTO_memcmp(digest, entry->sha256, sizeof(digest)) != 0) {
        result = ALRIOS_LAYERING_ERR_HASH;
    }
    OPENSSL_cleanse(digest, sizeof(digest));
    return result;
}

static int buffered_copy(int source_fd, int destination_fd) {
    uint8_t buffer[LAYERING_COPY_BUFFER_SIZE];
    int result = ALRIOS_LAYERING_OK;

    if (lseek(source_fd, 0, SEEK_SET) < 0) {
        return ALRIOS_LAYERING_ERR_IO;
    }
    for (;;) {
        ssize_t received = read(source_fd, buffer, sizeof(buffer));
        size_t written = 0U;
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            result = ALRIOS_LAYERING_ERR_IO;
            break;
        }
        if (received == 0) {
            break;
        }
        while (written < (size_t)received) {
            ssize_t amount = write(destination_fd, buffer + written,
                                   (size_t)received - written);
            if (amount < 0) {
                if (errno == EINTR) {
                    continue;
                }
                result = ALRIOS_LAYERING_ERR_IO;
                break;
            }
            if (amount == 0) {
                result = ALRIOS_LAYERING_ERR_IO;
                break;
            }
            written += (size_t)amount;
        }
        if (result != ALRIOS_LAYERING_OK) {
            break;
        }
    }
    OPENSSL_cleanse(buffer, sizeof(buffer));
    return result;
}

static int transfer_file(int source_root_fd, int destination_root_fd,
                         const alrios_package_index_entry_t *entry,
                         int permit_hardlink, int permit_reflink,
                         transfer_kind_t *out_kind) {
    char *source_name = NULL;
    char *destination_name = NULL;
    int source_parent_fd = -1;
    int destination_parent_fd = -1;
    int source_fd = -1;
    int destination_fd = -1;
    int result;

    result = open_parent_directory(source_root_fd, entry->path, 0,
                                   &source_parent_fd, &source_name);
    if (result != ALRIOS_LAYERING_OK) {
        goto done;
    }
    source_fd = openat(source_parent_fd, source_name,
                       O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (source_fd < 0) {
        result = ALRIOS_LAYERING_ERR_IO;
        goto done;
    }
    result = verify_open_file(source_fd, entry);
    if (result != ALRIOS_LAYERING_OK) {
        goto done;
    }
    if (permit_hardlink) {
        struct stat source_stat;
        if (fstat(source_fd, &source_stat) != 0 ||
            (source_stat.st_mode & 0777) != readonly_mode(entry->unix_mode)) {
            permit_hardlink = 0;
        }
    }
    result = open_parent_directory(destination_root_fd, entry->path, 1,
                                   &destination_parent_fd, &destination_name);
    if (result != ALRIOS_LAYERING_OK) {
        goto done;
    }
    if (permit_hardlink &&
        linkat(source_parent_fd, source_name, destination_parent_fd,
               destination_name, 0) == 0) {
        destination_fd = openat(destination_parent_fd, destination_name,
                                O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
        if (destination_fd < 0) {
            result = ALRIOS_LAYERING_ERR_IO;
            goto done;
        }
        result = verify_open_file(destination_fd, entry);
        if (result == ALRIOS_LAYERING_OK) {
            *out_kind = TRANSFER_HARDLINK;
        } else {
            (void)unlinkat(destination_parent_fd, destination_name, 0);
        }
        goto done;
    }
    destination_fd = openat(destination_parent_fd, destination_name,
                            O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
                            0600);
    if (destination_fd < 0) {
        result = ALRIOS_LAYERING_ERR_IO;
        goto done;
    }
#if defined(__linux__) && defined(FICLONE)
    if (permit_reflink && ioctl(destination_fd, FICLONE, source_fd) == 0) {
        *out_kind = TRANSFER_REFLINK;
        result = ALRIOS_LAYERING_OK;
    } else
#else
    (void)permit_reflink;
#endif
    {
        if (ftruncate(destination_fd, 0) != 0) {
            result = ALRIOS_LAYERING_ERR_IO;
            goto done;
        }
        result = buffered_copy(source_fd, destination_fd);
        if (result == ALRIOS_LAYERING_OK) {
            *out_kind = TRANSFER_COPY;
        }
    }
    if (result == ALRIOS_LAYERING_OK && fsync(destination_fd) != 0) {
        result = ALRIOS_LAYERING_ERR_IO;
    }
    if (result == ALRIOS_LAYERING_OK) {
        result = verify_open_file(destination_fd, entry);
    }
    if (result != ALRIOS_LAYERING_OK) {
        (void)unlinkat(destination_parent_fd, destination_name, 0);
    }

done:
    if (destination_fd >= 0) {
        (void)close(destination_fd);
    }
    if (source_fd >= 0) {
        (void)close(source_fd);
    }
    if (destination_parent_fd >= 0) {
        (void)close(destination_parent_fd);
    }
    if (source_parent_fd >= 0) {
        (void)close(source_parent_fd);
    }
    free(destination_name);
    free(source_name);
    return result;
}

static int verify_directory_contents(int root_fd,
                                     const alrios_package_index_t *index) {
    uint32_t position;

    for (position = 0U; position < index->file_count; ++position) {
        int file_fd = -1;
        int result = open_indexed_file(root_fd, &index->entries[position],
                                       &file_fd);
        if (result == ALRIOS_LAYERING_OK) {
            result = verify_open_file(file_fd, &index->entries[position]);
            (void)close(file_fd);
        }
        if (result != ALRIOS_LAYERING_OK) {
            return result;
        }
    }
    return ALRIOS_LAYERING_OK;
}

static int enforce_file_permissions(int root_fd,
                                    const alrios_package_index_t *index) {
    uint32_t position;

    for (position = 0U; position < index->file_count; ++position) {
        char *filename = NULL;
        int parent_fd = -1;
        int result = open_parent_directory(root_fd, index->entries[position].path,
                                           0, &parent_fd, &filename);
        if (result != ALRIOS_LAYERING_OK) {
            return result;
        }
        if (fchmodat(parent_fd, filename,
                     readonly_mode(index->entries[position].unix_mode), 0) != 0) {
            free(filename);
            (void)close(parent_fd);
            return ALRIOS_LAYERING_ERR_IO;
        }
        free(filename);
        (void)close(parent_fd);
    }
    return ALRIOS_LAYERING_OK;
}

static int make_directories_readonly(int directory_fd) {
    DIR *directory;
    struct dirent *item;
    int scan_fd = dup(directory_fd);
    int result = ALRIOS_LAYERING_OK;

    if (scan_fd < 0) {
        return ALRIOS_LAYERING_ERR_IO;
    }
    directory = fdopendir(scan_fd);
    if (!directory) {
        (void)close(scan_fd);
        return ALRIOS_LAYERING_ERR_IO;
    }
    errno = 0;
    while ((item = readdir(directory)) != NULL) {
        struct stat item_stat;
        int child_fd;
        if (strcmp(item->d_name, ".") == 0 || strcmp(item->d_name, "..") == 0) {
            continue;
        }
        if (fstatat(directory_fd, item->d_name, &item_stat,
                    AT_SYMLINK_NOFOLLOW) != 0) {
            result = ALRIOS_LAYERING_ERR_IO;
            break;
        }
        if (!S_ISDIR(item_stat.st_mode)) {
            continue;
        }
        child_fd = openat(directory_fd, item->d_name,
                          O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
        if (child_fd < 0) {
            result = ALRIOS_LAYERING_ERR_PATH;
            break;
        }
        result = make_directories_readonly(child_fd);
        if (result == ALRIOS_LAYERING_OK && fchmod(child_fd, 0555) != 0) {
            result = ALRIOS_LAYERING_ERR_IO;
        }
        (void)close(child_fd);
        if (result != ALRIOS_LAYERING_OK) {
            break;
        }
    }
    if (result == ALRIOS_LAYERING_OK && errno != 0) {
        result = ALRIOS_LAYERING_ERR_IO;
    }
    (void)closedir(directory);
    return result;
}

static int remove_tree_contents(int directory_fd) {
    DIR *directory;
    struct dirent *item;
    int scan_fd;
    int result = ALRIOS_LAYERING_OK;

    if (fchmod(directory_fd, 0700) != 0) {
        return ALRIOS_LAYERING_ERR_ROLLBACK;
    }
    scan_fd = dup(directory_fd);
    if (scan_fd < 0) {
        return ALRIOS_LAYERING_ERR_ROLLBACK;
    }
    directory = fdopendir(scan_fd);
    if (!directory) {
        (void)close(scan_fd);
        return ALRIOS_LAYERING_ERR_ROLLBACK;
    }
    errno = 0;
    while ((item = readdir(directory)) != NULL) {
        struct stat item_stat;
        if (strcmp(item->d_name, ".") == 0 || strcmp(item->d_name, "..") == 0) {
            continue;
        }
        if (fstatat(directory_fd, item->d_name, &item_stat,
                    AT_SYMLINK_NOFOLLOW) != 0) {
            result = ALRIOS_LAYERING_ERR_ROLLBACK;
            break;
        }
        if (S_ISDIR(item_stat.st_mode)) {
            int child_fd = openat(directory_fd, item->d_name,
                                  O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
            if (child_fd < 0 || remove_tree_contents(child_fd) != ALRIOS_LAYERING_OK) {
                if (child_fd >= 0) {
                    (void)close(child_fd);
                }
                result = ALRIOS_LAYERING_ERR_ROLLBACK;
                break;
            }
            (void)close(child_fd);
            if (unlinkat(directory_fd, item->d_name, AT_REMOVEDIR) != 0) {
                result = ALRIOS_LAYERING_ERR_ROLLBACK;
                break;
            }
        } else if (unlinkat(directory_fd, item->d_name, 0) != 0) {
            result = ALRIOS_LAYERING_ERR_ROLLBACK;
            break;
        }
    }
    if (result == ALRIOS_LAYERING_OK && errno != 0) {
        result = ALRIOS_LAYERING_ERR_ROLLBACK;
    }
    (void)closedir(directory);
    return result;
}

static int cleanup_staging(const char *path) {
    int directory_fd = open(path, O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
    int result;

    if (directory_fd < 0) {
        return errno == ENOENT ? ALRIOS_LAYERING_OK :
                                ALRIOS_LAYERING_ERR_ROLLBACK;
    }
    result = remove_tree_contents(directory_fd);
    (void)close(directory_fd);
    if (result != ALRIOS_LAYERING_OK || rmdir(path) != 0) {
        return ALRIOS_LAYERING_ERR_ROLLBACK;
    }
    return ALRIOS_LAYERING_OK;
}

static int split_target_path(const char *target, char **out_parent,
                             char **out_name) {
    char *copy;
    char *slash;
    size_t size;

    if (!target || target[0] == '\0') {
        return ALRIOS_LAYERING_ERR_ARGUMENT;
    }
    size = strlen(target);
    while (size > 1U && target[size - 1U] == '/') {
        --size;
    }
    copy = malloc(size + 1U);
    if (!copy) {
        return ALRIOS_LAYERING_ERR_IO;
    }
    memcpy(copy, target, size);
    copy[size] = '\0';
    slash = strrchr(copy, '/');
    if (!slash) {
        *out_parent = strdup(".");
        *out_name = strdup(copy);
    } else if (slash == copy) {
        *slash = '\0';
        *out_parent = strdup("/");
        *out_name = strdup(slash + 1);
    } else {
        *slash = '\0';
        *out_parent = strdup(copy);
        *out_name = strdup(slash + 1);
    }
    free(copy);
    if (!*out_parent || !*out_name || (*out_name)[0] == '\0' ||
        strcmp(*out_name, ".") == 0 || strcmp(*out_name, "..") == 0) {
        free(*out_parent);
        free(*out_name);
        *out_parent = NULL;
        *out_name = NULL;
        return ALRIOS_LAYERING_ERR_PATH;
    }
    return ALRIOS_LAYERING_OK;
}

int alrios_layering_install(const char *existing_directory,
                            const alrios_package_index_t *existing_index,
                            const char *payload_directory,
                            const alrios_package_index_t *target_index,
                            const char *target_directory,
                            alrios_layering_stats_t *out_stats) {
    alrios_layering_stats_t stats;
    char *target_parent = NULL;
    char *target_name = NULL;
    char *staging_path = NULL;
    int existing_fd = -1;
    int payload_fd = -1;
    int staging_fd = -1;
    int parent_fd = -1;
    int result;
    uint32_t position;
    size_t template_size;
    struct stat target_stat;

    memset(&stats, 0, sizeof(stats));
    if (out_stats) {
        memset(out_stats, 0, sizeof(*out_stats));
    }
    if (!payload_directory || payload_directory[0] == '\0' ||
        !target_directory || target_directory[0] == '\0' ||
        !valid_index(target_index) ||
        ((existing_directory == NULL) != (existing_index == NULL)) ||
        (existing_index && !valid_index(existing_index))) {
        return ALRIOS_LAYERING_ERR_ARGUMENT;
    }
    result = split_target_path(target_directory, &target_parent, &target_name);
    if (result != ALRIOS_LAYERING_OK) {
        goto done;
    }
    parent_fd = open(target_parent,
                     O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
    if (parent_fd < 0) {
        result = ALRIOS_LAYERING_ERR_IO;
        goto done;
    }
    if (fstatat(parent_fd, target_name, &target_stat,
                AT_SYMLINK_NOFOLLOW) == 0) {
        result = ALRIOS_LAYERING_ERR_EXISTS;
        goto done;
    }
    if (errno != ENOENT) {
        result = ALRIOS_LAYERING_ERR_IO;
        goto done;
    }
    template_size = strlen(target_parent) + strlen(target_name) + 24U;
    staging_path = malloc(template_size);
    if (!staging_path) {
        result = ALRIOS_LAYERING_ERR_IO;
        goto done;
    }
    if (snprintf(staging_path, template_size, "%s/.%s.staging.XXXXXX",
                 target_parent, target_name) < 0 || !mkdtemp(staging_path)) {
        result = ALRIOS_LAYERING_ERR_IO;
        goto done;
    }
    staging_fd = open(staging_path,
                      O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
    payload_fd = open(payload_directory,
                      O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
    if (staging_fd < 0 || payload_fd < 0) {
        result = ALRIOS_LAYERING_ERR_IO;
        goto rollback;
    }
    if (existing_index) {
        existing_fd = open(existing_directory,
                           O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
        if (existing_fd < 0) {
            result = ALRIOS_LAYERING_ERR_IO;
            goto rollback;
        }
    }
    for (position = 0U; position < target_index->file_count; ++position) {
        const alrios_package_index_entry_t *target_entry =
            &target_index->entries[position];
        const alrios_package_index_entry_t *old_entry =
            find_entry(existing_index, target_entry);
        transfer_kind_t kind = TRANSFER_COPY;
        int source_fd = old_entry ? existing_fd : payload_fd;
        int permit_hardlink = old_entry &&
            readonly_mode(old_entry->unix_mode) == readonly_mode(target_entry->unix_mode);

        result = transfer_file(source_fd, staging_fd, target_entry,
                               permit_hardlink, old_entry != NULL, &kind);
        if (result != ALRIOS_LAYERING_OK) {
            goto rollback;
        }
        if (kind == TRANSFER_HARDLINK) {
            ++stats.hardlinked_files;
        } else if (kind == TRANSFER_REFLINK) {
            ++stats.reflinked_files;
        } else {
            ++stats.copied_files;
        }
        if (target_entry->file_size > UINT64_MAX - stats.installed_bytes) {
            result = ALRIOS_LAYERING_ERR_IO;
            goto rollback;
        }
        stats.installed_bytes += target_entry->file_size;
    }
    result = verify_directory_contents(staging_fd, target_index);
    if (result != ALRIOS_LAYERING_OK) {
        goto rollback;
    }
    result = enforce_file_permissions(staging_fd, target_index);
    if (result == ALRIOS_LAYERING_OK) {
        result = make_directories_readonly(staging_fd);
    }
    if (result == ALRIOS_LAYERING_OK && fchmod(staging_fd, 0555) != 0) {
        result = ALRIOS_LAYERING_ERR_IO;
    }
    if (result == ALRIOS_LAYERING_OK && fsync(staging_fd) != 0) {
        result = ALRIOS_LAYERING_ERR_IO;
    }
    if (result != ALRIOS_LAYERING_OK) {
        goto rollback;
    }
    if (rename(staging_path, target_directory) != 0) {
        result = errno == EEXIST || errno == ENOTEMPTY ?
                     ALRIOS_LAYERING_ERR_EXISTS : ALRIOS_LAYERING_ERR_IO;
        goto rollback;
    }
    if (fsync(parent_fd) != 0) {
        result = ALRIOS_LAYERING_ERR_IO;
        goto done;
    }
    if (out_stats) {
        *out_stats = stats;
    }
    result = ALRIOS_LAYERING_OK;
    goto done;

rollback:
    if (staging_fd >= 0) {
        (void)close(staging_fd);
        staging_fd = -1;
    }
    if (staging_path && cleanup_staging(staging_path) != ALRIOS_LAYERING_OK &&
        result == ALRIOS_LAYERING_OK) {
        result = ALRIOS_LAYERING_ERR_ROLLBACK;
    }

done:
    if (existing_fd >= 0) {
        (void)close(existing_fd);
    }
    if (payload_fd >= 0) {
        (void)close(payload_fd);
    }
    if (staging_fd >= 0) {
        (void)close(staging_fd);
    }
    if (parent_fd >= 0) {
        (void)close(parent_fd);
    }
    free(staging_path);
    free(target_name);
    free(target_parent);
    return result;
}

const char *alrios_layering_error_string(int code) {
    switch (code) {
    case ALRIOS_LAYERING_OK:
        return "success";
    case ALRIOS_LAYERING_ERR_ARGUMENT:
        return "invalid argument or index";
    case ALRIOS_LAYERING_ERR_IO:
        return "I/O error";
    case ALRIOS_LAYERING_ERR_PATH:
        return "unsafe path or symlink encountered";
    case ALRIOS_LAYERING_ERR_HASH:
        return "file size or SHA-256 mismatch";
    case ALRIOS_LAYERING_ERR_EXISTS:
        return "target already exists";
    case ALRIOS_LAYERING_ERR_ROLLBACK:
        return "staging rollback failed";
    default:
        return "unknown layering error";
    }
}
