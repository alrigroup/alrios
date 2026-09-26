/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/pki/issuer.h"
#include "alrios/crypto_verify.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <openssl/rand.h>

static int generate_ed25519_keypair(EVP_PKEY **out_pkey, uint8_t raw_pub[32]) {
    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
    if (!pctx) return ALRIOS_ISSUER_ERR_KEYGEN_FAILED;

    if (EVP_PKEY_keygen_init(pctx) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return ALRIOS_ISSUER_ERR_KEYGEN_FAILED;
    }

    EVP_PKEY *pkey = NULL;
    if (EVP_PKEY_keygen(pctx, &pkey) <= 0 || !pkey) {
        EVP_PKEY_CTX_free(pctx);
        return ALRIOS_ISSUER_ERR_KEYGEN_FAILED;
    }
    EVP_PKEY_CTX_free(pctx);

    size_t pub_len = 32;
    if (EVP_PKEY_get_raw_public_key(pkey, raw_pub, &pub_len) <= 0 || pub_len != 32) {
        EVP_PKEY_free(pkey);
        return ALRIOS_ISSUER_ERR_KEYGEN_FAILED;
    }

    *out_pkey = pkey;
    return ALRIOS_ISSUER_OK;
}

static int sign_tbs(EVP_PKEY *signing_key, const uint8_t *tbs, size_t tbs_len, uint8_t sig[64]) {
    EVP_MD_CTX *mctx = EVP_MD_CTX_new();
    if (!mctx) return ALRIOS_ISSUER_ERR_SIGN_FAILED;

    if (EVP_DigestSignInit(mctx, NULL, NULL, NULL, signing_key) <= 0) {
        EVP_MD_CTX_free(mctx);
        return ALRIOS_ISSUER_ERR_SIGN_FAILED;
    }

    size_t sig_len = 64;
    if (EVP_DigestSign(mctx, sig, &sig_len, tbs, tbs_len) <= 0 || sig_len != 64) {
        EVP_MD_CTX_free(mctx);
        return ALRIOS_ISSUER_ERR_SIGN_FAILED;
    }

    EVP_MD_CTX_free(mctx);
    return ALRIOS_ISSUER_OK;
}

int alrios_pki_issue_root(const char *subject,
                          uint64_t valid_days,
                          EVP_PKEY **out_pkey,
                          alrios_certificate_t *out_cert,
                          uint8_t *out_wire,
                          size_t out_capacity,
                          size_t *out_wire_len) {
    if (!subject || !out_pkey || !out_cert || !out_wire || !out_wire_len)
        return ALRIOS_ISSUER_ERR_INVALID_ARGUMENT;

    size_t sub_len = strlen(subject);
    if (sub_len == 0 || sub_len > ALRIOS_CERTIFICATE_SUBJECT_MAX)
        return ALRIOS_ISSUER_ERR_INVALID_ARGUMENT;

    uint8_t raw_pub[32];
    EVP_PKEY *pkey = NULL;
    int rc = generate_ed25519_keypair(&pkey, raw_pub);
    if (rc != ALRIOS_ISSUER_OK) return rc;

    memset(out_cert, 0, sizeof(*out_cert));
    out_cert->version = ALRIOS_CERTIFICATE_VERSION_1;
    out_cert->role = ALRIOS_CERTIFICATE_ROLE_ROOT;
    out_cert->public_key_algorithm = ALRIOS_PUBLIC_KEY_ALGORITHM_ED25519;
    out_cert->signature_algorithm = ALRIOS_SIGNATURE_ALGORITHM_ED25519;

    if (RAND_bytes(out_cert->serial, ALRIOS_CERTIFICATE_SERIAL_SIZE) <= 0) {
        EVP_PKEY_free(pkey);
        return ALRIOS_ISSUER_ERR_KEYGEN_FAILED;
    }

    uint64_t now = (uint64_t)time(NULL);
    out_cert->not_before = now;
    out_cert->not_after = now + (valid_days * 86400ULL);

    out_cert->subject_len = sub_len;
    memcpy(out_cert->subject, subject, sub_len);

    out_cert->public_key_len = 32;
    memcpy(out_cert->public_key, raw_pub, 32);
    out_cert->signature_len = 64;

    /* Compute key id */
    if (alrios_certificate_public_key_id(out_cert, out_cert->issuer_key_id) != 0) {
        EVP_PKEY_free(pkey);
        return ALRIOS_ISSUER_ERR_KEYGEN_FAILED;
    }

    /* Encode TBS */
    uint8_t tbs[ALRIOS_CERTIFICATE_TBS_MAX];
    size_t tbs_len = 0;
    if (alrios_certificate_encode_tbs(out_cert, tbs, sizeof(tbs), &tbs_len) != 0) {
        EVP_PKEY_free(pkey);
        return ALRIOS_ISSUER_ERR_ENCODE_FAILED;
    }

    /* Self-sign TBS */
    if (sign_tbs(pkey, tbs, tbs_len, out_cert->signature) != ALRIOS_ISSUER_OK) {
        EVP_PKEY_free(pkey);
        return ALRIOS_ISSUER_ERR_SIGN_FAILED;
    }

    /* Encode full wire format */
    if (alrios_certificate_encode(out_cert, out_wire, out_capacity, out_wire_len) != 0) {
        EVP_PKEY_free(pkey);
        return ALRIOS_ISSUER_ERR_ENCODE_FAILED;
    }

    *out_pkey = pkey;
    return ALRIOS_ISSUER_OK;
}

int alrios_pki_issue_intermediate(const char *subject,
                                  uint64_t valid_days,
                                  const alrios_certificate_t *root_cert,
                                  EVP_PKEY *root_pkey,
                                  EVP_PKEY **out_inter_pkey,
                                  alrios_certificate_t *out_cert,
                                  uint8_t *out_wire,
                                  size_t out_capacity,
                                  size_t *out_wire_len) {
    if (!subject || !root_cert || !root_pkey || !out_inter_pkey || !out_cert || !out_wire || !out_wire_len)
        return ALRIOS_ISSUER_ERR_INVALID_ARGUMENT;

    size_t sub_len = strlen(subject);
    if (sub_len == 0 || sub_len > ALRIOS_CERTIFICATE_SUBJECT_MAX)
        return ALRIOS_ISSUER_ERR_INVALID_ARGUMENT;

    uint8_t raw_pub[32];
    EVP_PKEY *pkey = NULL;
    int rc = generate_ed25519_keypair(&pkey, raw_pub);
    if (rc != ALRIOS_ISSUER_OK) return rc;

    memset(out_cert, 0, sizeof(*out_cert));
    out_cert->version = ALRIOS_CERTIFICATE_VERSION_1;
    out_cert->role = ALRIOS_CERTIFICATE_ROLE_INTERMEDIATE;
    out_cert->public_key_algorithm = ALRIOS_PUBLIC_KEY_ALGORITHM_ED25519;
    out_cert->signature_algorithm = ALRIOS_SIGNATURE_ALGORITHM_ED25519;

    if (RAND_bytes(out_cert->serial, ALRIOS_CERTIFICATE_SERIAL_SIZE) <= 0) {
        EVP_PKEY_free(pkey);
        return ALRIOS_ISSUER_ERR_KEYGEN_FAILED;
    }

    /* Issuer key id is Root's public key id */
    if (alrios_certificate_public_key_id(root_cert, out_cert->issuer_key_id) != 0) {
        EVP_PKEY_free(pkey);
        return ALRIOS_ISSUER_ERR_KEYGEN_FAILED;
    }

    uint64_t now = (uint64_t)time(NULL);
    out_cert->not_before = now;
    out_cert->not_after = now + (valid_days * 86400ULL);

    out_cert->subject_len = sub_len;
    memcpy(out_cert->subject, subject, sub_len);

    out_cert->public_key_len = 32;
    memcpy(out_cert->public_key, raw_pub, 32);
    out_cert->signature_len = 64;

    /* Encode TBS */
    uint8_t tbs[ALRIOS_CERTIFICATE_TBS_MAX];
    size_t tbs_len = 0;
    if (alrios_certificate_encode_tbs(out_cert, tbs, sizeof(tbs), &tbs_len) != 0) {
        EVP_PKEY_free(pkey);
        return ALRIOS_ISSUER_ERR_ENCODE_FAILED;
    }

    /* Sign with Root private key */
    if (sign_tbs(root_pkey, tbs, tbs_len, out_cert->signature) != ALRIOS_ISSUER_OK) {
        EVP_PKEY_free(pkey);
        return ALRIOS_ISSUER_ERR_SIGN_FAILED;
    }

    /* Encode full wire format */
    if (alrios_certificate_encode(out_cert, out_wire, out_capacity, out_wire_len) != 0) {
        EVP_PKEY_free(pkey);
        return ALRIOS_ISSUER_ERR_ENCODE_FAILED;
    }

    *out_inter_pkey = pkey;
    return ALRIOS_ISSUER_OK;
}

int alrios_pki_issue_leaf(const char *subject,
                          uint64_t valid_days,
                          const alrios_certificate_t *inter_cert,
                          EVP_PKEY *inter_pkey,
                          EVP_PKEY **out_leaf_pkey,
                          alrios_certificate_t *out_cert,
                          uint8_t *out_wire,
                          size_t out_capacity,
                          size_t *out_wire_len) {
    if (!subject || !inter_cert || !inter_pkey || !out_leaf_pkey || !out_cert || !out_wire || !out_wire_len)
        return ALRIOS_ISSUER_ERR_INVALID_ARGUMENT;

    size_t sub_len = strlen(subject);
    if (sub_len == 0 || sub_len > ALRIOS_CERTIFICATE_SUBJECT_MAX)
        return ALRIOS_ISSUER_ERR_INVALID_ARGUMENT;

    uint8_t raw_pub[32];
    EVP_PKEY *pkey = NULL;
    int rc = generate_ed25519_keypair(&pkey, raw_pub);
    if (rc != ALRIOS_ISSUER_OK) return rc;

    memset(out_cert, 0, sizeof(*out_cert));
    out_cert->version = ALRIOS_CERTIFICATE_VERSION_1;
    out_cert->role = ALRIOS_CERTIFICATE_ROLE_LEAF;
    out_cert->public_key_algorithm = ALRIOS_PUBLIC_KEY_ALGORITHM_ED25519;
    out_cert->signature_algorithm = ALRIOS_SIGNATURE_ALGORITHM_ED25519;

    if (RAND_bytes(out_cert->serial, ALRIOS_CERTIFICATE_SERIAL_SIZE) <= 0) {
        EVP_PKEY_free(pkey);
        return ALRIOS_ISSUER_ERR_KEYGEN_FAILED;
    }

    /* Issuer key id is Intermediate's public key id */
    if (alrios_certificate_public_key_id(inter_cert, out_cert->issuer_key_id) != 0) {
        EVP_PKEY_free(pkey);
        return ALRIOS_ISSUER_ERR_KEYGEN_FAILED;
    }

    uint64_t now = (uint64_t)time(NULL);
    out_cert->not_before = now;
    out_cert->not_after = now + (valid_days * 86400ULL);

    out_cert->subject_len = sub_len;
    memcpy(out_cert->subject, subject, sub_len);

    out_cert->public_key_len = 32;
    memcpy(out_cert->public_key, raw_pub, 32);
    out_cert->signature_len = 64;

    /* Encode TBS */
    uint8_t tbs[ALRIOS_CERTIFICATE_TBS_MAX];
    size_t tbs_len = 0;
    if (alrios_certificate_encode_tbs(out_cert, tbs, sizeof(tbs), &tbs_len) != 0) {
        EVP_PKEY_free(pkey);
        return ALRIOS_ISSUER_ERR_ENCODE_FAILED;
    }

    /* Sign with Intermediate private key */
    if (sign_tbs(inter_pkey, tbs, tbs_len, out_cert->signature) != ALRIOS_ISSUER_OK) {
        EVP_PKEY_free(pkey);
        return ALRIOS_ISSUER_ERR_SIGN_FAILED;
    }

    /* Encode full wire format */
    if (alrios_certificate_encode(out_cert, out_wire, out_capacity, out_wire_len) != 0) {
        EVP_PKEY_free(pkey);
        return ALRIOS_ISSUER_ERR_ENCODE_FAILED;
    }

    *out_leaf_pkey = pkey;
    return ALRIOS_ISSUER_OK;
}

static int verify_cert_signature(const alrios_certificate_t *child, const alrios_certificate_t *parent) {
    uint8_t tbs[ALRIOS_CERTIFICATE_TBS_MAX];
    size_t tbs_len = 0;
    if (alrios_certificate_encode_tbs(child, tbs, sizeof(tbs), &tbs_len) != 0)
        return ALRIOS_ISSUER_ERR_VERIFY_FAILED;

    EVP_PKEY *parent_pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL,
                                                         parent->public_key, parent->public_key_len);
    if (!parent_pkey) return ALRIOS_ISSUER_ERR_VERIFY_FAILED;

    EVP_MD_CTX *mctx = EVP_MD_CTX_new();
    if (!mctx) {
        EVP_PKEY_free(parent_pkey);
        return ALRIOS_ISSUER_ERR_VERIFY_FAILED;
    }

    if (EVP_DigestVerifyInit(mctx, NULL, NULL, NULL, parent_pkey) <= 0) {
        EVP_MD_CTX_free(mctx);
        EVP_PKEY_free(parent_pkey);
        return ALRIOS_ISSUER_ERR_VERIFY_FAILED;
    }

    int rc = EVP_DigestVerify(mctx, child->signature, child->signature_len, tbs, tbs_len);
    EVP_MD_CTX_free(mctx);
    EVP_PKEY_free(parent_pkey);

    return (rc == 1) ? ALRIOS_ISSUER_OK : ALRIOS_ISSUER_ERR_VERIFY_FAILED;
}

int alrios_pki_verify_chain(const alrios_trust_store_t *store,
                            const alrios_certificate_t *leaf_cert,
                            const alrios_certificate_t *inter_cert,
                            uint64_t current_time) {
    if (!store || !leaf_cert || !inter_cert)
        return ALRIOS_ISSUER_ERR_INVALID_ARGUMENT;

    /* 1. Validate temporal windows */
    if (current_time < leaf_cert->not_before || current_time > leaf_cert->not_after)
        return ALRIOS_ISSUER_ERR_EXPIRED;
    if (current_time < inter_cert->not_before || current_time > inter_cert->not_after)
        return ALRIOS_ISSUER_ERR_EXPIRED;

    /* 2. Validate roles */
    if (leaf_cert->role != ALRIOS_CERTIFICATE_ROLE_LEAF)
        return ALRIOS_ISSUER_ERR_CHAIN_INVALID;
    if (inter_cert->role != ALRIOS_CERTIFICATE_ROLE_INTERMEDIATE)
        return ALRIOS_ISSUER_ERR_CHAIN_INVALID;

    /* 3. Lookup Root CA anchor in trust store by Intermediate's issuer_key_id */
    const alrios_certificate_t *root_anchor = NULL;
    if (alrios_trust_store_lookup(store, inter_cert->issuer_key_id, &root_anchor) != ALRIOS_CERTIFICATE_OK || !root_anchor) {
        return ALRIOS_ISSUER_ERR_CHAIN_INVALID;
    }

    if (current_time < root_anchor->not_before || current_time > root_anchor->not_after)
        return ALRIOS_ISSUER_ERR_EXPIRED;

    /* 4. Match Key IDs */
    uint8_t inter_key_id[ALRIOS_CERTIFICATE_KEY_ID_SIZE];
    if (alrios_certificate_public_key_id(inter_cert, inter_key_id) != 0)
        return ALRIOS_ISSUER_ERR_CHAIN_INVALID;

    if (alrios_constant_time_memcmp(leaf_cert->issuer_key_id, inter_key_id, ALRIOS_CERTIFICATE_KEY_ID_SIZE) != 0)
        return ALRIOS_ISSUER_ERR_CHAIN_INVALID;

    /* 5. Cryptographic signature check: Leaf signed by Intermediate */
    if (verify_cert_signature(leaf_cert, inter_cert) != ALRIOS_ISSUER_OK)
        return ALRIOS_ISSUER_ERR_VERIFY_FAILED;

    /* 6. Cryptographic signature check: Intermediate signed by Root */
    if (verify_cert_signature(inter_cert, root_anchor) != ALRIOS_ISSUER_OK)
        return ALRIOS_ISSUER_ERR_VERIFY_FAILED;

    return ALRIOS_ISSUER_OK;
}
