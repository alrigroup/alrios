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
#include "alrios/package/index.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "CHECK failed at %s:%d: %s (errno=%d)\n", \
                      __FILE__, __LINE__, #condition, errno); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void join_path(char *output, size_t output_size,
                      const char *directory, const char *name) {
    int written = snprintf(output, output_size, "%s/%s", directory, name);
    CHECK(written >= 0 && (size_t)written < output_size);
}

static void write_file(const char *path, const char *content, mode_t mode) {
    size_t size = strlen(content);
    size_t offset = 0U;
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, mode);
    CHECK(fd >= 0);
    while (offset < size) {
        ssize_t amount = write(fd, content + offset, size - offset);
        CHECK(amount > 0);
        offset += (size_t)amount;
    }
    CHECK(close(fd) == 0);
    CHECK(chmod(path, mode) == 0);
}

static void generate_key(uint8_t private_key[32], uint8_t public_key[32]) {
    EVP_PKEY_CTX *context = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
    EVP_PKEY *key = NULL;
    size_t private_size = 32U;
    size_t public_size = 32U;

    CHECK(context != NULL);
    CHECK(EVP_PKEY_keygen_init(context) == 1);
    CHECK(EVP_PKEY_keygen(context, &key) == 1);
    CHECK(EVP_PKEY_get_raw_private_key(key, private_key, &private_size) == 1);
    CHECK(EVP_PKEY_get_raw_public_key(key, public_key, &public_size) == 1);
    CHECK(private_size == 32U && public_size == 32U);
    EVP_PKEY_free(key);
    EVP_PKEY_CTX_free(context);
}

static void build_index(const char *directory, const uint8_t private_key[32],
                        const uint8_t public_key[32],
                        alrios_package_index_t *out_index) {
    uint8_t *wire = NULL;
    size_t wire_size = 0U;

    memset(out_index, 0, sizeof(*out_index));
    CHECK(alrios_package_index_build_directory(directory, private_key,
                                               &wire, &wire_size) ==
          ALRIOS_PACKAGE_INDEX_OK);
    CHECK(alrios_package_index_parse_and_verify(wire, wire_size, public_key,
                                                out_index) ==
          ALRIOS_PACKAGE_INDEX_OK);
    free(wire);
}

static int remove_tree(const char *path) {
    DIR *directory = opendir(path);
    struct dirent *item;

    if (!directory) {
        return unlink(path);
    }
    CHECK(chmod(path, 0700) == 0);
    while ((item = readdir(directory)) != NULL) {
        char child[1024];
        struct stat child_stat;
        if (strcmp(item->d_name, ".") == 0 || strcmp(item->d_name, "..") == 0) {
            continue;
        }
        join_path(child, sizeof(child), path, item->d_name);
        CHECK(lstat(child, &child_stat) == 0);
        if (S_ISDIR(child_stat.st_mode)) {
            CHECK(remove_tree(child) == 0);
        } else {
            CHECK(chmod(child, 0600) == 0);
            CHECK(unlink(child) == 0);
        }
    }
    CHECK(closedir(directory) == 0);
    return rmdir(path);
}

static void test_delta_install(void) {
    char workspace_template[] = "/tmp/alrios-delta-XXXXXX";
    char *workspace = mkdtemp(workspace_template);
    char existing[1024];
    char payload[1024];
    char target[1024];
    char existing_same[1024];
    char existing_changed[1024];
    char payload_same[1024];
    char payload_changed[1024];
    char payload_new[1024];
    char target_same[1024];
    char target_changed[1024];
    char target_new[1024];
    struct stat old_same_stat;
    struct stat new_same_stat;
    struct stat old_changed_stat;
    struct stat new_changed_stat;
    struct stat installed_stat;
    uint8_t private_key[32];
    uint8_t public_key[32];
    alrios_package_index_t existing_index;
    alrios_package_index_t target_index;
    alrios_layering_stats_t stats;

    CHECK(workspace != NULL);
    join_path(existing, sizeof(existing), workspace, "existing");
    join_path(payload, sizeof(payload), workspace, "payload");
    join_path(target, sizeof(target), workspace, "installed");
    CHECK(mkdir(existing, 0700) == 0);
    CHECK(mkdir(payload, 0700) == 0);

    join_path(existing_same, sizeof(existing_same), existing, "same.txt");
    join_path(existing_changed, sizeof(existing_changed), existing, "changed.txt");
    join_path(payload_same, sizeof(payload_same), payload, "same.txt");
    join_path(payload_changed, sizeof(payload_changed), payload, "changed.txt");
    join_path(payload_new, sizeof(payload_new), payload, "new.txt");
    write_file(existing_same, "identical immutable content\n", 0444);
    write_file(existing_changed, "old content\n", 0444);
    write_file(payload_same, "identical immutable content\n", 0644);
    write_file(payload_changed, "new independent content\n", 0644);
    write_file(payload_new, "brand new content\n", 0755);

    generate_key(private_key, public_key);
    build_index(existing, private_key, public_key, &existing_index);
    build_index(payload, private_key, public_key, &target_index);

    CHECK(alrios_layering_install(existing, &existing_index, payload,
                                  &target_index, target, &stats) ==
          ALRIOS_LAYERING_OK);
    CHECK(stats.hardlinked_files == 1U);
    CHECK(stats.reflinked_files == 0U);
    CHECK(stats.copied_files == 2U);

    join_path(target_same, sizeof(target_same), target, "same.txt");
    join_path(target_changed, sizeof(target_changed), target, "changed.txt");
    join_path(target_new, sizeof(target_new), target, "new.txt");
    CHECK(stat(existing_same, &old_same_stat) == 0);
    CHECK(stat(target_same, &new_same_stat) == 0);
    CHECK(old_same_stat.st_dev == new_same_stat.st_dev &&
          old_same_stat.st_ino == new_same_stat.st_ino);
    CHECK(stat(existing_changed, &old_changed_stat) == 0);
    CHECK(stat(target_changed, &new_changed_stat) == 0);
    CHECK(old_changed_stat.st_dev != new_changed_stat.st_dev ||
          old_changed_stat.st_ino != new_changed_stat.st_ino);
    CHECK((new_same_stat.st_mode & 0222) == 0);
    CHECK((new_changed_stat.st_mode & 0222) == 0);
    CHECK(stat(target_new, &installed_stat) == 0);
    CHECK((installed_stat.st_mode & 0777) == 0555);

    alrios_package_index_free(&target_index);
    alrios_package_index_free(&existing_index);
    CHECK(remove_tree(target) == 0);
    CHECK(remove_tree(payload) == 0);
    CHECK(remove_tree(existing) == 0);
    CHECK(rmdir(workspace) == 0);
}

static void test_forced_fallback_and_rollback(void) {
    char workspace_template[] = "/tmp/alrios-delta-fallback-XXXXXX";
    char *workspace = mkdtemp(workspace_template);
    char payload[1024];
    char target[1024];
    char payload_file[1024];
    char target_file[1024];
    uint8_t private_key[32];
    uint8_t public_key[32];
    alrios_package_index_t target_index;
    alrios_layering_stats_t stats;

    CHECK(workspace != NULL);
    join_path(payload, sizeof(payload), workspace, "payload");
    join_path(target, sizeof(target), workspace, "installed");
    join_path(payload_file, sizeof(payload_file), payload, "payload.bin");
    CHECK(mkdir(payload, 0700) == 0);
    write_file(payload_file, "copy fallback content\n", 0644);
    generate_key(private_key, public_key);
    build_index(payload, private_key, public_key, &target_index);

    CHECK(alrios_layering_install(NULL, NULL, payload, &target_index,
                                  target, &stats) == ALRIOS_LAYERING_OK);
    CHECK(stats.hardlinked_files == 0U && stats.reflinked_files == 0U &&
          stats.copied_files == 1U);
    join_path(target_file, sizeof(target_file), target, "payload.bin");
    CHECK(access(target_file, R_OK) == 0);
    CHECK(remove_tree(target) == 0);

    write_file(payload_file, "tampered after signed index\n", 0644);
    CHECK(alrios_layering_install(NULL, NULL, payload, &target_index,
                                  target, &stats) == ALRIOS_LAYERING_ERR_HASH);
    CHECK(access(target, F_OK) != 0 && errno == ENOENT);

    alrios_package_index_free(&target_index);
    CHECK(remove_tree(payload) == 0);
    CHECK(rmdir(workspace) == 0);
}

int main(void) {
    test_delta_install();
    test_forced_fallback_and_rollback();
    (void)printf("test_delta_install: all integration tests passed\n");
    return EXIT_SUCCESS;
}
