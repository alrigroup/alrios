/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/package/arapp_v2.h"
#include "alrios/pki/certificate.h"
#include "alrios/pki/issuer.h"
#include "alrios/crypto_verify.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        (void)fprintf(stderr, "FAIL: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static const char *valid_canonical_manifest =
    "{\"app_id\":\"test-service\",\"execution_profile\":\"sovereign\",\"execution_ring\":\"sovereign_trust\",\"ipc\":{},\"network\":{},\"resources\":{},\"storage\":{},\"vault\":{},\"version\":\"1.0.0\"}";

static void test_signed_arapp_lifecycle(void) {
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
    uint8_t buffer[16384];
    size_t written = 0U;
    uint8_t ml_priv[ML_DSA_65_SECRET_KEY_LEN];
    uint8_t ml_pub[ML_DSA_65_PUBLIC_KEY_LEN];
    const uint8_t ciphertext[64] = "Encrypted application payload bytes for signed arapp v2 test";
    uint64_t current_time;

    (void)printf("[TEST] MP-008: Testing signed .arapp v2 with certificate chain & hybrid signatures...\n");

    TEST_ASSERT(alrios_pki_issue_root("alrios.root.ca", 365, &root_pkey, &root_cert, root_wire, sizeof(root_wire), &root_wire_len) == ALRIOS_ISSUER_OK);
    alrios_trust_store_init(&store);
    TEST_ASSERT(alrios_trust_store_add_anchor(&store, &root_cert) == ALRIOS_CERTIFICATE_OK);
    TEST_ASSERT(alrios_trust_store_seal(&store) == ALRIOS_CERTIFICATE_OK);

    TEST_ASSERT(alrios_pki_issue_intermediate("alrios.inter.ca", 90, &root_cert, root_pkey, &inter_pkey, &inter_cert, inter_wire, sizeof(inter_wire), &inter_wire_len) == ALRIOS_ISSUER_OK);
    TEST_ASSERT(alrios_pki_issue_leaf("com.alrigroup.signed.arapp", 30, &inter_cert, inter_pkey, &leaf_pkey, &leaf_cert, leaf_wire, sizeof(leaf_wire), &leaf_wire_len) == ALRIOS_ISSUER_OK);

    current_time = leaf_cert.not_before + 10ULL;

    if (alrios_ml_dsa_65_provider_available()) {
        TEST_ASSERT(alrios_ml_dsa_65_keypair(ml_pub, ml_priv) == ALRIOS_CRYPTO_OK);
    } else {
        memset(ml_priv, 0x11, sizeof(ml_priv));
        memset(ml_pub, 0x22, sizeof(ml_pub));
    }

    memset(&prefix, 0, sizeof(prefix));
    memcpy(prefix.magic, "ALRIGROUP@ARAPP\0", ARAPP_V2_MAGIC_LEN);
    prefix.format_version = ARAPP_V2_FORMAT_VERSION;
    prefix.target_arch = ARAPP_V2_ARCH_X86_64;
    prefix.container_flags = ARAPP_V2_FLAG_PROFILE_SOVEREIGN | ARAPP_V2_FLAG_PQC_HYBRID_SIG | ARAPP_V2_FLAG_RING_SOVEREIGN;
    prefix.timestamp_issued = current_time - 5ULL;
    prefix.timestamp_expiry = leaf_cert.not_after + 2000ULL;
    prefix.header_size_bytes = ARAPP_V2_FILE_HEADER_SIZE;
    prefix.entitlements_json_length = (uint32_t)strlen(valid_canonical_manifest);
    prefix.ciphertext_size_bytes = (uint32_t)sizeof(ciphertext);

    memset(&sigs, 0, sizeof(sigs));
    TEST_ASSERT(arapp_v2_build(&prefix, &sigs, valid_canonical_manifest, strlen(valid_canonical_manifest),
                         ciphertext, sizeof(ciphertext), buffer, sizeof(buffer), &written) == ARAPP_V2_OK);

    TEST_ASSERT(arapp_v2_read(buffer, written, &pkg) == ARAPP_V2_OK);

    TEST_ASSERT(arapp_v2_sign(&pkg, leaf_pkey, ml_priv, &sigs) == ARAPP_V2_OK);

    TEST_ASSERT(arapp_v2_build(&prefix, &sigs, valid_canonical_manifest, strlen(valid_canonical_manifest),
                         ciphertext, sizeof(ciphertext), buffer, sizeof(buffer), &written) == ARAPP_V2_OK);

    if (alrios_ml_dsa_65_provider_available()) {
        TEST_ASSERT(arapp_v2_verify_package_with_chain(buffer, written, &leaf_cert, &inter_cert, &store, ml_pub, current_time, &pkg) == ARAPP_V2_OK);
    }

    buffer[written - 5] ^= 0xFFU;
    TEST_ASSERT(arapp_v2_verify_package_with_chain(buffer, written, &leaf_cert, &inter_cert, &store, ml_pub, current_time, &pkg) == ARAPP_V2_ERR_SIGNATURE_INVALID);
    buffer[written - 5] ^= 0xFFU;

    buffer[ARAPP_V2_HEADER_PREFIX_SIZE + 5] ^= 0x01U;
    TEST_ASSERT(arapp_v2_verify_package_with_chain(buffer, written, &leaf_cert, &inter_cert, &store, ml_pub, current_time, &pkg) == ARAPP_V2_ERR_SIGNATURE_INVALID);
    buffer[ARAPP_V2_HEADER_PREFIX_SIZE + 5] ^= 0x01U;

    TEST_ASSERT(arapp_v2_verify_package_with_chain(buffer, written, &leaf_cert, &inter_cert, &store, ml_pub, leaf_cert.not_after + 100ULL, &pkg) == ARAPP_V2_ERR_SIGNATURE_INVALID);

    TEST_ASSERT(arapp_v2_verify_package_with_chain(buffer, written, &leaf_cert, &inter_cert, &store, NULL, current_time, &pkg) == ARAPP_V2_ERR_SIGNATURE_INVALID);

    EVP_PKEY_free(root_pkey);
    EVP_PKEY_free(inter_pkey);
    EVP_PKEY_free(leaf_pkey);
    alrios_trust_store_clear(&store);

    (void)printf("[PASS] MP-008: Signed .arapp v2 package verification & tampering protection verified\n");
}

int main(void) {
    test_signed_arapp_lifecycle();
    return 0;
}
