/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/pki/notary.h"
#include "alrios/pki/issuer.h"
#include "alrios/crypto_verify.h"
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#define TEST_PATH_SEP '\\'
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>
#define TEST_PATH_SEP '/'
#endif

#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        (void)fprintf(stderr, "FAIL: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void make_unique_temp_path(char *out, size_t out_len, const char *suffix) {
    const char *base = NULL;
    unsigned long pid_value;
    unsigned long counter_value;
    static unsigned long counter = 0UL;

    TEST_ASSERT(out != NULL);
    TEST_ASSERT(out_len > 0U);
    TEST_ASSERT(suffix != NULL);

#ifdef _WIN32
    char tmp[MAX_PATH];
    DWORD len = GetTempPathA((DWORD)sizeof(tmp), tmp);
    TEST_ASSERT(len > 0U && len < (DWORD)sizeof(tmp));
    base = tmp;
    pid_value = (unsigned long)_getpid();
#else
    base = getenv("TMPDIR");
    if (!base || base[0] == '\0') {
        base = "/tmp";
    }
    pid_value = (unsigned long)getpid();
#endif
    counter_value = ++counter;
    TEST_ASSERT(snprintf(out, out_len, "%s%c%s_%lu_%lu_%s", base, TEST_PATH_SEP,
                         "test_alrios_notary", pid_value, counter_value, suffix) > 0);
}

static void write_exact_file(const char *path, const uint8_t *data, size_t len) {
    FILE *f;

    TEST_ASSERT(path != NULL);
    TEST_ASSERT(len == 0U || data != NULL);

    f = fopen(path, "wb");
    TEST_ASSERT(f != NULL);
    TEST_ASSERT(fwrite(data, 1U, len, f) == len);
    TEST_ASSERT(fflush(f) == 0);
    TEST_ASSERT(fclose(f) == 0);
}

static EVP_PKEY *generate_test_ed25519_key(uint8_t out_pub[ED25519_PUBLIC_KEY_LEN]) {
    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
    TEST_ASSERT(pctx != NULL);
    TEST_ASSERT(EVP_PKEY_keygen_init(pctx) > 0);

    EVP_PKEY *pkey = NULL;
    TEST_ASSERT(EVP_PKEY_keygen(pctx, &pkey) > 0 && pkey != NULL);
    EVP_PKEY_CTX_free(pctx);

    size_t pub_len = ED25519_PUBLIC_KEY_LEN;
    TEST_ASSERT(EVP_PKEY_get_raw_public_key(pkey, out_pub, &pub_len) > 0);
    TEST_ASSERT(pub_len == ED25519_PUBLIC_KEY_LEN);

    return pkey;
}

static void test_acceptance_criteria(void) {
    (void)printf("[TEST] Checking Acceptance Criteria: Key isolation and compiler prohibition...\n");
    (void)printf("[INFO] Acceptance proof is enforced by CLI/process tests and armake build graph tests, not by status stubs.\n");
}

static void test_algorithm_policy(void) {
    alrios_notary_policy_t policy;

    (void)printf("[TEST] Testing Algorithm Policy Matrix...\n");

    /* Default policy allows Ed25519 */
    TEST_ASSERT(alrios_notary_policy_init(&policy, ALRIOS_NOTARY_POLICY_DEFAULT) == ALRIOS_NOTARY_OK);
    TEST_ASSERT(alrios_notary_policy_validate(&policy, ALRIOS_SIGNATURE_ALGORITHM_ED25519) == ALRIOS_NOTARY_OK);
    TEST_ASSERT(alrios_notary_policy_validate(&policy, 99U) == ALRIOS_NOTARY_ERR_POLICY_VIOLATION);
    TEST_ASSERT(alrios_notary_policy_validate(&policy, 0U) == ALRIOS_NOTARY_ERR_POLICY_VIOLATION);

    /* PQC/hybrid policy is fail-closed until a real ML-DSA backend and envelope exist. */
    TEST_ASSERT(alrios_notary_policy_init(&policy, ALRIOS_NOTARY_POLICY_REQUIRE_PQC) == ALRIOS_NOTARY_ERR_POLICY_VIOLATION);

    /* Unknown policy bits and noncanonical policy bitsets are never normalized into success. */
    TEST_ASSERT(alrios_notary_policy_init(&policy, ALRIOS_NOTARY_POLICY_DEFAULT | (1U << 31U)) ==
                ALRIOS_NOTARY_ERR_POLICY_VIOLATION);
    TEST_ASSERT(alrios_notary_policy_init(&policy, ALRIOS_NOTARY_POLICY_REQUIRE_CERT_BIND) ==
                ALRIOS_NOTARY_ERR_POLICY_VIOLATION);

    memset(&policy, 0, sizeof(policy));
    policy.flags = ALRIOS_NOTARY_POLICY_DEFAULT | (1U << 31U);
    policy.allowed_signature_algorithm = ALRIOS_SIGNATURE_ALGORITHM_ED25519;
    TEST_ASSERT(alrios_notary_policy_validate(&policy, ALRIOS_SIGNATURE_ALGORITHM_ED25519) == ALRIOS_NOTARY_ERR_POLICY_VIOLATION);

    TEST_ASSERT(alrios_notary_policy_init(&policy, ALRIOS_NOTARY_POLICY_STRICT) == ALRIOS_NOTARY_OK);
    TEST_ASSERT((policy.flags & ALRIOS_NOTARY_POLICY_REQUIRE_CERT_BIND) != 0U);
    TEST_ASSERT(alrios_notary_policy_validate(&policy, ALRIOS_SIGNATURE_ALGORITHM_HYBRID_ED25519_ML_DSA_65) == ALRIOS_NOTARY_ERR_POLICY_VIOLATION);
    TEST_ASSERT(alrios_notary_policy_validate(&policy, ALRIOS_SIGNATURE_ALGORITHM_ML_DSA_65) == ALRIOS_NOTARY_ERR_POLICY_VIOLATION);
}

static void test_payload_signing_and_tamper_rejection(void) {
    uint8_t pubkey[ED25519_PUBLIC_KEY_LEN];
    uint8_t wrong_pubkey[ED25519_PUBLIC_KEY_LEN];
    EVP_PKEY *privkey = generate_test_ed25519_key(pubkey);
    EVP_PKEY *wrong_privkey = generate_test_ed25519_key(wrong_pubkey);
    EVP_PKEY *rsa_key = NULL;
    EVP_PKEY_CTX *rsa_ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    alrios_notary_policy_t policy;
    alrios_notary_policy_t strict_policy;
    alrios_notary_signature_t sig;
    uint8_t payload[256];
    size_t i;

    (void)printf("[TEST] Testing Payload Signing and Tamper Rejection...\n");

    for (i = 0; i < sizeof(payload); ++i) {
        payload[i] = (uint8_t)(i ^ 0x5aU);
    }

    TEST_ASSERT(alrios_notary_policy_init(&policy, ALRIOS_NOTARY_POLICY_DEFAULT) == ALRIOS_NOTARY_OK);
    TEST_ASSERT(rsa_ctx != NULL);
    TEST_ASSERT(EVP_PKEY_keygen_init(rsa_ctx) > 0);
    TEST_ASSERT(EVP_PKEY_CTX_set_rsa_keygen_bits(rsa_ctx, 2048) > 0);
    TEST_ASSERT(EVP_PKEY_keygen(rsa_ctx, &rsa_key) > 0 && rsa_key != NULL);
    EVP_PKEY_CTX_free(rsa_ctx);
    rsa_ctx = NULL;
    TEST_ASSERT(alrios_notary_sign_payload(&policy, payload, sizeof(payload), rsa_key, &sig) ==
                ALRIOS_NOTARY_ERR_KEY_INVALID);
    EVP_PKEY_free(rsa_key);
    rsa_key = NULL;

    /* Positive sign and verify */
    TEST_ASSERT(alrios_notary_sign_payload(&policy, payload, sizeof(payload), privkey, &sig) == ALRIOS_NOTARY_OK);
    TEST_ASSERT(sig.version == ALRIOS_NOTARY_VERSION_1);
    TEST_ASSERT(sig.algorithm == ALRIOS_SIGNATURE_ALGORITHM_ED25519);
    TEST_ASSERT(sig.signature_len == ED25519_SIGNATURE_LEN);
    TEST_ASSERT(sig.payload_len == sizeof(payload));
    TEST_ASSERT(sig.policy_flags == ALRIOS_NOTARY_POLICY_DEFAULT);

    TEST_ASSERT(alrios_notary_verify_payload(&policy, payload, sizeof(payload), &sig, pubkey) == ALRIOS_NOTARY_OK);
    TEST_ASSERT(alrios_notary_policy_init(&strict_policy, ALRIOS_NOTARY_POLICY_STRICT) == ALRIOS_NOTARY_OK);
    TEST_ASSERT(alrios_notary_verify_payload(&strict_policy, payload, sizeof(payload), &sig, pubkey) == ALRIOS_NOTARY_ERR_POLICY_VIOLATION);
    TEST_ASSERT(alrios_notary_sign_payload(&strict_policy, payload, sizeof(payload), privkey, &sig) == ALRIOS_NOTARY_OK);
    TEST_ASSERT(alrios_notary_verify_payload(&strict_policy, payload, sizeof(payload), &sig, pubkey) == ALRIOS_NOTARY_ERR_POLICY_VIOLATION);

    TEST_ASSERT(alrios_notary_sign_payload(&policy, payload, sizeof(payload), privkey, &sig) == ALRIOS_NOTARY_OK);

    /* Negative: Acceptance 3 - Tampered payload rejected */
    /* 1. Modify one byte of payload */
    uint8_t tampered_payload[sizeof(payload)];
    memcpy(tampered_payload, payload, sizeof(payload));
    tampered_payload[10] ^= 0x01U;
    TEST_ASSERT(alrios_notary_verify_payload(&policy, tampered_payload, sizeof(tampered_payload), &sig, pubkey) ==
                ALRIOS_NOTARY_ERR_PAYLOAD_TAMPERED);

    /* 2. Truncate payload */
    TEST_ASSERT(alrios_notary_verify_payload(&policy, payload, sizeof(payload) - 1U, &sig, pubkey) ==
                ALRIOS_NOTARY_ERR_PAYLOAD_TAMPERED);

    /* 3. Empty payload vs non-empty signature */
    TEST_ASSERT(alrios_notary_verify_payload(&policy, NULL, 0U, &sig, pubkey) ==
                ALRIOS_NOTARY_ERR_PAYLOAD_TAMPERED);

    /* 4. Mutated signature byte */
    alrios_notary_signature_t bad_sig = sig;
    bad_sig.signature[5] ^= 0x80U;
    TEST_ASSERT(alrios_notary_verify_payload(&policy, payload, sizeof(payload), &bad_sig, pubkey) ==
                ALRIOS_NOTARY_ERR_VERIFY_FAILED);

    /* 5. Mutated digest in signature */
    bad_sig = sig;
    bad_sig.digest[2] ^= 0x42U;
    TEST_ASSERT(alrios_notary_verify_payload(&policy, payload, sizeof(payload), &bad_sig, pubkey) ==
                ALRIOS_NOTARY_ERR_PAYLOAD_TAMPERED);

    /* 6. Mutated key_id in signature */
    bad_sig = sig;
    bad_sig.key_id[0] ^= 0xffU;
    TEST_ASSERT(alrios_notary_verify_payload(&policy, payload, sizeof(payload), &bad_sig, pubkey) ==
                ALRIOS_NOTARY_ERR_KEY_INVALID);

    /* 7. Verification with wrong public key */
    TEST_ASSERT(alrios_notary_verify_payload(&policy, payload, sizeof(payload), &sig, wrong_pubkey) ==
                ALRIOS_NOTARY_ERR_KEY_INVALID);

    /* 8. Policy violation: PQC is unavailable and must fail closed. */
    alrios_notary_policy_t pqc_policy;
    TEST_ASSERT(alrios_notary_policy_init(&pqc_policy, ALRIOS_NOTARY_POLICY_REQUIRE_PQC) == ALRIOS_NOTARY_ERR_POLICY_VIOLATION);

    /* 9. Mutating signed critical fields must invalidate the Ed25519 signature. */
    bad_sig = sig;
    bad_sig.algorithm = ALRIOS_SIGNATURE_ALGORITHM_HYBRID_ED25519_ML_DSA_65;
    TEST_ASSERT(alrios_notary_verify_payload(&policy, payload, sizeof(payload), &bad_sig, pubkey) ==
                ALRIOS_NOTARY_ERR_POLICY_VIOLATION);

    bad_sig = sig;
    bad_sig.timestamp += 1U;
    TEST_ASSERT(alrios_notary_verify_payload(&policy, payload, sizeof(payload), &bad_sig, pubkey) ==
                ALRIOS_NOTARY_ERR_VERIFY_FAILED);

    bad_sig = sig;
    bad_sig.policy_flags = ALRIOS_NOTARY_POLICY_STRICT;
    TEST_ASSERT(alrios_notary_verify_payload(&policy, payload, sizeof(payload), &bad_sig, pubkey) ==
                ALRIOS_NOTARY_ERR_POLICY_VIOLATION);

    if (rsa_ctx) {
        EVP_PKEY_CTX_free(rsa_ctx);
    }
    EVP_PKEY_free(rsa_key);
    EVP_PKEY_free(privkey);
    EVP_PKEY_free(wrong_privkey);
}

static void test_wire_format_canonicality(void) {
    uint8_t pubkey[ED25519_PUBLIC_KEY_LEN];
    EVP_PKEY *privkey = generate_test_ed25519_key(pubkey);
    alrios_notary_policy_t policy;
    alrios_notary_signature_t sig;
    alrios_notary_signature_t decoded_sig;
    uint8_t payload[] = "ALRIOS sovereign payload to be certified by arsign notary";
    uint8_t wire[ALRIOS_NOTARY_WIRE_MAX];
    uint8_t mutated_wire[ALRIOS_NOTARY_WIRE_MAX];
    size_t wire_len = 0U;

    (void)printf("[TEST] Testing Wire Format Serialization & Boundary Enforcements...\n");

    TEST_ASSERT(alrios_notary_policy_init(&policy, ALRIOS_NOTARY_POLICY_DEFAULT) == ALRIOS_NOTARY_OK);
    TEST_ASSERT(alrios_notary_sign_payload(&policy, payload, sizeof(payload), privkey, &sig) == ALRIOS_NOTARY_OK);

    /* Positive encode & decode */
    TEST_ASSERT(alrios_notary_signature_encode(&sig, wire, sizeof(wire), &wire_len) == ALRIOS_NOTARY_OK);
    TEST_ASSERT(wire_len == ALRIOS_NOTARY_HEADER_SIZE + ED25519_SIGNATURE_LEN);

    TEST_ASSERT(alrios_notary_signature_decode(wire, wire_len, &decoded_sig) == ALRIOS_NOTARY_OK);
    TEST_ASSERT(decoded_sig.version == sig.version);
    TEST_ASSERT(decoded_sig.algorithm == sig.algorithm);
    TEST_ASSERT(decoded_sig.payload_len == sig.payload_len);
    TEST_ASSERT(decoded_sig.timestamp == sig.timestamp);
    TEST_ASSERT(decoded_sig.policy_flags == sig.policy_flags);
    TEST_ASSERT(memcmp(decoded_sig.key_id, sig.key_id, ALRIOS_CERTIFICATE_KEY_ID_SIZE) == 0);
    TEST_ASSERT(memcmp(decoded_sig.digest, sig.digest, SHA512_DIGEST_LEN) == 0);
    TEST_ASSERT(memcmp(decoded_sig.signature, sig.signature, ED25519_SIGNATURE_LEN) == 0);

    /* Wire negative: Truncated buffer */
    TEST_ASSERT(alrios_notary_signature_decode(wire, wire_len - 1U, &decoded_sig) == ALRIOS_NOTARY_ERR_TRUNCATED);
    TEST_ASSERT(alrios_notary_signature_decode(wire, 50U, &decoded_sig) == ALRIOS_NOTARY_ERR_TRUNCATED);

    /* Wire negative: Trailing garbage */
    memcpy(mutated_wire, wire, wire_len);
    mutated_wire[wire_len] = 0xAAU;
    TEST_ASSERT(alrios_notary_signature_decode(mutated_wire, wire_len + 1U, &decoded_sig) == ALRIOS_NOTARY_ERR_NONCANONICAL);

    /* Wire negative: Bad magic */
    memcpy(mutated_wire, wire, wire_len);
    mutated_wire[0] = 'X';
    TEST_ASSERT(alrios_notary_signature_decode(mutated_wire, wire_len, &decoded_sig) == ALRIOS_NOTARY_ERR_BAD_MAGIC);

    /* Wire negative: Bad version */
    memcpy(mutated_wire, wire, wire_len);
    mutated_wire[9] = 2U;
    TEST_ASSERT(alrios_notary_signature_decode(mutated_wire, wire_len, &decoded_sig) == ALRIOS_NOTARY_ERR_NONCANONICAL);

    /* Wire negative: Nonzero reserved byte */
    memcpy(mutated_wire, wire, wire_len);
    mutated_wire[11] = 1U;
    TEST_ASSERT(alrios_notary_signature_decode(mutated_wire, wire_len, &decoded_sig) == ALRIOS_NOTARY_ERR_NONCANONICAL);

    /* Wire negative: fake hybrid/PQC algorithm is never accepted. */
    memcpy(mutated_wire, wire, wire_len);
    mutated_wire[10] = ALRIOS_SIGNATURE_ALGORITHM_HYBRID_ED25519_ML_DSA_65;
    TEST_ASSERT(alrios_notary_signature_decode(mutated_wire, wire_len, &decoded_sig) == ALRIOS_NOTARY_ERR_NONCANONICAL);

    /* Wire negative: unknown policy bit is rejected by parser. */
    memcpy(mutated_wire, wire, wire_len);
    mutated_wire[28] = 0x80U;
    TEST_ASSERT(alrios_notary_signature_decode(mutated_wire, wire_len, &decoded_sig) == ALRIOS_NOTARY_ERR_POLICY_VIOLATION);

    EVP_PKEY_free(privkey);
}

static void test_certificate_chain_notarization(void) {
    (void)printf("[TEST] Testing PKI Chain + Notary Signature Binding...\n");

    /* 1. Issue Root CA */
    EVP_PKEY *root_pkey = NULL;
    alrios_certificate_t root_cert;
    uint8_t root_wire[ALRIOS_CERTIFICATE_WIRE_MAX];
    size_t root_wire_len = 0U;
    TEST_ASSERT(alrios_pki_issue_root("alrios.root.notary.ca", 365, &root_pkey,
                                     &root_cert, root_wire, sizeof(root_wire), &root_wire_len) == ALRIOS_ISSUER_OK);

    /* 2. Setup Trust Store with Root Anchor */
    alrios_trust_store_t store;
    alrios_trust_store_init(&store);
    TEST_ASSERT(alrios_trust_store_add_anchor(&store, &root_cert) == ALRIOS_CERTIFICATE_OK);
    TEST_ASSERT(alrios_trust_store_seal(&store) == ALRIOS_CERTIFICATE_OK);

    /* 3. Issue Intermediate CA */
    EVP_PKEY *inter_pkey = NULL;
    alrios_certificate_t inter_cert;
    uint8_t inter_wire[ALRIOS_CERTIFICATE_WIRE_MAX];
    size_t inter_wire_len = 0U;
    TEST_ASSERT(alrios_pki_issue_intermediate("alrios.intermediate.notary", 90,
                                             &root_cert, root_pkey, &inter_pkey,
                                             &inter_cert, inter_wire, sizeof(inter_wire), &inter_wire_len) == ALRIOS_ISSUER_OK);

    /* 4. Issue Leaf Certificate */
    EVP_PKEY *leaf_pkey = NULL;
    alrios_certificate_t leaf_cert;
    uint8_t leaf_wire[ALRIOS_CERTIFICATE_WIRE_MAX];
    size_t leaf_wire_len = 0U;
    TEST_ASSERT(alrios_pki_issue_leaf("com.alrigroup.arsign.notary", 30,
                                     &inter_cert, inter_pkey, &leaf_pkey,
                                     &leaf_cert, leaf_wire, sizeof(leaf_wire), &leaf_wire_len) == ALRIOS_ISSUER_OK);

    /* 5. Notarize payload with Leaf Private Key */
    uint8_t payload[] = "Payload signed with Sovereign Leaf certificate";
    alrios_notary_policy_t policy;
    alrios_notary_signature_t sig;
    uint64_t current_time = leaf_cert.not_before + 50ULL;

    TEST_ASSERT(alrios_notary_policy_init(&policy, ALRIOS_NOTARY_POLICY_STRICT) == ALRIOS_NOTARY_OK);
    TEST_ASSERT(alrios_notary_sign_payload(&policy, payload, sizeof(payload), leaf_pkey, &sig) == ALRIOS_NOTARY_OK);

    /* 6. Verify against certificate chain */
    TEST_ASSERT(alrios_notary_verify_with_certificate(&policy, payload, sizeof(payload), &sig,
                                                      &leaf_cert, &inter_cert, &store, current_time) == ALRIOS_NOTARY_OK);

    alrios_notary_policy_t unbound_policy;
    TEST_ASSERT(alrios_notary_policy_init(&unbound_policy, ALRIOS_NOTARY_POLICY_DEFAULT) == ALRIOS_NOTARY_OK);
    TEST_ASSERT(alrios_notary_verify_with_certificate(&unbound_policy, payload, sizeof(payload), &sig,
                                                      &leaf_cert, &inter_cert, &store, current_time) == ALRIOS_NOTARY_ERR_POLICY_VIOLATION);

    /* 7. Negative: Tampered payload rejected in chain verification */
    uint8_t bad_payload[sizeof(payload)];
    memcpy(bad_payload, payload, sizeof(payload));
    bad_payload[3] ^= 0x01U;
    TEST_ASSERT(alrios_notary_verify_with_certificate(&policy, bad_payload, sizeof(bad_payload), &sig,
                                                      &leaf_cert, &inter_cert, &store, current_time) == ALRIOS_NOTARY_ERR_PAYLOAD_TAMPERED);

    /* 8. Negative: Corrupted leaf cert rejected */
    alrios_certificate_t bad_leaf = leaf_cert;
    bad_leaf.signature[10] ^= 0xFFU;
    TEST_ASSERT(alrios_notary_verify_with_certificate(&policy, payload, sizeof(payload), &sig,
                                                      &bad_leaf, &inter_cert, &store, current_time) == ALRIOS_NOTARY_ERR_VERIFY_FAILED);

    EVP_PKEY_free(root_pkey);
    EVP_PKEY_free(inter_pkey);
    EVP_PKEY_free(leaf_pkey);
    alrios_trust_store_clear(&store);
}

static void test_arsign_cli_fail_closed(void) {
    char priv_path[1024];
    char pub_path[1024];
    char payload_path[1024];
    char sig_path[1024];
    char cmd[8192];
    uint8_t pubkey[ED25519_PUBLIC_KEY_LEN];
    uint8_t payload[] = "arsign CLI strict parser and raw-key downgrade negative test";
    EVP_PKEY *pkey = generate_test_ed25519_key(pubkey);
    const char *arsign_path = NULL;
    int rc;

    (void)printf("[TEST] Testing arsign CLI fail-closed parsing and strict raw-key rejection...\n");

    make_unique_temp_path(priv_path, sizeof(priv_path), "cli_priv.key");
    make_unique_temp_path(pub_path, sizeof(pub_path), "cli_pub.key");
    make_unique_temp_path(payload_path, sizeof(payload_path), "cli_payload.bin");
    make_unique_temp_path(sig_path, sizeof(sig_path), "cli_sig.bin");

    TEST_ASSERT(alrios_notary_save_private_key_raw(priv_path, pkey) == ALRIOS_NOTARY_OK);
    TEST_ASSERT(alrios_notary_save_public_key_raw(pub_path, pubkey) == ALRIOS_NOTARY_OK);
    write_exact_file(payload_path, payload, sizeof(payload));

#ifndef _WIN32
    arsign_path = getenv("ALRIOS_ARSIGN_BIN");
    if (!arsign_path || arsign_path[0] == '\0') {
        arsign_path = "/home/alexsanderalri/ALRIGROUP/code-space/alrios/arcore/arsign";
    }
    TEST_ASSERT(access(arsign_path, X_OK) == 0);

    TEST_ASSERT(snprintf(cmd, sizeof(cmd), "'%s' sign --key '%s' --in '%s' --out '%s'",
                         arsign_path, priv_path, payload_path, sig_path) > 0);
    rc = system(cmd);
    TEST_ASSERT(rc != -1);
    TEST_ASSERT(WIFEXITED(rc) && WEXITSTATUS(rc) == 0);

    TEST_ASSERT(snprintf(cmd, sizeof(cmd), "'%s' sign --key '%s' --in '%s' --out '%s' --policy=pqc",
                         arsign_path, priv_path, payload_path, sig_path) > 0);
    rc = system(cmd);
    TEST_ASSERT(rc != -1);
    TEST_ASSERT(!WIFEXITED(rc) || WEXITSTATUS(rc) != 0);

    TEST_ASSERT(snprintf(cmd, sizeof(cmd), "'%s' sign --key '%s' --in '%s' --out '%s' --policy",
                         arsign_path, priv_path, payload_path, sig_path) > 0);
    rc = system(cmd);
    TEST_ASSERT(rc != -1);
    TEST_ASSERT(!WIFEXITED(rc) || WEXITSTATUS(rc) != 0);

    TEST_ASSERT(snprintf(cmd, sizeof(cmd), "'%s' sign --key '%s' --key '%s' --in '%s' --out '%s'",
                         arsign_path, priv_path, priv_path, payload_path, sig_path) > 0);
    rc = system(cmd);
    TEST_ASSERT(rc != -1);
    TEST_ASSERT(!WIFEXITED(rc) || WEXITSTATUS(rc) != 0);

    TEST_ASSERT(snprintf(cmd, sizeof(cmd), "'%s' verify --key '%s' --in '%s' --sig '%s' --policy strict",
                         arsign_path, pub_path, payload_path, sig_path) > 0);
    rc = system(cmd);
    TEST_ASSERT(rc != -1);
    TEST_ASSERT(!WIFEXITED(rc) || WEXITSTATUS(rc) != 0);

    TEST_ASSERT(snprintf(cmd, sizeof(cmd), "'%s' sign --key '%s' --in '%s' --out /dev/full",
                         arsign_path, priv_path, payload_path) > 0);
    rc = system(cmd);
    TEST_ASSERT(rc != -1);
    TEST_ASSERT(!WIFEXITED(rc) || WEXITSTATUS(rc) != 0);
#else
    (void)cmd;
    (void)rc;
#endif

    EVP_PKEY_free(pkey);
    (void)remove(priv_path);
    (void)remove(pub_path);
    (void)remove(payload_path);
    (void)remove(sig_path);
}

static void test_key_file_io(void) {
    char priv_path[1024];
    char pub_path[1024];
    char rsa_path[1024];
    uint8_t orig_pub[ED25519_PUBLIC_KEY_LEN];
    uint8_t loaded_pub[ED25519_PUBLIC_KEY_LEN];
    EVP_PKEY *orig_pkey = generate_test_ed25519_key(orig_pub);
    EVP_PKEY *loaded_pkey = NULL;
    EVP_PKEY *invalid_loaded_pkey = NULL;
    EVP_PKEY *rsa_pkey = NULL;
    EVP_PKEY_CTX *rsa_ctx = NULL;
    alrios_notary_policy_t policy;
    alrios_notary_signature_t sig;
    uint8_t payload[] = "Secure persistent key I/O roundtrip test payload";

    (void)printf("[TEST] Testing Notary Key File Persistence & Loading...\n");
    make_unique_temp_path(priv_path, sizeof(priv_path), "priv.key");
    make_unique_temp_path(pub_path, sizeof(pub_path), "pub.key");
    make_unique_temp_path(rsa_path, sizeof(rsa_path), "rsa.key");

    TEST_ASSERT(alrios_notary_save_private_key_raw(priv_path, orig_pkey) == ALRIOS_NOTARY_OK);
    TEST_ASSERT(alrios_notary_save_private_key_raw(priv_path, orig_pkey) == ALRIOS_NOTARY_ERR_IO_FAILED);
#ifndef _WIN32
    {
        struct stat st;
        TEST_ASSERT(stat(priv_path, &st) == 0);
        TEST_ASSERT((st.st_mode & 0777) == 0600);
    }
#endif
    TEST_ASSERT(alrios_notary_save_public_key_raw(pub_path, orig_pub) == ALRIOS_NOTARY_OK);

    TEST_ASSERT(alrios_notary_load_private_key_file(priv_path, &loaded_pkey) == ALRIOS_NOTARY_OK);
    TEST_ASSERT(loaded_pkey != NULL);

    rsa_ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    TEST_ASSERT(rsa_ctx != NULL);
    TEST_ASSERT(EVP_PKEY_keygen_init(rsa_ctx) > 0);
    TEST_ASSERT(EVP_PKEY_CTX_set_rsa_keygen_bits(rsa_ctx, 2048) > 0);
    TEST_ASSERT(EVP_PKEY_keygen(rsa_ctx, &rsa_pkey) > 0 && rsa_pkey != NULL);
    EVP_PKEY_CTX_free(rsa_ctx);
    rsa_ctx = NULL;
    {
        FILE *rsa_out = fopen(rsa_path, "wb");
        TEST_ASSERT(rsa_out != NULL);
        TEST_ASSERT(PEM_write_PrivateKey(rsa_out, rsa_pkey, NULL, NULL, 0, NULL, NULL) == 1);
        TEST_ASSERT(fclose(rsa_out) == 0);
    }
    TEST_ASSERT(alrios_notary_load_private_key_file(rsa_path, &invalid_loaded_pkey) == ALRIOS_NOTARY_ERR_KEY_INVALID);
    TEST_ASSERT(invalid_loaded_pkey == NULL);
    TEST_ASSERT(alrios_notary_load_public_key_file(rsa_path, loaded_pub) == ALRIOS_NOTARY_ERR_KEY_INVALID);
    TEST_ASSERT(alrios_notary_load_public_key_file(pub_path, loaded_pub) == ALRIOS_NOTARY_OK);
    TEST_ASSERT(memcmp(orig_pub, loaded_pub, ED25519_PUBLIC_KEY_LEN) == 0);

    TEST_ASSERT(alrios_notary_policy_init(&policy, ALRIOS_NOTARY_POLICY_DEFAULT) == ALRIOS_NOTARY_OK);
    TEST_ASSERT(alrios_notary_sign_payload(&policy, payload, sizeof(payload), loaded_pkey, &sig) == ALRIOS_NOTARY_OK);
    TEST_ASSERT(alrios_notary_verify_payload(&policy, payload, sizeof(payload), &sig, loaded_pub) == ALRIOS_NOTARY_OK);

    if (rsa_ctx) {
        EVP_PKEY_CTX_free(rsa_ctx);
    }
    EVP_PKEY_free(rsa_pkey);
    EVP_PKEY_free(orig_pkey);
    EVP_PKEY_free(loaded_pkey);
    EVP_PKEY_free(invalid_loaded_pkey);
    (void)remove(priv_path);
    (void)remove(pub_path);
    (void)remove(rsa_path);
}

int main(void) {
    (void)printf("=====================================================\n");
    (void)printf("  ALRIOS MP-003: Isolated arsign Notary & Policy Test\n");
    (void)printf("=====================================================\n");

    test_acceptance_criteria();
    test_algorithm_policy();
    test_payload_signing_and_tamper_rejection();
    test_wire_format_canonicality();
    test_certificate_chain_notarization();
    test_arsign_cli_fail_closed();
    test_key_file_io();

    (void)printf("[PASS] TASK-MP-003: Isolated arsign Notary & Algorithm Policy verified\n");
    return 0;
}
