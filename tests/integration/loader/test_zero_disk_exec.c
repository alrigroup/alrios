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

#include "alrios/loader/sovereign_loader.h"
#include "alrios/package/arapp_v2.h"
#include "alrios/pki/issuer.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef F_GET_SEALS
#define F_GET_SEALS 1034
#endif
#ifndef F_SEAL_SEAL
#define F_SEAL_SEAL 0x0001
#endif
#ifndef F_SEAL_SHRINK
#define F_SEAL_SHRINK 0x0002
#endif
#ifndef F_SEAL_GROW
#define F_SEAL_GROW 0x0004
#endif
#ifndef F_SEAL_WRITE
#define F_SEAL_WRITE 0x0008
#endif

#define REQUIRED_SEALS \
    (F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE)

#if defined(ALRIOS_HAVE_ML_DSA_65)
#define TEST_REQUIRE_PQC 1
#define TEST_POLICY_FLAGS ALRIOS_SOVEREIGN_POLICY_REQUIRE_PQC
#else
#define TEST_REQUIRE_PQC 0
#define TEST_POLICY_FLAGS 0U
#endif

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                      __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

typedef struct test_fixture {
    EVP_PKEY *root_key;
    EVP_PKEY *intermediate_key;
    EVP_PKEY *leaf_key;
    EVP_PKEY *ml_dsa_key;
    alrios_certificate_t root_certificate;
    alrios_certificate_t intermediate_certificate;
    alrios_certificate_t leaf_certificate;
    alrios_trust_store_t trust_store;
    alrios_sovereign_keys_t keys;
    uint8_t aes_key[32];
    uint8_t *package;
    size_t package_len;
    size_t ciphertext_offset;
} test_fixture_t;

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
    size_t index;

    for (index = 8U; index > 0U; --index) {
        output[index - 1U] = (uint8_t)value;
        value >>= 8U;
    }
}

static EVP_PKEY *generate_ml_dsa_key(void) {
#if defined(ALRIOS_HAVE_ML_DSA_65)
    EVP_PKEY_CTX *context = EVP_PKEY_CTX_new_from_name(NULL, "ML-DSA-65", NULL);
    EVP_PKEY *key = NULL;

    CHECK(context != NULL);
    CHECK(EVP_PKEY_keygen_init(context) == 1);
    CHECK(EVP_PKEY_keygen(context, &key) == 1);
    EVP_PKEY_CTX_free(context);
    return key;
#else
    return NULL;
#endif
}

static uint8_t *read_self_executable(size_t *out_len) {
    int fd = open("/proc/self/exe", O_RDONLY | O_CLOEXEC);
    struct stat metadata;
    uint8_t *bytes;
    size_t offset = 0U;

    CHECK(fd >= 0);
    CHECK(fstat(fd, &metadata) == 0);
    CHECK(metadata.st_size > 0 && (uintmax_t)metadata.st_size <= SIZE_MAX);
    *out_len = (size_t)metadata.st_size;
    bytes = (uint8_t *)malloc(*out_len);
    CHECK(bytes != NULL);
    while (offset < *out_len) {
        ssize_t count = read(fd, bytes + offset, *out_len - offset);
        CHECK(count > 0);
        offset += (size_t)count;
    }
    CHECK(close(fd) == 0);
    return bytes;
}

static void encrypt_payload(const uint8_t *plaintext,
                            size_t plaintext_len,
                            const uint8_t *aad,
                            size_t aad_len,
                            const uint8_t key[32],
                            const uint8_t iv[12],
                            uint8_t *ciphertext,
                            uint8_t tag[16]) {
    EVP_CIPHER_CTX *context = EVP_CIPHER_CTX_new();
    int count = 0;
    int output_len = 0;

    CHECK(context != NULL);
    CHECK(plaintext_len <= (size_t)INT_MAX);
    CHECK(aad_len <= (size_t)INT_MAX);
    CHECK(EVP_EncryptInit_ex(context, EVP_aes_256_gcm(), NULL, NULL, NULL) == 1);
    CHECK(EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_SET_IVLEN, 12, NULL) == 1);
    CHECK(EVP_EncryptInit_ex(context, NULL, NULL, key, iv) == 1);
    CHECK(EVP_EncryptUpdate(context, NULL, &count, aad, (int)aad_len) == 1);
    CHECK(EVP_EncryptUpdate(context, ciphertext, &count, plaintext,
                            (int)plaintext_len) == 1);
    output_len = count;
    CHECK(EVP_EncryptFinal_ex(context, ciphertext + output_len, &count) == 1);
    output_len += count;
    CHECK((size_t)output_len == plaintext_len);
    CHECK(EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_GET_TAG, 16, tag) == 1);
    EVP_CIPHER_CTX_free(context);
}

static void sign_package(test_fixture_t *fixture) {
    CHECK(alrios_arapp_v2_sign_ed25519(fixture->package,
                                       fixture->package_len,
                                       fixture->leaf_key) ==
          ALRIOS_ARAPP_V2_SIG_OK);
#if defined(ALRIOS_HAVE_ML_DSA_65)
    CHECK(fixture->ml_dsa_key != NULL);
    CHECK(alrios_arapp_v2_sign_ml_dsa_65(fixture->package,
                                         fixture->package_len,
                                         fixture->ml_dsa_key) ==
          ALRIOS_ARAPP_V2_SIG_OK);
#endif
}

static void fixture_init(test_fixture_t *fixture,
                         const uint8_t *plaintext,
                         size_t plaintext_len,
                         uint64_t now) {
    static const uint8_t manifest[] = "{\"app_id\":\"com.alrigroup.loader.test\"}";
    uint8_t root_wire[ALRIOS_CERTIFICATE_WIRE_MAX];
    uint8_t intermediate_wire[ALRIOS_CERTIFICATE_WIRE_MAX];
    uint8_t leaf_wire[ALRIOS_CERTIFICATE_WIRE_MAX];
    size_t root_wire_len = 0U;
    size_t intermediate_wire_len = 0U;
    size_t leaf_wire_len = 0U;
    const size_t manifest_len = sizeof(manifest) - 1U;
    uint8_t iv[12];
    uint8_t tag[16];
    uint8_t cleartext_sha[64];
    uint32_t flags = ALRIOS_ARAPP_V2_FLAG_PROFILE_ENTERPRISE |
                     ALRIOS_ARAPP_V2_FLAG_RING_DEVMODE |
                     ALRIOS_ARAPP_V2_FLAG_ENCRYPTED_GCM;

    memset(fixture, 0, sizeof(*fixture));
    CHECK(plaintext_len > 0U && plaintext_len <= UINT32_MAX);
    CHECK(manifest_len <= SIZE_MAX - ALRIOS_ARAPP_V2_HEADER_SIZE);
    CHECK(plaintext_len <= SIZE_MAX - ALRIOS_ARAPP_V2_HEADER_SIZE - manifest_len);

    CHECK(alrios_pki_issue_root("alrios.loader.root", 365U,
                                &fixture->root_key,
                                &fixture->root_certificate,
                                root_wire, sizeof(root_wire),
                                &root_wire_len) == ALRIOS_ISSUER_OK);
    CHECK(alrios_pki_issue_intermediate(
              "alrios.loader.intermediate", 90U,
              &fixture->root_certificate, fixture->root_key,
              &fixture->intermediate_key,
              &fixture->intermediate_certificate,
              intermediate_wire, sizeof(intermediate_wire),
              &intermediate_wire_len) == ALRIOS_ISSUER_OK);
    CHECK(alrios_pki_issue_leaf(
              "com.alrigroup.loader.test", 30U,
              &fixture->intermediate_certificate, fixture->intermediate_key,
              &fixture->leaf_key, &fixture->leaf_certificate,
              leaf_wire, sizeof(leaf_wire), &leaf_wire_len) == ALRIOS_ISSUER_OK);
    CHECK(now >= fixture->leaf_certificate.not_before &&
          now <= fixture->leaf_certificate.not_after);

    alrios_trust_store_init(&fixture->trust_store);
    CHECK(alrios_trust_store_add_anchor(&fixture->trust_store,
                                        &fixture->root_certificate) ==
          ALRIOS_CERTIFICATE_OK);
    CHECK(alrios_trust_store_seal(&fixture->trust_store) ==
          ALRIOS_CERTIFICATE_OK);

    fixture->ml_dsa_key = generate_ml_dsa_key();
#if defined(ALRIOS_HAVE_ML_DSA_65)
    flags |= ALRIOS_ARAPP_V2_FLAG_PQC_HYBRID_SIG;
#endif
    memset(fixture->aes_key, 0x55, sizeof(fixture->aes_key));
    memset(iv, 0xa6, sizeof(iv));
    CHECK(SHA512(plaintext, plaintext_len, cleartext_sha) != NULL);

    fixture->package_len = ALRIOS_ARAPP_V2_HEADER_SIZE + manifest_len +
                           plaintext_len;
    fixture->package = (uint8_t *)calloc(fixture->package_len, 1U);
    CHECK(fixture->package != NULL);
    fixture->ciphertext_offset = ALRIOS_ARAPP_V2_HEADER_SIZE + manifest_len;

    memcpy(fixture->package, ALRIOS_ARAPP_V2_MAGIC,
           ALRIOS_ARAPP_V2_MAGIC_SIZE);
    write_u16_be(fixture->package + 16U, ALRIOS_ARAPP_V2_VERSION);
    write_u16_be(fixture->package + 18U, ALRIOS_ARAPP_V2_ARCH_X86_64);
    write_u32_be(fixture->package + 20U, flags);
    write_u64_be(fixture->package + 24U, now);
    write_u64_be(fixture->package + 32U, now + 3600U);
    write_u32_be(fixture->package + 40U, ALRIOS_ARAPP_V2_HEADER_SIZE);
    write_u32_be(fixture->package + 44U, (uint32_t)manifest_len);
    write_u32_be(fixture->package + 48U, (uint32_t)plaintext_len);
    memcpy(fixture->package + 52U, iv, sizeof(iv));
    memcpy(fixture->package + 80U, cleartext_sha, sizeof(cleartext_sha));
    memcpy(fixture->package + ALRIOS_ARAPP_V2_HEADER_SIZE,
           manifest, manifest_len);
    encrypt_payload(plaintext, plaintext_len, manifest, manifest_len,
                    fixture->aes_key, iv,
                    fixture->package + fixture->ciphertext_offset, tag);
    memcpy(fixture->package + 64U, tag, sizeof(tag));
    sign_package(fixture);

    fixture->keys.ed25519_pubkey = fixture->leaf_key;
    fixture->keys.ml_dsa_pubkey = fixture->ml_dsa_key;
    fixture->keys.aes_key = fixture->aes_key;
    fixture->keys.aes_key_len = sizeof(fixture->aes_key);
    fixture->keys.leaf_certificate = &fixture->leaf_certificate;
    fixture->keys.intermediate_certificate = &fixture->intermediate_certificate;
    fixture->keys.trust_store = &fixture->trust_store;
}

static void fixture_clear(test_fixture_t *fixture) {
    free(fixture->package);
    alrios_trust_store_clear(&fixture->trust_store);
    EVP_PKEY_free(fixture->ml_dsa_key);
    EVP_PKEY_free(fixture->leaf_key);
    EVP_PKEY_free(fixture->intermediate_key);
    EVP_PKEY_free(fixture->root_key);
    memset(fixture, 0, sizeof(*fixture));
}

static void check_memfd_payload(int memfd,
                                const uint8_t *expected,
                                size_t expected_len) {
    uint8_t *actual = (uint8_t *)malloc(expected_len);
    size_t offset = 0U;
    int seals;

    CHECK(actual != NULL);
    seals = fcntl(memfd, F_GET_SEALS);
    CHECK(seals >= 0);
    CHECK((seals & REQUIRED_SEALS) == REQUIRED_SEALS);
    CHECK(lseek(memfd, 0, SEEK_SET) == 0);
    while (offset < expected_len) {
        ssize_t count = read(memfd, actual + offset, expected_len - offset);
        CHECK(count > 0);
        offset += (size_t)count;
    }
    CHECK(memcmp(actual, expected, expected_len) == 0);
    free(actual);
}

static void test_verified_decrypt_to_sealed_memfd(void) {
    static const uint8_t payload[] = {
        0x7fU, 0x45U, 0x4cU, 0x46U, 0x02U, 0x01U, 0x01U, 0x00U,
        0x41U, 0x4cU, 0x52U, 0x49U, 0x4fU, 0x53U
    };
    uint64_t now = (uint64_t)time(NULL);
    test_fixture_t fixture;
    int memfd = -1;

    fixture_init(&fixture, payload, sizeof(payload), now);
    CHECK(alrios_sovereign_loader_stage_memfd(
              fixture.package, fixture.package_len, now, TEST_POLICY_FLAGS,
              &fixture.keys, &memfd) == ALRIOS_SOVEREIGN_OK);
    CHECK(memfd >= 0);
    check_memfd_payload(memfd, payload, sizeof(payload));
    CHECK(close(memfd) == 0);
    fixture_clear(&fixture);
}

static void test_signature_and_chain_fail_before_decrypt(void) {
    static const uint8_t payload[] = "verified-before-decryption";
    uint64_t now = (uint64_t)time(NULL);
    test_fixture_t fixture;
    const uint8_t *saved_aes_key;
    int memfd = 123;

    fixture_init(&fixture, payload, sizeof(payload), now);
    fixture.package[ALRIOS_ARAPP_V2_PREFIX_SIZE] ^= 0x80U;
    saved_aes_key = fixture.keys.aes_key;
    fixture.keys.aes_key = NULL;
    fixture.keys.aes_key_len = 0U;
    CHECK(alrios_sovereign_loader_stage_memfd(
              fixture.package, fixture.package_len, now, 0U,
              &fixture.keys, &memfd) == ALRIOS_SOVEREIGN_ERR_VERIFY);
    CHECK(memfd == -1);

    fixture.keys.aes_key = saved_aes_key;
    fixture.keys.aes_key_len = sizeof(fixture.aes_key);
    fixture.package[ALRIOS_ARAPP_V2_PREFIX_SIZE] ^= 0x80U;
    fixture.leaf_certificate.signature[0] ^= 0x40U;
    CHECK(alrios_sovereign_loader_stage_memfd(
              fixture.package, fixture.package_len, now, 0U,
              &fixture.keys, &memfd) == ALRIOS_SOVEREIGN_ERR_VERIFY);
    CHECK(memfd == -1);
    fixture_clear(&fixture);
}

static void test_unsigned_policy(void) {
    static const uint8_t payload[] = "explicitly permitted unsigned payload";
    uint64_t now = (uint64_t)time(NULL);
    test_fixture_t fixture;
    int memfd = -1;

    fixture_init(&fixture, payload, sizeof(payload), now);
    memset(fixture.package + ALRIOS_ARAPP_V2_PREFIX_SIZE, 0,
           ALRIOS_ARAPP_V2_SIGNATURES_SIZE);
    write_u32_be(fixture.package + 20U,
                 ALRIOS_ARAPP_V2_FLAG_PROFILE_ENTERPRISE |
                 ALRIOS_ARAPP_V2_FLAG_RING_DEVMODE |
                 ALRIOS_ARAPP_V2_FLAG_ENCRYPTED_GCM);

    CHECK(alrios_sovereign_loader_stage_memfd(
              fixture.package, fixture.package_len, now, 0U,
              &fixture.keys, &memfd) == ALRIOS_SOVEREIGN_ERR_POLICY);
    CHECK(memfd == -1);
    CHECK(alrios_sovereign_loader_stage_memfd(
              fixture.package, fixture.package_len, now,
              ALRIOS_SOVEREIGN_POLICY_ALLOW_UNSIGNED,
              &fixture.keys, &memfd) == ALRIOS_SOVEREIGN_OK);
    check_memfd_payload(memfd, payload, sizeof(payload));
    CHECK(close(memfd) == 0);
    fixture_clear(&fixture);
}

static void test_corrupt_ciphertext_and_tag_fail_closed(void) {
    static const uint8_t payload[] = "authenticated plaintext must not escape";
    uint64_t now = (uint64_t)time(NULL);
    test_fixture_t fixture;
    int memfd = -1;

    fixture_init(&fixture, payload, sizeof(payload), now);
    fixture.package[fixture.ciphertext_offset] ^= 0x01U;
    sign_package(&fixture);
    CHECK(alrios_sovereign_loader_stage_memfd(
              fixture.package, fixture.package_len, now, 0U,
              &fixture.keys, &memfd) == ALRIOS_SOVEREIGN_ERR_DECRYPT);
    CHECK(memfd == -1);

    fixture.package[fixture.ciphertext_offset] ^= 0x01U;
    fixture.package[64U] ^= 0x01U;
    sign_package(&fixture);
    CHECK(alrios_sovereign_loader_stage_memfd(
              fixture.package, fixture.package_len, now, 0U,
              &fixture.keys, &memfd) == ALRIOS_SOVEREIGN_ERR_DECRYPT);
    CHECK(memfd == -1);
    fixture_clear(&fixture);
}

static size_t directory_entry_count(const char *path) {
    DIR *directory = opendir(path);
    struct dirent *entry;
    size_t count = 0U;

    CHECK(directory != NULL);
    errno = 0;
    while ((entry = readdir(directory)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 &&
            strcmp(entry->d_name, "..") != 0) {
            ++count;
        }
    }
    CHECK(errno == 0);
    CHECK(closedir(directory) == 0);
    return count;
}

static void test_no_plaintext_filesystem_write_and_execution(void) {
    uint8_t *executable;
    size_t executable_len;
    uint64_t now = (uint64_t)time(NULL);
    test_fixture_t fixture;
    char temporary_directory[] = "/tmp/alrios-loader-test-XXXXXX";
    int original_directory;
    int child_status = -1;
    char *argv[] = {(char *)"zero-disk-child", (char *)"--staged-child", NULL};
    char *envp[] = {NULL};

    executable = read_self_executable(&executable_len);
    fixture_init(&fixture, executable, executable_len, now);
    CHECK(mkdtemp(temporary_directory) != NULL);
    original_directory = open(".", O_RDONLY | O_CLOEXEC | O_DIRECTORY);
    CHECK(original_directory >= 0);
    CHECK(chdir(temporary_directory) == 0);
    CHECK(directory_entry_count(".") == 0U);

    CHECK(alrios_sovereign_execute_package(
              fixture.package, fixture.package_len, now, TEST_REQUIRE_PQC,
              &fixture.keys, argv, envp, &child_status) ==
          ALRIOS_SOVEREIGN_OK);
    CHECK(WIFEXITED(child_status));
    CHECK(WEXITSTATUS(child_status) == 23);
    CHECK(directory_entry_count(".") == 0U);

    CHECK(fchdir(original_directory) == 0);
    CHECK(close(original_directory) == 0);
    CHECK(rmdir(temporary_directory) == 0);
    fixture_clear(&fixture);
    free(executable);
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--staged-child") == 0) {
        return 23;
    }

    test_verified_decrypt_to_sealed_memfd();
    test_signature_and_chain_fail_before_decrypt();
    test_unsigned_policy();
    test_corrupt_ciphertext_and_tag_fail_closed();
    test_no_plaintext_filesystem_write_and_execution();
    (void)puts("test_zero_disk_exec: PASS");
    return EXIT_SUCCESS;
}
