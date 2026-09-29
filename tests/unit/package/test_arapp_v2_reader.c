/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/package/arapp_v2.h"
#include "alrios/crypto_verify.h"
#include "alrios/pki/certificate.h"

#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond) do { \
    if (!(cond)) { \
        (void)fprintf(stderr, "FAIL: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static const char valid_canonical_manifest[] =
    "{\"app_id\":\"com.alrigroup.sample\","
    "\"execution_profile\":\"sovereign\","
    "\"execution_ring\":\"sovereign_trust\","
    "\"ipc\":{\"allowed_peers\":[\"arws\"]},"
    "\"network\":{\"allow_inbound\":true,\"allow_outbound\":true,\"ports\":[]},"
    "\"resources\":{\"cpu_weight\":100,\"max_threads\":4,\"memory_burst_mb\":128,\"memory_guaranteed_mb\":64},"
    "\"storage\":{\"persistent_mount\":\"var/data/sample\",\"quota_mb\":512},"
    "\"vault\":{\"secrets\":[\"SECRET_KEY\"]},"
    "\"version\":\"1.0.0\"}";

static void setup_valid_prefix(arapp_v2_header_prefix_t *prefix, uint32_t manifest_len, uint32_t ct_len) {
    memset(prefix, 0, sizeof(*prefix));
    memcpy(prefix->magic, "ALRIGROUP@ARAPP\0", ARAPP_V2_MAGIC_LEN);
    prefix->format_version = ARAPP_V2_FORMAT_VERSION;
    prefix->target_arch = ARAPP_V2_ARCH_X86_64;
    prefix->container_flags = ARAPP_V2_FLAG_PROFILE_SOVEREIGN | ARAPP_V2_FLAG_PQC_HYBRID_SIG;
    prefix->timestamp_issued = 1700000000ULL;
    prefix->timestamp_expiry = 1700086400ULL;
    prefix->header_size_bytes = ARAPP_V2_FILE_HEADER_SIZE;
    prefix->entitlements_json_length = manifest_len;
    prefix->ciphertext_size_bytes = ct_len;
    memset(prefix->aes_gcm_iv, 0x01, ARAPP_V2_GCM_IV_LEN);
    memset(prefix->aes_gcm_tag, 0x02, ARAPP_V2_GCM_TAG_LEN);
    memset(prefix->cleartext_sha512, 0x03, ARAPP_V2_SHA512_LEN);
}

static void test_positive_canonical_reader(void) {
    arapp_v2_header_prefix_t prefix;
    arapp_v2_signatures_t sigs;
    arapp_v2_package_t pkg;
    uint8_t buffer[16384];
    size_t written = 0U;
    const uint8_t dummy_ct[128] = { 0x42 };

    memset(&sigs, 0xAA, sizeof(sigs));
    setup_valid_prefix(&prefix, (uint32_t)strlen(valid_canonical_manifest), sizeof(dummy_ct));

    CHECK(arapp_v2_build(&prefix, &sigs, valid_canonical_manifest, strlen(valid_canonical_manifest),
                         dummy_ct, sizeof(dummy_ct), buffer, sizeof(buffer), &written) == ARAPP_V2_OK);
    CHECK(written == ARAPP_V2_FILE_HEADER_SIZE + strlen(valid_canonical_manifest) + sizeof(dummy_ct));

    /* Sovereign profile requires hybrid signature & FILE_HEADER_SIZE */
    setup_valid_prefix(&prefix, (uint32_t)strlen(valid_canonical_manifest), sizeof(dummy_ct));
    prefix.container_flags = ARAPP_V2_FLAG_PROFILE_ENTERPRISE;
    prefix.header_size_bytes = ARAPP_V2_HEADER_PREFIX_SIZE;
    const char enterprise_manifest[] =
        "{\"app_id\":\"com.alrigroup.enterprise\","
        "\"execution_profile\":\"enterprise\","
        "\"execution_ring\":\"devmode_sandbox\","
        "\"ipc\":{\"allowed_peers\":[\"arws\"]},"
        "\"network\":{\"allow_inbound\":true,\"allow_outbound\":true,\"ports\":[]},"
        "\"resources\":{\"cpu_weight\":100,\"max_threads\":4,\"memory_burst_mb\":128,\"memory_guaranteed_mb\":64},"
        "\"storage\":{\"persistent_mount\":\"var/data/sample\",\"quota_mb\":512},"
        "\"vault\":{\"secrets\":[\"SECRET_KEY\"]},"
        "\"version\":\"1.0.0\"}";
    setup_valid_prefix(&prefix, (uint32_t)strlen(enterprise_manifest), sizeof(dummy_ct));
    prefix.container_flags = ARAPP_V2_FLAG_PROFILE_ENTERPRISE;
    prefix.header_size_bytes = ARAPP_V2_HEADER_PREFIX_SIZE;

    CHECK(arapp_v2_build(&prefix, NULL, enterprise_manifest, strlen(enterprise_manifest),
                         dummy_ct, sizeof(dummy_ct), buffer, sizeof(buffer), &written) == ARAPP_V2_OK);
    CHECK(arapp_v2_read_and_validate(buffer, written, 1700005000ULL, &pkg) == ARAPP_V2_OK);
    CHECK(pkg.format_version == ARAPP_V2_FORMAT_VERSION);
    CHECK(pkg.container_flags == ARAPP_V2_FLAG_PROFILE_ENTERPRISE);
    CHECK(pkg.has_signatures == 0U);
}

static void test_acceptance_overflow_safe_offsets(void) {
    arapp_v2_header_prefix_t prefix;
    arapp_v2_signatures_t sigs;
    arapp_v2_package_t pkg;
    uint8_t buffer[8192];
    size_t written = 0U;
    const uint8_t dummy_ct[16] = { 0x55 };

    memset(&sigs, 0, sizeof(sigs));
    setup_valid_prefix(&prefix, (uint32_t)strlen(valid_canonical_manifest), sizeof(dummy_ct));

    CHECK(arapp_v2_build(&prefix, &sigs, valid_canonical_manifest, strlen(valid_canonical_manifest),
                         dummy_ct, sizeof(dummy_ct), buffer, sizeof(buffer), &written) == ARAPP_V2_OK);

    CHECK(arapp_v2_read(NULL, written, &pkg) == ARAPP_V2_ERR_INVALID_ARGUMENT);
    CHECK(arapp_v2_read(buffer, written, NULL) == ARAPP_V2_ERR_INVALID_ARGUMENT);
    CHECK(arapp_v2_read(buffer, 0U, &pkg) == ARAPP_V2_ERR_TRUNCATED);
    CHECK(arapp_v2_read(buffer, ARAPP_V2_HEADER_PREFIX_SIZE - 1U, &pkg) == ARAPP_V2_ERR_TRUNCATED);

    CHECK(arapp_v2_read(buffer, written - 1U, &pkg) == ARAPP_V2_ERR_TRUNCATED);
    CHECK(arapp_v2_read(buffer, written + 1U, &pkg) == ARAPP_V2_ERR_TRAILING_BYTES);

    uint8_t corrupt[8192];
    memcpy(corrupt, buffer, written);
    corrupt[40] = 0xFF;
    corrupt[41] = 0xFF;
    corrupt[42] = 0xFF;
    corrupt[43] = 0xFF;
    CHECK(arapp_v2_read(corrupt, written, &pkg) == ARAPP_V2_ERR_HEADER_SIZE);

    memcpy(corrupt, buffer, written);
    corrupt[44] = 0xFF;
    corrupt[45] = 0xFF;
    corrupt[46] = 0xFF;
    corrupt[47] = 0xFF;
    CHECK(arapp_v2_read(corrupt, written, &pkg) == ARAPP_V2_ERR_TRUNCATED ||
          arapp_v2_read(corrupt, written, &pkg) == ARAPP_V2_ERR_OFFSET_OVERFLOW);

    memcpy(corrupt, buffer, written);
    corrupt[48] = 0xFF;
    corrupt[49] = 0xFF;
    corrupt[50] = 0xFF;
    corrupt[51] = 0xFF;
    CHECK(arapp_v2_read(corrupt, written, &pkg) == ARAPP_V2_ERR_TRUNCATED ||
          arapp_v2_read(corrupt, written, &pkg) == ARAPP_V2_ERR_OFFSET_OVERFLOW);

    memcpy(corrupt, buffer, written);
    corrupt[0] = 'X';
    CHECK(arapp_v2_read(corrupt, written, &pkg) == ARAPP_V2_ERR_BAD_MAGIC);

    memcpy(corrupt, buffer, written);
    corrupt[16] = 0x00;
    corrupt[17] = 0x03;
    CHECK(arapp_v2_read(corrupt, written, &pkg) == ARAPP_V2_ERR_VERSION);

    memcpy(corrupt, buffer, written);
    corrupt[18] = 0x00;
    corrupt[19] = 0x99;
    CHECK(arapp_v2_read(corrupt, written, &pkg) == ARAPP_V2_ERR_ARCH);
}

static void test_acceptance_canonical_manifest(void) {
    arapp_v2_header_prefix_t prefix;
    arapp_v2_signatures_t sigs;
    arapp_v2_package_t pkg;
    uint8_t buffer[8192];
    size_t written = 0U;

    memset(&sigs, 0, sizeof(sigs));

    const char noncanonical_whitespace[] =
        " { \"app_id\" : \"com.alrigroup.sample\", "
        "\"execution_profile\":\"sovereign\","
        "\"execution_ring\":\"sovereign_trust\","
        "\"ipc\":{\"allowed_peers\":[\"arws\"]},"
        "\"network\":{\"allow_inbound\":true,\"allow_outbound\":true,\"ports\":[]},"
        "\"resources\":{\"cpu_weight\":100,\"max_threads\":4,\"memory_burst_mb\":128,\"memory_guaranteed_mb\":64},"
        "\"storage\":{\"persistent_mount\":\"var/data/sample\",\"quota_mb\":512},"
        "\"vault\":{\"secrets\":[\"SECRET_KEY\"]},"
        "\"version\":\"1.0.0\" } \n";

    setup_valid_prefix(&prefix, (uint32_t)strlen(noncanonical_whitespace), 0U);
    CHECK(arapp_v2_build(&prefix, &sigs, noncanonical_whitespace, strlen(noncanonical_whitespace),
                         NULL, 0U, buffer, sizeof(buffer), &written) == ARAPP_V2_OK);
    CHECK(arapp_v2_read(buffer, written, &pkg) == ARAPP_V2_OK);
    CHECK(arapp_v2_validate_manifest(&pkg) == ARAPP_V2_ERR_NONCANONICAL_MANIFEST);

    const char noncanonical_unsorted[] =
        "{\"version\":\"1.0.0\","
        "\"app_id\":\"com.alrigroup.sample\","
        "\"execution_profile\":\"sovereign\","
        "\"execution_ring\":\"sovereign_trust\","
        "\"ipc\":{\"allowed_peers\":[\"arws\"]},"
        "\"network\":{\"allow_inbound\":true,\"allow_outbound\":true,\"ports\":[]},"
        "\"resources\":{\"cpu_weight\":100,\"max_threads\":4,\"memory_burst_mb\":128,\"memory_guaranteed_mb\":64},"
        "\"storage\":{\"persistent_mount\":\"var/data/sample\",\"quota_mb\":512},"
        "\"vault\":{\"secrets\":[\"SECRET_KEY\"]}}";

    setup_valid_prefix(&prefix, (uint32_t)strlen(noncanonical_unsorted), 0U);
    CHECK(arapp_v2_build(&prefix, &sigs, noncanonical_unsorted, strlen(noncanonical_unsorted),
                         NULL, 0U, buffer, sizeof(buffer), &written) == ARAPP_V2_OK);
    CHECK(arapp_v2_read(buffer, written, &pkg) == ARAPP_V2_OK);
    CHECK(arapp_v2_validate_manifest(&pkg) == ARAPP_V2_ERR_NONCANONICAL_MANIFEST);

    const char missing_vault[] =
        "{\"app_id\":\"com.alrigroup.sample\","
        "\"execution_profile\":\"sovereign\","
        "\"execution_ring\":\"sovereign_trust\","
        "\"ipc\":{\"allowed_peers\":[\"arws\"]},"
        "\"network\":{\"allow_inbound\":true,\"allow_outbound\":true,\"ports\":[]},"
        "\"resources\":{\"cpu_weight\":100,\"max_threads\":4,\"memory_burst_mb\":128,\"memory_guaranteed_mb\":64},"
        "\"storage\":{\"persistent_mount\":\"var/data/sample\",\"quota_mb\":512},"
        "\"version\":\"1.0.0\"}";

    setup_valid_prefix(&prefix, (uint32_t)strlen(missing_vault), 0U);
    CHECK(arapp_v2_build(&prefix, &sigs, missing_vault, strlen(missing_vault),
                         NULL, 0U, buffer, sizeof(buffer), &written) == ARAPP_V2_OK);
    CHECK(arapp_v2_read(buffer, written, &pkg) == ARAPP_V2_OK);
    CHECK(arapp_v2_validate_manifest(&pkg) == ARAPP_V2_ERR_MANIFEST_SCHEMA);

    /* Test key-ordering injection attack: "a_spoof" containing fake safe app_id */
    const char spoof_injection_manifest[] =
        "{\"a_spoof\":{\"app_id\":\"safe_app\"},"
        "\"app_id\":\"../../../etc/passwd\","
        "\"execution_profile\":\"sovereign\","
        "\"execution_ring\":\"sovereign_trust\","
        "\"ipc\":{\"allowed_peers\":[\"arws\"]},"
        "\"network\":{\"allow_inbound\":true,\"allow_outbound\":true,\"ports\":[]},"
        "\"resources\":{\"cpu_weight\":100,\"max_threads\":4,\"memory_burst_mb\":128,\"memory_guaranteed_mb\":64},"
        "\"storage\":{\"persistent_mount\":\"var/data/sample\",\"quota_mb\":512},"
        "\"vault\":{\"secrets\":[\"SECRET_KEY\"]},"
        "\"version\":\"1.0.0\"}";

    setup_valid_prefix(&prefix, (uint32_t)strlen(spoof_injection_manifest), 0U);
    CHECK(arapp_v2_build(&prefix, &sigs, spoof_injection_manifest, strlen(spoof_injection_manifest),
                         NULL, 0U, buffer, sizeof(buffer), &written) == ARAPP_V2_OK);
    CHECK(arapp_v2_read(buffer, written, &pkg) == ARAPP_V2_OK);
    CHECK(arapp_v2_validate_manifest(&pkg) == ARAPP_V2_ERR_MANIFEST_SCHEMA);

    /* Test absolute path in persistent_mount: /etc/shadow or Windows C:\path */
    const char abs_mount_manifest[] =
        "{\"app_id\":\"com.alrigroup.sample\","
        "\"execution_profile\":\"sovereign\","
        "\"execution_ring\":\"sovereign_trust\","
        "\"ipc\":{\"allowed_peers\":[\"arws\"]},"
        "\"network\":{\"allow_inbound\":true,\"allow_outbound\":true,\"ports\":[]},"
        "\"resources\":{\"cpu_weight\":100,\"max_threads\":4,\"memory_burst_mb\":128,\"memory_guaranteed_mb\":64},"
        "\"storage\":{\"persistent_mount\":\"/etc/shadow\",\"quota_mb\":512},"
        "\"vault\":{\"secrets\":[\"SECRET_KEY\"]},"
        "\"version\":\"1.0.0\"}";

    setup_valid_prefix(&prefix, (uint32_t)strlen(abs_mount_manifest), 0U);
    CHECK(arapp_v2_build(&prefix, &sigs, abs_mount_manifest, strlen(abs_mount_manifest),
                         NULL, 0U, buffer, sizeof(buffer), &written) == ARAPP_V2_OK);
    CHECK(arapp_v2_read(buffer, written, &pkg) == ARAPP_V2_OK);
    CHECK(arapp_v2_validate_manifest(&pkg) == ARAPP_V2_ERR_MANIFEST_SCHEMA);
}

static void test_acceptance_anti_replay_timestamps(void) {
    arapp_v2_package_t pkg;
    memset(&pkg, 0, sizeof(pkg));

    pkg.timestamp_issued = 1700000000ULL;
    pkg.timestamp_expiry = 1700086400ULL;

    CHECK(arapp_v2_validate_anti_replay(&pkg, 1700043200ULL, 0U) == ARAPP_V2_OK);

    CHECK(arapp_v2_validate_anti_replay(&pkg, 1699999999ULL, 0U) == ARAPP_V2_ERR_TIMESTAMP_FUTURE);
    CHECK(arapp_v2_validate_anti_replay(&pkg, 1699999999ULL, 10U) == ARAPP_V2_OK);

    CHECK(arapp_v2_validate_anti_replay(&pkg, 1700086401ULL, 0U) == ARAPP_V2_ERR_TIMESTAMP_EXPIRED);
    CHECK(arapp_v2_validate_anti_replay(&pkg, 1700086401ULL, 10U) == ARAPP_V2_OK);

    pkg.timestamp_issued = 1700086400ULL;
    pkg.timestamp_expiry = 1700000000ULL;
    CHECK(arapp_v2_validate_anti_replay(&pkg, 1700043200ULL, 0U) == ARAPP_V2_ERR_TIMESTAMP_ORDER);

    pkg.timestamp_issued = 0ULL;
    pkg.timestamp_expiry = 1700086400ULL;
    CHECK(arapp_v2_validate_anti_replay(&pkg, 1700043200ULL, 0U) == ARAPP_V2_ERR_TIMESTAMP_ORDER);
}

static void test_cryptographic_signatures(void) {
    arapp_v2_header_prefix_t prefix;
    arapp_v2_signatures_t sigs;
    arapp_v2_package_t pkg;
    uint8_t buffer[16384];
    size_t written = 0U;
    uint8_t digest[ARAPP_V2_SHA512_LEN];

    uint8_t ed_pub[ED25519_PUBLIC_KEY_LEN];
    uint8_t ed_priv[64];
    uint8_t ml_pub[ML_DSA_65_PUBLIC_KEY_LEN];
    uint8_t ml_priv[ML_DSA_65_SECRET_KEY_LEN];

    memset(ed_priv, 0x77, sizeof(ed_priv));
    memset(ed_pub, 0x88, sizeof(ed_pub));
    memset(ml_priv, 0x33, sizeof(ml_priv));
    memset(ml_pub, 0x44, sizeof(ml_pub));
    memset(&sigs, 0, sizeof(sigs));

    if (alrios_ml_dsa_65_provider_available()) {
        CHECK(alrios_ml_dsa_65_keypair(ml_pub, ml_priv) == ALRIOS_CRYPTO_OK);
    }

    /* Generate valid Ed25519 keypair */
    EVP_PKEY_CTX *ed_kctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
    CHECK(ed_kctx != NULL);
    CHECK(EVP_PKEY_keygen_init(ed_kctx) == 1);
    EVP_PKEY *ed_pkey = NULL;
    CHECK(EVP_PKEY_keygen(ed_kctx, &ed_pkey) == 1);
    EVP_PKEY_CTX_free(ed_kctx);

    size_t ed_pub_len = sizeof(ed_pub);
    CHECK(EVP_PKEY_get_raw_public_key(ed_pkey, ed_pub, &ed_pub_len) == 1);
    CHECK(ed_pub_len == sizeof(ed_pub));

    const uint8_t payload[32] = "Test payload for signature v2";
    setup_valid_prefix(&prefix, (uint32_t)strlen(valid_canonical_manifest), sizeof(payload));

    CHECK(arapp_v2_build(&prefix, &sigs, valid_canonical_manifest, strlen(valid_canonical_manifest),
                         payload, sizeof(payload), buffer, sizeof(buffer), &written) == ARAPP_V2_OK);
    CHECK(arapp_v2_read(buffer, written, &pkg) == ARAPP_V2_OK);
    CHECK(arapp_v2_compute_digest(&pkg, digest) == ARAPP_V2_OK);

    /* Test rejecting when both keys are NULL */
    CHECK(arapp_v2_verify_signatures(&pkg, NULL, NULL) == ARAPP_V2_ERR_INVALID_ARGUMENT);

    /* Sign digest with Ed25519 */
    EVP_MD_CTX *sign_ctx = EVP_MD_CTX_new();
    CHECK(sign_ctx != NULL);
    CHECK(EVP_DigestSignInit(sign_ctx, NULL, NULL, NULL, ed_pkey) == 1);
    size_t ed_sig_len = sizeof(sigs.sig_ed25519);
    CHECK(EVP_DigestSign(sign_ctx, sigs.sig_ed25519, &ed_sig_len, digest, sizeof(digest)) == 1);
    EVP_MD_CTX_free(sign_ctx);
    EVP_PKEY_free(ed_pkey);

    memcpy(buffer + ARAPP_V2_HEADER_PREFIX_SIZE + offsetof(arapp_v2_signatures_t, sig_ed25519),
           sigs.sig_ed25519, ARAPP_V2_ED25519_SIG_LEN);

    /* When PQC_HYBRID_SIG is set, both keys are strictly required */
    CHECK(arapp_v2_verify_signatures(&pkg, ed_pub, NULL) == ARAPP_V2_ERR_SIGNATURE_INVALID);
    CHECK(arapp_v2_verify_signatures(&pkg, NULL, ml_pub) == ARAPP_V2_ERR_SIGNATURE_INVALID);

    if (alrios_ml_dsa_65_provider_available()) {
        CHECK(alrios_ml_dsa_65_sign(ml_priv, digest, sigs.sig_ml_dsa_65) == ALRIOS_CRYPTO_OK);
        memcpy(buffer + ARAPP_V2_HEADER_PREFIX_SIZE + offsetof(arapp_v2_signatures_t, sig_ml_dsa_65),
               sigs.sig_ml_dsa_65, ARAPP_V2_ML_DSA_65_SIG_LEN);

        CHECK(arapp_v2_read(buffer, written, &pkg) == ARAPP_V2_OK);
        CHECK(arapp_v2_verify_signatures(&pkg, ed_pub, ml_pub) == ARAPP_V2_OK);

        buffer[ARAPP_V2_HEADER_PREFIX_SIZE + offsetof(arapp_v2_signatures_t, sig_ml_dsa_65) + 5] ^= 0x01;
        CHECK(arapp_v2_read(buffer, written, &pkg) == ARAPP_V2_OK);
        CHECK(arapp_v2_verify_signatures(&pkg, ed_pub, ml_pub) == ARAPP_V2_ERR_SIGNATURE_INVALID);

        buffer[ARAPP_V2_HEADER_PREFIX_SIZE + offsetof(arapp_v2_signatures_t, sig_ml_dsa_65) + 5] ^= 0x01;

        buffer[written - 1] ^= 0xFF;
        CHECK(arapp_v2_read(buffer, written, &pkg) == ARAPP_V2_OK);
        CHECK(arapp_v2_verify_signatures(&pkg, ed_pub, ml_pub) == ARAPP_V2_ERR_SIGNATURE_INVALID);
    }
}

static void test_acceptance_fuzz_harness(void) {
    arapp_v2_header_prefix_t prefix;
    arapp_v2_signatures_t sigs;
    arapp_v2_package_t pkg;
    uint8_t baseline[8192];
    uint8_t mutated[8192];
    size_t written = 0U;
    size_t i;
    const uint8_t ct[64] = { 0x99 };

    memset(&sigs, 0, sizeof(sigs));
    setup_valid_prefix(&prefix, (uint32_t)strlen(valid_canonical_manifest), sizeof(ct));
    CHECK(arapp_v2_build(&prefix, &sigs, valid_canonical_manifest, strlen(valid_canonical_manifest),
                         ct, sizeof(ct), baseline, sizeof(baseline), &written) == ARAPP_V2_OK);

    uint32_t prng_state = 0x12345678U;
    for (i = 0U; i < 5000U; ++i) {
        prng_state = prng_state * 1664525U + 1013904223U;
        size_t mut_len = (size_t)(prng_state % (written + 64U));
        memcpy(mutated, baseline, (mut_len < written) ? mut_len : written);

        size_t corruptions = (prng_state >> 16U) % 8U + 1U;
        size_t c;
        for (c = 0U; c < corruptions && mut_len > 0U; ++c) {
            prng_state = prng_state * 1664525U + 1013904223U;
            size_t pos = (size_t)(prng_state % mut_len);
            mutated[pos] ^= (uint8_t)(prng_state >> 24U);
        }

        (void)arapp_v2_read(mutated, mut_len, &pkg);
        (void)arapp_v2_read_and_validate(mutated, mut_len, 1700005000ULL, &pkg);
    }
}

int main(void) {
    test_positive_canonical_reader();
    test_acceptance_overflow_safe_offsets();
    test_acceptance_canonical_manifest();
    test_acceptance_anti_replay_timestamps();
    test_cryptographic_signatures();
    test_acceptance_fuzz_harness();

    (void)printf("MP-006 (Canonical .arapp v2 reader and structural validator): PASS\n");
    return 0;
}
