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
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <share.h>
#include <sys/types.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

static const uint8_t g_notary_magic[8] = {
    ALRIOS_NOTARY_MAGIC_0, ALRIOS_NOTARY_MAGIC_1,
    ALRIOS_NOTARY_MAGIC_2, ALRIOS_NOTARY_MAGIC_3,
    ALRIOS_NOTARY_MAGIC_4, ALRIOS_NOTARY_MAGIC_5,
    ALRIOS_NOTARY_MAGIC_6, ALRIOS_NOTARY_MAGIC_7
};

static const uint8_t g_key_id_domain[16] = {
    0x41U, 0x4cU, 0x52U, 0x49U, 0x4fU, 0x53U, 0x2dU, 0x4bU,
    0x45U, 0x59U, 0x2dU, 0x49U, 0x44U, 0x2dU, 0x56U, 0x31U
};

static const uint8_t g_notary_tbs_domain[ALRIOS_NOTARY_TBS_DOMAIN_SIZE] = {
    0x41U, 0x4cU, 0x52U, 0x49U, 0x4fU, 0x53U, 0x2dU, 0x4eU, 0x4fU, 0x54U,
    0x41U, 0x52U, 0x59U, 0x2dU, 0x54U, 0x42U, 0x53U, 0x2dU, 0x56U, 0x31U
};

static void write_u16_be(uint8_t *dest, uint16_t val) {
    dest[0] = (uint8_t)((val >> 8U) & 0xFFU);
    dest[1] = (uint8_t)(val & 0xFFU);
}

static uint16_t read_u16_be(const uint8_t *src) {
    return (uint16_t)(((uint16_t)src[0] << 8U) | (uint16_t)src[1]);
}

static void write_u32_be(uint8_t *dest, uint32_t val) {
    dest[0] = (uint8_t)((val >> 24U) & 0xFFU);
    dest[1] = (uint8_t)((val >> 16U) & 0xFFU);
    dest[2] = (uint8_t)((val >> 8U) & 0xFFU);
    dest[3] = (uint8_t)(val & 0xFFU);
}

static uint32_t read_u32_be(const uint8_t *src) {
    return ((uint32_t)src[0] << 24U) | ((uint32_t)src[1] << 16U) |
           ((uint32_t)src[2] << 8U) | (uint32_t)src[3];
}

static void write_u64_be(uint8_t *dest, uint64_t val) {
    dest[0] = (uint8_t)((val >> 56U) & 0xFFU);
    dest[1] = (uint8_t)((val >> 48U) & 0xFFU);
    dest[2] = (uint8_t)((val >> 40U) & 0xFFU);
    dest[3] = (uint8_t)((val >> 32U) & 0xFFU);
    dest[4] = (uint8_t)((val >> 24U) & 0xFFU);
    dest[5] = (uint8_t)((val >> 16U) & 0xFFU);
    dest[6] = (uint8_t)((val >> 8U) & 0xFFU);
    dest[7] = (uint8_t)(val & 0xFFU);
}

static uint64_t read_u64_be(const uint8_t *src) {
    return ((uint64_t)src[0] << 56U) | ((uint64_t)src[1] << 48U) |
           ((uint64_t)src[2] << 40U) | ((uint64_t)src[3] << 32U) |
           ((uint64_t)src[4] << 24U) | ((uint64_t)src[5] << 16U) |
           ((uint64_t)src[6] << 8U)  | (uint64_t)src[7];
}

static uint32_t canonical_policy_flags(uint32_t flags) {
    if ((flags & ALRIOS_NOTARY_POLICY_REQUIRE_PQC) != 0U) {
        return ALRIOS_NOTARY_POLICY_REQUIRE_PQC | (flags & ALRIOS_NOTARY_POLICY_REQUIRE_CERT_BIND);
    }
    return ALRIOS_NOTARY_POLICY_ALLOW_ED25519 | (flags & ALRIOS_NOTARY_POLICY_REQUIRE_CERT_BIND);
}

static int is_policy_canonical(uint32_t flags) {
    return flags == canonical_policy_flags(flags);
}

static int is_supported_policy_flags(uint32_t flags) {
    const uint32_t known = ALRIOS_NOTARY_POLICY_ALLOW_ED25519 |
                           ALRIOS_NOTARY_POLICY_REQUIRE_PQC |
                           ALRIOS_NOTARY_POLICY_REQUIRE_CERT_BIND;
    if ((flags & ~known) != 0U) {
        return 0;
    }
    if ((flags & ALRIOS_NOTARY_POLICY_REQUIRE_PQC) != 0U &&
        (flags & ALRIOS_NOTARY_POLICY_ALLOW_ED25519) != 0U) {
        return 0;
    }
    return 1;
}

static int is_ed25519_evp_pkey(const EVP_PKEY *pkey) {
    return pkey != NULL && EVP_PKEY_base_id(pkey) == EVP_PKEY_ED25519;
}

static int derive_ed25519_key_id(const uint8_t pubkey[ED25519_PUBLIC_KEY_LEN],
                                  uint8_t out_key_id[ALRIOS_CERTIFICATE_KEY_ID_SIZE]) {

    alrios_sha512_ctx_t ctx;
    uint8_t algo_len[3];
    uint8_t full_digest[SHA512_DIGEST_LEN];

    if (!pubkey || !out_key_id) {
        return ALRIOS_NOTARY_ERR_INVALID_ARGUMENT;
    }

    algo_len[0] = ALRIOS_PUBLIC_KEY_ALGORITHM_ED25519;
    write_u16_be(algo_len + 1U, (uint16_t)ED25519_PUBLIC_KEY_LEN);

    if (alrios_sha512_init(&ctx) != ALRIOS_CRYPTO_OK) {
        return ALRIOS_NOTARY_ERR_KEY_INVALID;
    }
    if (alrios_sha512_update(&ctx, g_key_id_domain, sizeof(g_key_id_domain)) != ALRIOS_CRYPTO_OK ||
        alrios_sha512_update(&ctx, algo_len, sizeof(algo_len)) != ALRIOS_CRYPTO_OK ||
        alrios_sha512_update(&ctx, pubkey, ED25519_PUBLIC_KEY_LEN) != ALRIOS_CRYPTO_OK ||
        alrios_sha512_final(&ctx, full_digest) != ALRIOS_CRYPTO_OK) {
        alrios_explicit_zeroize(&ctx, sizeof(ctx));
        alrios_explicit_zeroize(full_digest, sizeof(full_digest));
        return ALRIOS_NOTARY_ERR_KEY_INVALID;
    }

    memcpy(out_key_id, full_digest, ALRIOS_CERTIFICATE_KEY_ID_SIZE);
    alrios_explicit_zeroize(&ctx, sizeof(ctx));
    alrios_explicit_zeroize(full_digest, sizeof(full_digest));
    return ALRIOS_NOTARY_OK;
}

int alrios_notary_policy_init(alrios_notary_policy_t *policy, uint32_t flags) {
    uint32_t normalized_flags;

    if (!policy) {
        return ALRIOS_NOTARY_ERR_INVALID_ARGUMENT;
    }

    memset(policy, 0, sizeof(*policy));
    if (!is_supported_policy_flags(flags) || !is_policy_canonical(flags)) {
        return ALRIOS_NOTARY_ERR_POLICY_VIOLATION;
    }
    normalized_flags = canonical_policy_flags(flags);
    if (!is_supported_policy_flags(normalized_flags)) {
        return ALRIOS_NOTARY_ERR_POLICY_VIOLATION;
    }

    policy->flags = normalized_flags;

    if ((normalized_flags & ALRIOS_NOTARY_POLICY_REQUIRE_PQC) != 0U) {
        policy->allowed_signature_algorithm = 0U;
        return ALRIOS_NOTARY_ERR_POLICY_VIOLATION;
    }

    policy->allowed_signature_algorithm = ALRIOS_SIGNATURE_ALGORITHM_ED25519;
    return ALRIOS_NOTARY_OK;
}

int alrios_notary_policy_validate(const alrios_notary_policy_t *policy, uint8_t algorithm) {
    if (!policy) {
        return ALRIOS_NOTARY_ERR_INVALID_ARGUMENT;
    }
    if (!is_supported_policy_flags(policy->flags)) {
        return ALRIOS_NOTARY_ERR_POLICY_VIOLATION;
    }
    if (!is_policy_canonical(policy->flags)) {
        return ALRIOS_NOTARY_ERR_POLICY_VIOLATION;
    }

    if ((policy->flags & ALRIOS_NOTARY_POLICY_REQUIRE_PQC) != 0U) {
        return ALRIOS_NOTARY_ERR_POLICY_VIOLATION;
    }

    if (algorithm == ALRIOS_SIGNATURE_ALGORITHM_ED25519 &&
        (policy->flags & ALRIOS_NOTARY_POLICY_ALLOW_ED25519) != 0U &&
        policy->allowed_signature_algorithm == ALRIOS_SIGNATURE_ALGORITHM_ED25519) {
        return ALRIOS_NOTARY_OK;
    }

    return ALRIOS_NOTARY_ERR_POLICY_VIOLATION;
}

static int build_signature_tbs(const alrios_notary_signature_t *sig,
                               uint8_t *out_tbs,
                               size_t out_capacity,
                               size_t *out_written) {
    size_t offset = 0U;

    if (out_written) {
        *out_written = 0U;
    }
    if (!sig || !out_tbs || !out_written) {
        return ALRIOS_NOTARY_ERR_INVALID_ARGUMENT;
    }
    if (out_capacity < ALRIOS_NOTARY_TBS_DOMAIN_SIZE + ALRIOS_NOTARY_HEADER_SIZE) {
        return ALRIOS_NOTARY_ERR_CAPACITY;
    }
    if (sig->version != ALRIOS_NOTARY_VERSION_1 || sig->reserved != 0U ||
        sig->algorithm != ALRIOS_SIGNATURE_ALGORITHM_ED25519 ||
        sig->signature_len != ED25519_SIGNATURE_LEN ||
        !is_supported_policy_flags(sig->policy_flags) ||
        !is_policy_canonical(sig->policy_flags) ||
        (sig->policy_flags & ALRIOS_NOTARY_POLICY_REQUIRE_PQC) != 0U) {
        return ALRIOS_NOTARY_ERR_NONCANONICAL;
    }

    memcpy(out_tbs + offset, g_notary_tbs_domain, sizeof(g_notary_tbs_domain));
    offset += sizeof(g_notary_tbs_domain);
    memcpy(out_tbs + offset, g_notary_magic, sizeof(g_notary_magic));
    offset += sizeof(g_notary_magic);
    write_u16_be(out_tbs + offset, sig->version);
    offset += 2U;
    out_tbs[offset++] = sig->algorithm;
    out_tbs[offset++] = 0U;
    write_u64_be(out_tbs + offset, sig->payload_len);
    offset += 8U;
    write_u64_be(out_tbs + offset, sig->timestamp);
    offset += 8U;
    write_u32_be(out_tbs + offset, sig->policy_flags);
    offset += 4U;
    memcpy(out_tbs + offset, sig->key_id, ALRIOS_CERTIFICATE_KEY_ID_SIZE);
    offset += ALRIOS_CERTIFICATE_KEY_ID_SIZE;
    memcpy(out_tbs + offset, sig->digest, SHA512_DIGEST_LEN);
    offset += SHA512_DIGEST_LEN;
    write_u16_be(out_tbs + offset, (uint16_t)sig->signature_len);
    offset += 2U;
    write_u16_be(out_tbs + offset, 0U);
    offset += 2U;

    if (offset != ALRIOS_NOTARY_TBS_DOMAIN_SIZE + ALRIOS_NOTARY_HEADER_SIZE) {
        alrios_explicit_zeroize(out_tbs, out_capacity);
        return ALRIOS_NOTARY_ERR_SIGN_FAILED;
    }

    *out_written = offset;
    return ALRIOS_NOTARY_OK;
}

int alrios_notary_sign_payload(const alrios_notary_policy_t *policy,
                               const uint8_t *payload,
                               size_t payload_len,
                               EVP_PKEY *privkey,
                               alrios_notary_signature_t *out_sig) {
    uint8_t raw_pub[ED25519_PUBLIC_KEY_LEN];
    uint8_t tbs[ALRIOS_NOTARY_TBS_DOMAIN_SIZE + ALRIOS_NOTARY_HEADER_SIZE];
    uint8_t tbs_digest[SHA512_DIGEST_LEN];
    size_t pub_len = sizeof(raw_pub);
    size_t sig_len = ED25519_SIGNATURE_LEN;
    size_t tbs_len = 0U;
    EVP_MD_CTX *mctx = NULL;
    int rc;

    if (!policy || !privkey || !out_sig) {
        return ALRIOS_NOTARY_ERR_INVALID_ARGUMENT;
    }
    if (payload_len > 0 && !payload) {
        return ALRIOS_NOTARY_ERR_INVALID_ARGUMENT;
    }
    if (payload_len > ALRIOS_NOTARY_MAX_PAYLOAD_SIZE) {
        return ALRIOS_NOTARY_ERR_CAPACITY;
    }

    rc = alrios_notary_policy_validate(policy, ALRIOS_SIGNATURE_ALGORITHM_ED25519);
    if (rc != ALRIOS_NOTARY_OK) {
        return rc;
    }

    memset(out_sig, 0, sizeof(*out_sig));

    if (!is_ed25519_evp_pkey(privkey) ||
        EVP_PKEY_get_raw_public_key(privkey, raw_pub, &pub_len) <= 0 ||
        pub_len != ED25519_PUBLIC_KEY_LEN) {
        return ALRIOS_NOTARY_ERR_KEY_INVALID;
    }

    rc = derive_ed25519_key_id(raw_pub, out_sig->key_id);
    if (rc != ALRIOS_NOTARY_OK) {
        return rc;
    }

    rc = alrios_sha512(payload, payload_len, out_sig->digest);
    if (rc != ALRIOS_CRYPTO_OK) {
        return ALRIOS_NOTARY_ERR_SIGN_FAILED;
    }

    out_sig->version = ALRIOS_NOTARY_VERSION_1;
    out_sig->algorithm = ALRIOS_SIGNATURE_ALGORITHM_ED25519;
    out_sig->reserved = 0U;
    out_sig->payload_len = (uint64_t)payload_len;
    out_sig->timestamp = (uint64_t)time(NULL);
    out_sig->policy_flags = canonical_policy_flags(policy->flags);
    out_sig->signature_len = ED25519_SIGNATURE_LEN;

    rc = build_signature_tbs(out_sig, tbs, sizeof(tbs), &tbs_len);
    if (rc != ALRIOS_NOTARY_OK) {
        alrios_explicit_zeroize(tbs, sizeof(tbs));
        return rc;
    }

    mctx = EVP_MD_CTX_new();
    if (!mctx) {
        alrios_explicit_zeroize(tbs, sizeof(tbs));
        return ALRIOS_NOTARY_ERR_SIGN_FAILED;
    }

    rc = alrios_sha512(tbs, tbs_len, tbs_digest);
    if (rc != ALRIOS_CRYPTO_OK) {
        EVP_MD_CTX_free(mctx);
        alrios_explicit_zeroize(tbs, sizeof(tbs));
        alrios_explicit_zeroize(tbs_digest, sizeof(tbs_digest));
        return ALRIOS_NOTARY_ERR_SIGN_FAILED;
    }

    if (EVP_DigestSignInit(mctx, NULL, NULL, NULL, privkey) <= 0 ||
        EVP_DigestSign(mctx, out_sig->signature, &sig_len, tbs_digest, sizeof(tbs_digest)) <= 0 ||
        sig_len != ED25519_SIGNATURE_LEN) {
        EVP_MD_CTX_free(mctx);
        alrios_explicit_zeroize(tbs, sizeof(tbs));
        alrios_explicit_zeroize(tbs_digest, sizeof(tbs_digest));
        return ALRIOS_NOTARY_ERR_SIGN_FAILED;
    }

    EVP_MD_CTX_free(mctx);
    alrios_explicit_zeroize(tbs, sizeof(tbs));
    alrios_explicit_zeroize(tbs_digest, sizeof(tbs_digest));

    return ALRIOS_NOTARY_OK;
}

static int verify_payload_with_raw_ed25519_key(const alrios_notary_policy_t *policy,
                                               const uint8_t *payload,
                                               size_t payload_len,
                                               const alrios_notary_signature_t *sig,
                                               const uint8_t pubkey[ALRIOS_CERTIFICATE_ED25519_KEY_SIZE],
                                               int certificate_chain_validated) {
    uint8_t expected_key_id[ALRIOS_CERTIFICATE_KEY_ID_SIZE];
    uint8_t expected_digest[SHA512_DIGEST_LEN];
    uint8_t tbs[ALRIOS_NOTARY_TBS_DOMAIN_SIZE + ALRIOS_NOTARY_HEADER_SIZE];
    uint8_t tbs_digest[SHA512_DIGEST_LEN];
    size_t tbs_len = 0U;
    int rc;

    if (!policy || !sig || !pubkey) {
        return ALRIOS_NOTARY_ERR_INVALID_ARGUMENT;
    }
    if (payload_len > 0 && !payload) {
        return ALRIOS_NOTARY_ERR_INVALID_ARGUMENT;
    }

    if (sig->version != ALRIOS_NOTARY_VERSION_1 || sig->reserved != 0U) {
        return ALRIOS_NOTARY_ERR_NONCANONICAL;
    }

    rc = alrios_notary_policy_validate(policy, sig->algorithm);
    if (rc != ALRIOS_NOTARY_OK) {
        return rc;
    }
    if ((policy->flags & ALRIOS_NOTARY_POLICY_REQUIRE_CERT_BIND) != 0U &&
        certificate_chain_validated == 0) {
        return ALRIOS_NOTARY_ERR_POLICY_VIOLATION;
    }
    if (sig->policy_flags != canonical_policy_flags(policy->flags)) {
        return ALRIOS_NOTARY_ERR_POLICY_VIOLATION;
    }

    if (sig->payload_len != (uint64_t)payload_len) {
        return ALRIOS_NOTARY_ERR_PAYLOAD_TAMPERED;
    }

    if (policy->max_validity_period_sec > 0) {
        uint64_t now = (uint64_t)time(NULL);
        if (now < sig->timestamp || (now - sig->timestamp) > policy->max_validity_period_sec) {
            return ALRIOS_NOTARY_ERR_POLICY_VIOLATION;
        }
    }

    rc = derive_ed25519_key_id(pubkey, expected_key_id);
    if (rc != ALRIOS_NOTARY_OK) {
        return rc;
    }

    if (alrios_constant_time_memcmp(sig->key_id, expected_key_id, ALRIOS_CERTIFICATE_KEY_ID_SIZE) != 0) {
        return ALRIOS_NOTARY_ERR_KEY_INVALID;
    }

    rc = alrios_sha512(payload, payload_len, expected_digest);
    if (rc != ALRIOS_CRYPTO_OK) {
        return ALRIOS_NOTARY_ERR_VERIFY_FAILED;
    }

    if (alrios_constant_time_memcmp(sig->digest, expected_digest, SHA512_DIGEST_LEN) != 0) {
        return ALRIOS_NOTARY_ERR_PAYLOAD_TAMPERED;
    }

    if (sig->signature_len != ED25519_SIGNATURE_LEN) {
        return ALRIOS_NOTARY_ERR_VERIFY_FAILED;
    }

    rc = build_signature_tbs(sig, tbs, sizeof(tbs), &tbs_len);
    if (rc != ALRIOS_NOTARY_OK) {
        alrios_explicit_zeroize(tbs, sizeof(tbs));
        return rc;
    }

    rc = alrios_sha512(tbs, tbs_len, tbs_digest);
    alrios_explicit_zeroize(tbs, sizeof(tbs));
    if (rc != ALRIOS_CRYPTO_OK) {
        alrios_explicit_zeroize(tbs_digest, sizeof(tbs_digest));
        return ALRIOS_NOTARY_ERR_VERIFY_FAILED;
    }

    rc = alrios_ed25519_verify(pubkey, tbs_digest, sig->signature);
    alrios_explicit_zeroize(tbs_digest, sizeof(tbs_digest));
    if (rc != ALRIOS_CRYPTO_OK) {
        return ALRIOS_NOTARY_ERR_VERIFY_FAILED;
    }

    return ALRIOS_NOTARY_OK;
}

int alrios_notary_verify_payload(const alrios_notary_policy_t *policy,
                                 const uint8_t *payload,
                                 size_t payload_len,
                                 const alrios_notary_signature_t *sig,
                                 const uint8_t pubkey[ALRIOS_CERTIFICATE_ED25519_KEY_SIZE]) {
    if (!policy) {
        return ALRIOS_NOTARY_ERR_INVALID_ARGUMENT;
    }
    if ((policy->flags & ALRIOS_NOTARY_POLICY_REQUIRE_CERT_BIND) != 0U) {
        return ALRIOS_NOTARY_ERR_POLICY_VIOLATION;
    }
    return verify_payload_with_raw_ed25519_key(policy, payload, payload_len, sig, pubkey, 0);
}

int alrios_notary_verify_with_certificate(const alrios_notary_policy_t *policy,
                                          const uint8_t *payload,
                                          size_t payload_len,
                                          const alrios_notary_signature_t *sig,
                                          const alrios_certificate_t *leaf_cert,
                                          const alrios_certificate_t *inter_cert,
                                          const alrios_trust_store_t *store,
                                          uint64_t current_time) {
    int rc;

    if (!policy || !sig || !leaf_cert || !inter_cert || !store) {
        return ALRIOS_NOTARY_ERR_INVALID_ARGUMENT;
    }
    if (payload_len > 0U && !payload) {
        return ALRIOS_NOTARY_ERR_INVALID_ARGUMENT;
    }
    if ((policy->flags & ALRIOS_NOTARY_POLICY_REQUIRE_CERT_BIND) == 0U) {
        return ALRIOS_NOTARY_ERR_POLICY_VIOLATION;
    }

    rc = alrios_pki_verify_chain(store, leaf_cert, inter_cert, current_time);
    if (rc != ALRIOS_ISSUER_OK) {
        return ALRIOS_NOTARY_ERR_VERIFY_FAILED;
    }

    if (leaf_cert->public_key_algorithm != ALRIOS_PUBLIC_KEY_ALGORITHM_ED25519 ||
        leaf_cert->public_key_len != ED25519_PUBLIC_KEY_LEN) {
        return ALRIOS_NOTARY_ERR_POLICY_VIOLATION;
    }

    return verify_payload_with_raw_ed25519_key(policy, payload, payload_len, sig, leaf_cert->public_key, 1);
}

int alrios_notary_signature_encode(const alrios_notary_signature_t *sig,
                                   uint8_t *out_wire,
                                   size_t out_capacity,
                                   size_t *out_written) {
    size_t total_len;

    if (out_written) {
        *out_written = 0U;
    }
    if (!sig || !out_wire || !out_written) {
        return ALRIOS_NOTARY_ERR_INVALID_ARGUMENT;
    }

    if (sig->version != ALRIOS_NOTARY_VERSION_1 || sig->reserved != 0U ||
        sig->algorithm != ALRIOS_SIGNATURE_ALGORITHM_ED25519 ||
        sig->signature_len != ED25519_SIGNATURE_LEN ||
        !is_supported_policy_flags(sig->policy_flags) ||
        !is_policy_canonical(sig->policy_flags) ||
        (sig->policy_flags & ALRIOS_NOTARY_POLICY_REQUIRE_PQC) != 0U) {
        return ALRIOS_NOTARY_ERR_NONCANONICAL;
    }

    total_len = ALRIOS_NOTARY_HEADER_SIZE + sig->signature_len;
    if (out_capacity < total_len) {
        return ALRIOS_NOTARY_ERR_CAPACITY;
    }

    memset(out_wire, 0, total_len);
    memcpy(out_wire, g_notary_magic, sizeof(g_notary_magic));
    write_u16_be(out_wire + 8U, sig->version);
    out_wire[10U] = sig->algorithm;
    out_wire[11U] = 0U; /* reserved */
    write_u64_be(out_wire + 12U, sig->payload_len);
    write_u64_be(out_wire + 20U, sig->timestamp);
    write_u32_be(out_wire + 28U, sig->policy_flags);
    memcpy(out_wire + 32U, sig->key_id, ALRIOS_CERTIFICATE_KEY_ID_SIZE);
    memcpy(out_wire + 64U, sig->digest, SHA512_DIGEST_LEN);
    write_u16_be(out_wire + 128U, (uint16_t)sig->signature_len);
    write_u16_be(out_wire + 130U, 0U); /* reserved */
    memcpy(out_wire + ALRIOS_NOTARY_HEADER_SIZE, sig->signature, sig->signature_len);

    *out_written = total_len;
    return ALRIOS_NOTARY_OK;
}

int alrios_notary_signature_decode(const uint8_t *wire,
                                   size_t wire_len,
                                   alrios_notary_signature_t *out_sig) {
    uint16_t sig_len;
    size_t total_expected;

    if (out_sig) {
        memset(out_sig, 0, sizeof(*out_sig));
    }
    if (!wire || !out_sig) {
        return ALRIOS_NOTARY_ERR_INVALID_ARGUMENT;
    }

    if (wire_len < ALRIOS_NOTARY_HEADER_SIZE) {
        return ALRIOS_NOTARY_ERR_TRUNCATED;
    }

    if (memcmp(wire, g_notary_magic, sizeof(g_notary_magic)) != 0) {
        return ALRIOS_NOTARY_ERR_BAD_MAGIC;
    }

    out_sig->version = read_u16_be(wire + 8U);
    if (out_sig->version != ALRIOS_NOTARY_VERSION_1) {
        return ALRIOS_NOTARY_ERR_NONCANONICAL;
    }

    out_sig->algorithm = wire[10U];
    out_sig->reserved = wire[11U];
    if (out_sig->reserved != 0U) {
        return ALRIOS_NOTARY_ERR_NONCANONICAL;
    }

    out_sig->payload_len = read_u64_be(wire + 12U);
    out_sig->timestamp = read_u64_be(wire + 20U);
    out_sig->policy_flags = read_u32_be(wire + 28U);
    if (!is_supported_policy_flags(out_sig->policy_flags) ||
        !is_policy_canonical(out_sig->policy_flags) ||
        (out_sig->policy_flags & ALRIOS_NOTARY_POLICY_REQUIRE_PQC) != 0U) {
        return ALRIOS_NOTARY_ERR_POLICY_VIOLATION;
    }
    memcpy(out_sig->key_id, wire + 32U, ALRIOS_CERTIFICATE_KEY_ID_SIZE);
    memcpy(out_sig->digest, wire + 64U, SHA512_DIGEST_LEN);
    sig_len = read_u16_be(wire + 128U);

    if (read_u16_be(wire + 130U) != 0U) {
        return ALRIOS_NOTARY_ERR_NONCANONICAL;
    }

    if (out_sig->algorithm != ALRIOS_SIGNATURE_ALGORITHM_ED25519 || sig_len != ED25519_SIGNATURE_LEN) {
        return ALRIOS_NOTARY_ERR_NONCANONICAL;
    }

    total_expected = ALRIOS_NOTARY_HEADER_SIZE + (size_t)sig_len;
    if (wire_len != total_expected) {
        return (wire_len < total_expected) ? ALRIOS_NOTARY_ERR_TRUNCATED : ALRIOS_NOTARY_ERR_NONCANONICAL;
    }

    out_sig->signature_len = (size_t)sig_len;
    memcpy(out_sig->signature, wire + ALRIOS_NOTARY_HEADER_SIZE, (size_t)sig_len);

    return ALRIOS_NOTARY_OK;
}

int alrios_notary_load_private_key_file(const char *path, EVP_PKEY **out_pkey) {
    FILE *f = NULL;
    long sz;
    uint8_t *buf = NULL;
    size_t read_bytes;
    BIO *bio = NULL;
    EVP_PKEY *pkey = NULL;

    if (!path || !out_pkey) {
        return ALRIOS_NOTARY_ERR_INVALID_ARGUMENT;
    }
    *out_pkey = NULL;

    f = fopen(path, "rb");
    if (!f) {
        return ALRIOS_NOTARY_ERR_IO_FAILED;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        (void)fclose(f);
        return ALRIOS_NOTARY_ERR_IO_FAILED;
    }
    sz = ftell(f);
    if (sz <= 0 || sz > 16384L || sz > (long)INT_MAX) {
        (void)fclose(f);
        return ALRIOS_NOTARY_ERR_KEY_INVALID;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        (void)fclose(f);
        return ALRIOS_NOTARY_ERR_IO_FAILED;
    }

    buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) {
        (void)fclose(f);
        return ALRIOS_NOTARY_ERR_CAPACITY;
    }

    read_bytes = fread(buf, 1U, (size_t)sz, f);
    if (ferror(f) != 0 || read_bytes != (size_t)sz) {
        (void)fclose(f);
        alrios_explicit_zeroize(buf, (size_t)sz);
        free(buf);
        return ALRIOS_NOTARY_ERR_IO_FAILED;
    }
    if (fclose(f) != 0) {
        alrios_explicit_zeroize(buf, (size_t)sz);
        free(buf);
        return ALRIOS_NOTARY_ERR_IO_FAILED;
    }

    if (read_bytes == 32U) {
        pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, buf, 32U);
        alrios_explicit_zeroize(buf, (size_t)sz);
        free(buf);
        if (!pkey) {
            return ALRIOS_NOTARY_ERR_KEY_INVALID;
        }
        *out_pkey = pkey;
        return ALRIOS_NOTARY_OK;
    }

    if (read_bytes == 64U) {
        pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, buf, 32U);
        alrios_explicit_zeroize(buf, (size_t)sz);
        free(buf);
        if (!pkey) {
            return ALRIOS_NOTARY_ERR_KEY_INVALID;
        }
        *out_pkey = pkey;
        return ALRIOS_NOTARY_OK;
    }

    bio = BIO_new_mem_buf(buf, (int)read_bytes);
    if (!bio) {
        alrios_explicit_zeroize(buf, (size_t)sz);
        free(buf);
        return ALRIOS_NOTARY_ERR_KEY_INVALID;
    }

    pkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
    BIO_free(bio);
    alrios_explicit_zeroize(buf, (size_t)sz);
    free(buf);

    if (!pkey) {
        return ALRIOS_NOTARY_ERR_KEY_INVALID;
    }
    if (!is_ed25519_evp_pkey(pkey)) {
        EVP_PKEY_free(pkey);
        return ALRIOS_NOTARY_ERR_KEY_INVALID;
    }

    *out_pkey = pkey;
    return ALRIOS_NOTARY_OK;
}

int alrios_notary_load_public_key_file(const char *path, uint8_t out_pubkey[ALRIOS_CERTIFICATE_ED25519_KEY_SIZE]) {
    FILE *f = NULL;
    long sz;
    uint8_t buf[8192];
    size_t read_bytes;
    BIO *bio = NULL;
    EVP_PKEY *pkey = NULL;
    size_t pub_len = ED25519_PUBLIC_KEY_LEN;
    alrios_certificate_t cert;

    if (!path || !out_pubkey) {
        return ALRIOS_NOTARY_ERR_INVALID_ARGUMENT;
    }

    f = fopen(path, "rb");
    if (!f) {
        return ALRIOS_NOTARY_ERR_IO_FAILED;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return ALRIOS_NOTARY_ERR_IO_FAILED;
    }
    sz = ftell(f);
    if (sz <= 0 || sz > (long)sizeof(buf)) {
        fclose(f);
        return ALRIOS_NOTARY_ERR_KEY_INVALID;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return ALRIOS_NOTARY_ERR_IO_FAILED;
    }

    read_bytes = fread(buf, 1U, (size_t)sz, f);
    if (ferror(f) != 0 || read_bytes != (size_t)sz) {
        (void)fclose(f);
        return ALRIOS_NOTARY_ERR_IO_FAILED;
    }
    if (fclose(f) != 0) {
        return ALRIOS_NOTARY_ERR_IO_FAILED;
    }

    if (read_bytes == ED25519_PUBLIC_KEY_LEN) {
        memcpy(out_pubkey, buf, ED25519_PUBLIC_KEY_LEN);
        return ALRIOS_NOTARY_OK;
    }

    if (read_bytes >= ALRIOS_CERTIFICATE_HEADER_SIZE_V1 &&
        memcmp(buf, "ALRICERT", 8) == 0) {
        if (alrios_certificate_decode(buf, read_bytes, &cert) == ALRIOS_CERTIFICATE_OK) {
            if (cert.public_key_algorithm == ALRIOS_PUBLIC_KEY_ALGORITHM_ED25519 &&
                cert.public_key_len == ED25519_PUBLIC_KEY_LEN) {
                memcpy(out_pubkey, cert.public_key, ED25519_PUBLIC_KEY_LEN);
                return ALRIOS_NOTARY_OK;
            }
        }
    }

    bio = BIO_new_mem_buf(buf, (int)read_bytes);
    if (!bio) {
        return ALRIOS_NOTARY_ERR_KEY_INVALID;
    }

    pkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
    BIO_free(bio);

    if (!pkey) {
        return ALRIOS_NOTARY_ERR_KEY_INVALID;
    }

    if (!is_ed25519_evp_pkey(pkey) ||
        EVP_PKEY_get_raw_public_key(pkey, out_pubkey, &pub_len) <= 0 ||
        pub_len != ED25519_PUBLIC_KEY_LEN) {
        EVP_PKEY_free(pkey);
        return ALRIOS_NOTARY_ERR_KEY_INVALID;
    }

    EVP_PKEY_free(pkey);
    return ALRIOS_NOTARY_OK;
}

int alrios_notary_save_private_key_raw(const char *path, EVP_PKEY *pkey) {
    uint8_t raw_priv[64];
    size_t priv_len = sizeof(raw_priv);
    FILE *f = NULL;
    size_t written;
#ifdef _WIN32
    int fd = -1;
#else
    int fd = -1;
#endif

    if (!path || !pkey) {
        return ALRIOS_NOTARY_ERR_INVALID_ARGUMENT;
    }

    if (!is_ed25519_evp_pkey(pkey) ||
        EVP_PKEY_get_raw_private_key(pkey, raw_priv, &priv_len) <= 0 || priv_len != 32U) {
        return ALRIOS_NOTARY_ERR_KEY_INVALID;
    }

#ifdef _WIN32
    if (_sopen_s(&fd, path, _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY,
                 _SH_DENYRW, _S_IREAD | _S_IWRITE) != 0 || fd < 0) {
        alrios_explicit_zeroize(raw_priv, sizeof(raw_priv));
        return ALRIOS_NOTARY_ERR_IO_FAILED;
    }
    f = _fdopen(fd, "wb");
#else
    fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, (mode_t)0600);
    if (fd < 0) {
        alrios_explicit_zeroize(raw_priv, sizeof(raw_priv));
        return ALRIOS_NOTARY_ERR_IO_FAILED;
    }
    f = fdopen(fd, "wb");
#endif
    if (!f) {
#ifdef _WIN32
        (void)_close(fd);
#else
        (void)close(fd);
#endif
        alrios_explicit_zeroize(raw_priv, sizeof(raw_priv));
        return ALRIOS_NOTARY_ERR_IO_FAILED;
    }

    written = fwrite(raw_priv, 1U, priv_len, f);
    if (fflush(f) != 0) {
        (void)fclose(f);
        alrios_explicit_zeroize(raw_priv, sizeof(raw_priv));
        return ALRIOS_NOTARY_ERR_IO_FAILED;
    }
    if (fclose(f) != 0) {
        alrios_explicit_zeroize(raw_priv, sizeof(raw_priv));
        return ALRIOS_NOTARY_ERR_IO_FAILED;
    }
    alrios_explicit_zeroize(raw_priv, sizeof(raw_priv));

    if (written != priv_len) {
        return ALRIOS_NOTARY_ERR_IO_FAILED;
    }

    return ALRIOS_NOTARY_OK;
}

int alrios_notary_save_public_key_raw(const char *path, const uint8_t pubkey[ALRIOS_CERTIFICATE_ED25519_KEY_SIZE]) {
    FILE *f = NULL;
    size_t written;

    if (!path || !pubkey) {
        return ALRIOS_NOTARY_ERR_INVALID_ARGUMENT;
    }

    f = fopen(path, "wb");
    if (!f) {
        return ALRIOS_NOTARY_ERR_IO_FAILED;
    }

    written = fwrite(pubkey, 1, ED25519_PUBLIC_KEY_LEN, f);
    if (fflush(f) != 0) {
        (void)fclose(f);
        return ALRIOS_NOTARY_ERR_IO_FAILED;
    }
    if (fclose(f) != 0) {
        return ALRIOS_NOTARY_ERR_IO_FAILED;
    }

    if (written != ED25519_PUBLIC_KEY_LEN) {
        return ALRIOS_NOTARY_ERR_IO_FAILED;
    }

    return ALRIOS_NOTARY_OK;
}
