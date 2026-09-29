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
#include "alrios/pki/certificate.h"
#include "alrios/pki/issuer.h"
#include "alrios/crypto_verify.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        (void)fprintf(stderr, "FAIL: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void test_zero_disk_exec_pipeline(void) {
    EVP_PKEY *root_pkey = NULL;
    alrios_certificate_t root_cert;
    uint8_t root_wire[ALRIOS_CERTIFICATE_WIRE_MAX];
    size_t root_wire_len = 0U;

    EVP_PKEY *inter_pkey = NULL;
    alrios_certificate_t inter_cert;
    uint8_t inter_wire[ALRIOS_CERTIFICATE_WIRE_MAX];
    size_t inter_wire_len = 0U;

    EVP_PKEY *leaf_pkey = NULL;
    alrios_certificate_t leaf_cert;
    uint8_t leaf_wire[ALRIOS_CERTIFICATE_WIRE_MAX];
    size_t leaf_wire_len = 0U;

    alrios_trust_store_t store;
    arapp_v2_header_prefix_t prefix;
    arapp_v2_signatures_t sigs;
    arapp_v2_package_t pkg;
    uint8_t buffer[32768];
    size_t written = 0U;
    uint8_t ml_priv[ML_DSA_65_SECRET_KEY_LEN];
    uint8_t ml_pub[ML_DSA_65_PUBLIC_KEY_LEN];
    const uint8_t *ml_pub_ptr = NULL;

    uint8_t decryption_key[AES256_KEY_LEN];
    uint8_t wrong_key[AES256_KEY_LEN];
    uint8_t iv[AES256_GCM_IV_LEN];
    uint8_t tag[AES256_GCM_TAG_LEN];
    const char *canonical_manifest = "{\"app_id\":\"zero-disk-app\",\"execution_profile\":\"sovereign\",\"execution_ring\":\"sovereign_trust\",\"ipc\":{},\"network\":{},\"resources\":{},\"storage\":{},\"vault\":{},\"version\":\"1.0.0\"}";
    const uint8_t mock_elf[] = "\x7f" "ELF" "\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00SOVEREIGN_ELF_PAYLOAD_BYTES";
    uint8_t ciphertext[sizeof(mock_elf)];
    uint8_t elf_hash[SHA512_DIGEST_LEN];
    uint64_t current_time;
    int memfd = -1;

    (void)printf("[TEST] MP-009: Testing complete verified decrypt-to-memfd execution pipeline...\n");

    TEST_ASSERT(alrios_pki_issue_root("alrios.root.ca", 365, &root_pkey, &root_cert, root_wire, sizeof(root_wire), &root_wire_len) == ALRIOS_ISSUER_OK);
    alrios_trust_store_init(&store);
    TEST_ASSERT(alrios_trust_store_add_anchor(&store, &root_cert) == ALRIOS_CERTIFICATE_OK);
    TEST_ASSERT(alrios_trust_store_seal(&store) == ALRIOS_CERTIFICATE_OK);

    TEST_ASSERT(alrios_pki_issue_intermediate("alrios.inter.ca", 90, &root_cert, root_pkey, &inter_pkey, &inter_cert, inter_wire, sizeof(inter_wire), &inter_wire_len) == ALRIOS_ISSUER_OK);
    TEST_ASSERT(alrios_pki_issue_leaf("com.alrigroup.zerodisk.app", 30, &inter_cert, inter_pkey, &leaf_pkey, &leaf_cert, leaf_wire, sizeof(leaf_wire), &leaf_wire_len) == ALRIOS_ISSUER_OK);

    current_time = leaf_cert.not_before + 10ULL;

    if (alrios_ml_dsa_65_provider_available()) {
        TEST_ASSERT(alrios_ml_dsa_65_keypair(ml_pub, ml_priv) == ALRIOS_CRYPTO_OK);
        ml_pub_ptr = ml_pub;
    } else {
        memset(ml_priv, 0x11, sizeof(ml_priv));
        memset(ml_pub, 0x22, sizeof(ml_pub));
        ml_pub_ptr = ml_pub;
    }

    memset(decryption_key, 0x55, sizeof(decryption_key));
    memset(wrong_key, 0x99, sizeof(wrong_key));
    memset(iv, 0x33, sizeof(iv));

    TEST_ASSERT(alrios_sha512(mock_elf, sizeof(mock_elf), elf_hash) == ALRIOS_CRYPTO_OK);

    EVP_CIPHER_CTX *gcm_ctx = EVP_CIPHER_CTX_new();
    TEST_ASSERT(gcm_ctx != NULL);
    TEST_ASSERT(EVP_EncryptInit_ex(gcm_ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) == 1);
    TEST_ASSERT(EVP_CIPHER_CTX_ctrl(gcm_ctx, EVP_CTRL_GCM_SET_IVLEN, sizeof(iv), NULL) == 1);
    TEST_ASSERT(EVP_EncryptInit_ex(gcm_ctx, NULL, NULL, decryption_key, iv) == 1);

    int out_l = 0, fin_l = 0;
    TEST_ASSERT(EVP_EncryptUpdate(gcm_ctx, NULL, &out_l, (const uint8_t *)canonical_manifest, (int)strlen(canonical_manifest)) == 1);
    TEST_ASSERT(EVP_EncryptUpdate(gcm_ctx, ciphertext, &out_l, mock_elf, sizeof(mock_elf)) == 1);
    TEST_ASSERT(EVP_EncryptFinal_ex(gcm_ctx, ciphertext + out_l, &fin_l) == 1);
    TEST_ASSERT(EVP_CIPHER_CTX_ctrl(gcm_ctx, EVP_CTRL_GCM_GET_TAG, sizeof(tag), tag) == 1);
    EVP_CIPHER_CTX_free(gcm_ctx);

    memset(&prefix, 0, sizeof(prefix));
    memcpy(prefix.magic, "ALRIGROUP@ARAPP\0", ARAPP_V2_MAGIC_LEN);
    prefix.format_version = ARAPP_V2_FORMAT_VERSION;
    prefix.target_arch = ARAPP_V2_ARCH_X86_64;
    prefix.container_flags = ARAPP_V2_FLAG_PROFILE_SOVEREIGN | ARAPP_V2_FLAG_PQC_HYBRID_SIG | ARAPP_V2_FLAG_RING_SOVEREIGN;
    prefix.timestamp_issued = current_time - 5ULL;
    prefix.timestamp_expiry = leaf_cert.not_after + 2000ULL;
    prefix.header_size_bytes = ARAPP_V2_FILE_HEADER_SIZE;
    prefix.entitlements_json_length = (uint32_t)strlen(canonical_manifest);
    prefix.ciphertext_size_bytes = (uint32_t)sizeof(mock_elf);
    memcpy(prefix.aes_gcm_iv, iv, ARAPP_V2_GCM_IV_LEN);
    memcpy(prefix.aes_gcm_tag, tag, ARAPP_V2_GCM_TAG_LEN);
    memcpy(prefix.cleartext_sha512, elf_hash, SHA512_DIGEST_LEN);

    memset(&sigs, 0, sizeof(sigs));
    TEST_ASSERT(arapp_v2_build(&prefix, &sigs, canonical_manifest, strlen(canonical_manifest),
                         ciphertext, sizeof(mock_elf), buffer, sizeof(buffer), &written) == ARAPP_V2_OK);

    TEST_ASSERT(arapp_v2_read(buffer, written, &pkg) == ARAPP_V2_OK);
    TEST_ASSERT(arapp_v2_sign(&pkg, leaf_pkey, ml_priv, &sigs) == ARAPP_V2_OK);

    TEST_ASSERT(arapp_v2_build(&prefix, &sigs, canonical_manifest, strlen(canonical_manifest),
                         ciphertext, sizeof(mock_elf), buffer, sizeof(buffer), &written) == ARAPP_V2_OK);

    buffer[written - 5] ^= 0xFFU;
    TEST_ASSERT(alrios_sovereign_loader_prepare(buffer, written, &leaf_cert, &inter_cert, &store, ml_pub_ptr, current_time, decryption_key, &memfd) == ALRIOS_SOVEREIGN_LOADER_ERR_VERIFY_FAIL);
    buffer[written - 5] ^= 0xFFU;

    TEST_ASSERT(alrios_sovereign_loader_prepare(buffer, written, &leaf_cert, &inter_cert, &store, ml_pub_ptr, current_time, wrong_key, &memfd) == ALRIOS_SOVEREIGN_LOADER_ERR_DECRYPT_FAIL);

    TEST_ASSERT(alrios_sovereign_loader_prepare(buffer, written, &leaf_cert, &inter_cert, &store, ml_pub_ptr, current_time, decryption_key, &memfd) == ALRIOS_SOVEREIGN_LOADER_OK);
    TEST_ASSERT(memfd >= 0);

    int seals = fcntl(memfd, F_GET_SEALS);
    TEST_ASSERT(seals >= 0);
    TEST_ASSERT((seals & F_SEAL_WRITE) != 0);
    TEST_ASSERT((seals & F_SEAL_SEAL) != 0);
    TEST_ASSERT((seals & F_SEAL_SHRINK) != 0);
    TEST_ASSERT((seals & F_SEAL_GROW) != 0);

    uint8_t read_buf[sizeof(mock_elf)];
    ssize_t r = pread(memfd, read_buf, sizeof(mock_elf), 0);
    TEST_ASSERT(r == (ssize_t)sizeof(mock_elf));
    TEST_ASSERT(memcmp(read_buf, mock_elf, sizeof(mock_elf)) == 0);

    close(memfd);

    EVP_PKEY_free(root_pkey);
    EVP_PKEY_free(inter_pkey);
    EVP_PKEY_free(leaf_pkey);
    alrios_trust_store_clear(&store);

    (void)printf("[PASS] MP-009: Verified decrypt-to-memfd execution pipeline passed successfully\n");
}

int main(void) {
    test_zero_disk_exec_pipeline();
    return 0;
}
