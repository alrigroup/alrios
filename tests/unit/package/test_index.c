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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <openssl/evp.h>
#include <openssl/crypto.h>

#define CHECK(cond) do { \
    if (!(cond)) { \
        (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                      __FILE__, __LINE__, #cond); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void write_file(const char *path, const void *data, size_t size, mode_t mode) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, mode);
    CHECK(fd >= 0);
    if (size > 0U) {
        CHECK(write(fd, data, size) == (ssize_t)size);
    }
    CHECK(close(fd) == 0);
}

static void generate_ed25519_keypair(
    uint8_t priv[ALRIOS_PACKAGE_INDEX_ED25519_PRIVATE_KEY_SIZE],
    uint8_t pub[ALRIOS_PACKAGE_INDEX_ED25519_PUBLIC_KEY_SIZE]
) {
    EVP_PKEY *pkey = NULL;
    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
    size_t priv_len = ALRIOS_PACKAGE_INDEX_ED25519_PRIVATE_KEY_SIZE;
    size_t pub_len = ALRIOS_PACKAGE_INDEX_ED25519_PUBLIC_KEY_SIZE;

    CHECK(pctx != NULL);
    CHECK(EVP_PKEY_keygen_init(pctx) == 1);
    CHECK(EVP_PKEY_keygen(pctx, &pkey) == 1);
    EVP_PKEY_CTX_free(pctx);

    CHECK(EVP_PKEY_get_raw_private_key(pkey, priv, &priv_len) == 1);
    CHECK(priv_len == ALRIOS_PACKAGE_INDEX_ED25519_PRIVATE_KEY_SIZE);
    CHECK(EVP_PKEY_get_raw_public_key(pkey, pub, &pub_len) == 1);
    CHECK(pub_len == ALRIOS_PACKAGE_INDEX_ED25519_PUBLIC_KEY_SIZE);

    EVP_PKEY_free(pkey);
}

static void test_key_loader_and_derivation(void) {
    uint8_t priv[32];
    uint8_t pub[32];
    uint8_t derived_pub[32];
    uint8_t loaded_priv[32];
    uint8_t loaded_pub[32];
    char tmpl[] = "/tmp/alrios-test-idxkey-XXXXXX";
    char *dir = mkdtemp(tmpl);
    char priv_path[512];
    char pub_path[512];
    char hex_priv_path[512];
    char hex_buf[65];
    size_t i;

    CHECK(dir != NULL);
    generate_ed25519_keypair(priv, pub);

    CHECK(alrios_package_index_public_key_from_private(priv, derived_pub) == ALRIOS_PACKAGE_INDEX_OK);
    CHECK(memcmp(pub, derived_pub, 32) == 0);

    snprintf(priv_path, sizeof(priv_path), "%s/priv.bin", dir);
    snprintf(pub_path, sizeof(pub_path), "%s/pub.bin", dir);
    snprintf(hex_priv_path, sizeof(hex_priv_path), "%s/priv.hex", dir);

    write_file(priv_path, priv, sizeof(priv), 0600);
    write_file(pub_path, pub, sizeof(pub), 0644);

    for (i = 0U; i < 32U; ++i) {
        snprintf(hex_buf + (i * 2U), 3, "%02x", priv[i]);
    }
    hex_buf[64] = '\0';
    write_file(hex_priv_path, hex_buf, 64, 0600);

    CHECK(alrios_package_index_load_private_key_file(priv_path, loaded_priv) == ALRIOS_PACKAGE_INDEX_OK);
    CHECK(memcmp(priv, loaded_priv, 32) == 0);

    CHECK(alrios_package_index_load_private_key_file(hex_priv_path, loaded_priv) == ALRIOS_PACKAGE_INDEX_OK);
    CHECK(memcmp(priv, loaded_priv, 32) == 0);

    CHECK(alrios_package_index_load_public_key_file(pub_path, loaded_pub) == ALRIOS_PACKAGE_INDEX_OK);
    CHECK(memcmp(pub, loaded_pub, 32) == 0);

    unlink(priv_path);
    unlink(pub_path);
    unlink(hex_priv_path);
    rmdir(dir);
}

static void test_roundtrip_and_directory_verification(void) {
    uint8_t priv[32];
    uint8_t pub[32];
    char tmpl[] = "/tmp/alrios-test-idxrt-XXXXXX";
    char *dir = mkdtemp(tmpl);
    char sub_dir[512];
    char file1[512];
    char file2[512];
    char index_file[512];
    uint8_t *wire = NULL;
    size_t wire_size = 0U;
    alrios_package_index_t index;
    int rc;

    CHECK(dir != NULL);
    generate_ed25519_keypair(priv, pub);

    snprintf(sub_dir, sizeof(sub_dir), "%s/bin", dir);
    CHECK(mkdir(sub_dir, 0755) == 0);

    snprintf(file1, sizeof(file1), "%s/bin/app", dir);
    snprintf(file2, sizeof(file2), "%s/config.txt", dir);
    snprintf(index_file, sizeof(index_file), "%s/.arapp.index", dir);

    write_file(file1, "binary content 123456", 21, 0755);
    write_file(file2, "setting=true\n", 13, 0644);

    /* Build wire index */
    rc = alrios_package_index_build_directory(dir, priv, &wire, &wire_size);
    CHECK(rc == ALRIOS_PACKAGE_INDEX_OK);
    CHECK(wire != NULL && wire_size > 0);

    /* Parse and verify wire */
    memset(&index, 0, sizeof(index));
    rc = alrios_package_index_parse_and_verify(wire, wire_size, pub, &index);
    CHECK(rc == ALRIOS_PACKAGE_INDEX_OK);
    CHECK(index.file_count == 2);
    CHECK(index.total_uncompressed_size == 34);
    CHECK(strcmp(index.entries[0].path, "bin/app") == 0);
    CHECK(index.entries[0].file_size == 21);
    CHECK(index.entries[0].unix_mode == 0755);
    CHECK(strcmp(index.entries[1].path, "config.txt") == 0);
    CHECK(index.entries[1].file_size == 13);
    CHECK(index.entries[1].unix_mode == 0644);

    /* Verify directory contents match index */
    rc = alrios_package_index_verify_directory(&index, dir);
    CHECK(rc == ALRIOS_PACKAGE_INDEX_OK);

    /* Write out index to destination */
    rc = alrios_package_index_write_directory(dir, index_file, priv);
    CHECK(rc == ALRIOS_PACKAGE_INDEX_OK);

    /* Rebuilding directory must automatically ignore .arapp.index */
    {
        uint8_t *wire2 = NULL;
        size_t wire2_size = 0U;
        alrios_package_index_t index2;

        rc = alrios_package_index_build_directory(dir, priv, &wire2, &wire2_size);
        CHECK(rc == ALRIOS_PACKAGE_INDEX_OK);
        CHECK(wire2_size == wire_size);
        CHECK(memcmp(wire, wire2, wire_size) == 0);

        memset(&index2, 0, sizeof(index2));
        rc = alrios_package_index_parse_and_verify(wire2, wire2_size, pub, &index2);
        CHECK(rc == ALRIOS_PACKAGE_INDEX_OK);
        CHECK(index2.file_count == 2);
        alrios_package_index_free(&index2);
        free(wire2);
    }

    /* Modify file content in directory -> verify_directory fails */
    write_file(file2, "tampered_data\n", 14, 0644);
    rc = alrios_package_index_verify_directory(&index, dir);
    CHECK(rc == ALRIOS_PACKAGE_INDEX_ERR_CHANGED || rc == ALRIOS_PACKAGE_INDEX_ERR_HASH);

    alrios_package_index_free(&index);
    free(wire);

    unlink(file1);
    unlink(file2);
    unlink(index_file);
    rmdir(sub_dir);
    rmdir(dir);
}

static void test_adversarial_rejections(void) {
    uint8_t priv[32];
    uint8_t pub[32];
    uint8_t wrong_pub[32];
    uint8_t wrong_priv[32];
    char tmpl[] = "/tmp/alrios-test-idxadv-XXXXXX";
    char *dir = mkdtemp(tmpl);
    char file_ok[512];
    char file_mut[512];
    char file_symlink[512];
    uint8_t *wire = NULL;
    size_t wire_size = 0U;
    alrios_package_index_t index;
    int rc;

    CHECK(dir != NULL);
    generate_ed25519_keypair(priv, pub);
    generate_ed25519_keypair(wrong_priv, wrong_pub);

    snprintf(file_ok, sizeof(file_ok), "%s/payload.txt", dir);
    write_file(file_ok, "valid payload", 13, 0644);

    rc = alrios_package_index_build_directory(dir, priv, &wire, &wire_size);
    CHECK(rc == ALRIOS_PACKAGE_INDEX_OK);

    /* 1. Signature check with wrong public key */
    memset(&index, 0, sizeof(index));
    rc = alrios_package_index_parse_and_verify(wire, wire_size, wrong_pub, &index);
    CHECK(rc == ALRIOS_PACKAGE_INDEX_ERR_SIGNATURE);

    /* 2. Tampered header byte */
    wire[32] ^= 0x01; /* tamper total_size (part of signed header) */
    rc = alrios_package_index_parse_and_verify(wire, wire_size, pub, &index);
    CHECK(rc == ALRIOS_PACKAGE_INDEX_ERR_SIGNATURE || rc == ALRIOS_PACKAGE_INDEX_ERR_HASH);
    wire[32] ^= 0x01; /* restore */

    /* 3. Tampered payload / entry byte */
    wire[ALRIOS_PACKAGE_INDEX_HEADER_SIZE + 2] ^= 0x01;
    rc = alrios_package_index_parse_and_verify(wire, wire_size, pub, &index);
    CHECK(rc == ALRIOS_PACKAGE_INDEX_ERR_HASH || rc == ALRIOS_PACKAGE_INDEX_ERR_SIGNATURE);
    wire[ALRIOS_PACKAGE_INDEX_HEADER_SIZE + 2] ^= 0x01; /* restore */

    /* 4. Truncation */
    rc = alrios_package_index_parse_and_verify(wire, wire_size - 1, pub, &index);
    CHECK(rc != ALRIOS_PACKAGE_INDEX_OK);

    /* 5. Trailing junk */
    {
        uint8_t *extended = malloc(wire_size + 16);
        CHECK(extended != NULL);
        memcpy(extended, wire, wire_size);
        memset(extended + wire_size, 0x42, 16);
        rc = alrios_package_index_parse_and_verify(extended, wire_size + 16, pub, &index);
        CHECK(rc != ALRIOS_PACKAGE_INDEX_OK);
        free(extended);
    }

    free(wire);

    /* 6. Mutable file extension rejection (.db, .log, etc.) */
    snprintf(file_mut, sizeof(file_mut), "%s/database.sqlite", dir);
    write_file(file_mut, "fake db", 7, 0644);
    rc = alrios_package_index_build_directory(dir, priv, &wire, &wire_size);
    CHECK(rc == ALRIOS_PACKAGE_INDEX_ERR_MUTABLE);
    unlink(file_mut);

    /* 7. Symlink rejection */
    snprintf(file_symlink, sizeof(file_symlink), "%s/link.txt", dir);
    CHECK(symlink("payload.txt", file_symlink) == 0);
    rc = alrios_package_index_build_directory(dir, priv, &wire, &wire_size);
    CHECK(rc == ALRIOS_PACKAGE_INDEX_ERR_SYMLINK);
    unlink(file_symlink);

    unlink(file_ok);
    rmdir(dir);
}

static void test_error_strings(void) {
    CHECK(strcmp(alrios_package_index_error_string(ALRIOS_PACKAGE_INDEX_OK), "success") == 0);
    CHECK(strcmp(alrios_package_index_error_string(ALRIOS_PACKAGE_INDEX_ERR_SIGNATURE),
                 "Ed25519 signature verification failed") == 0);
    CHECK(strcmp(alrios_package_index_error_string(ALRIOS_PACKAGE_INDEX_ERR_MUTABLE),
                 "forbidden mutable file extension") == 0);
}

int main(void) {
    test_key_loader_and_derivation();
    test_roundtrip_and_directory_verification();
    test_adversarial_rejections();
    test_error_strings();

    printf("test_index: all unit tests passed\n");
    return 0;
}
