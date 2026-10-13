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

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <stdlib.h>
#include <string.h>

#define ALRIOS_ARAPP_V2_SIG_OK               0
#define ALRIOS_ARAPP_V2_SIG_ERR_ARGUMENT    -3101
#define ALRIOS_ARAPP_V2_SIG_ERR_KEY         -3102
#define ALRIOS_ARAPP_V2_SIG_ERR_SIGN        -3103
#define ALRIOS_ARAPP_V2_SIG_ERR_VERIFY      -3104
#define ALRIOS_ARAPP_V2_SIG_ERR_POLICY      -3105
#define ALRIOS_ARAPP_V2_SIG_ERR_TAMPERED    -3106
#define ALRIOS_ARAPP_V2_SIG_ERR_UNAVAILABLE -3107

static int compute_tbs_digest(const uint8_t *package, size_t package_len,
                              uint8_t out_digest[64]) {
    EVP_MD_CTX *ctx;
    unsigned int dlen = 0U;

    if (!package || package_len < ALRIOS_ARAPP_V2_HEADER_SIZE || !out_digest) {
        return ALRIOS_ARAPP_V2_SIG_ERR_ARGUMENT;
    }
    ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return ALRIOS_ARAPP_V2_SIG_ERR_SIGN;
    }
    if (EVP_DigestInit_ex(ctx, EVP_sha512(), NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        return ALRIOS_ARAPP_V2_SIG_ERR_SIGN;
    }
    /* Hash prefix (first ALRIOS_ARAPP_V2_PREFIX_SIZE bytes) */
    if (EVP_DigestUpdate(ctx, package, ALRIOS_ARAPP_V2_PREFIX_SIZE) != 1) {
        EVP_MD_CTX_free(ctx);
        return ALRIOS_ARAPP_V2_SIG_ERR_SIGN;
    }
    /* Hash manifest + ciphertext (everything after header) */
    if (package_len > ALRIOS_ARAPP_V2_HEADER_SIZE) {
        if (EVP_DigestUpdate(ctx, package + ALRIOS_ARAPP_V2_HEADER_SIZE,
                             package_len - ALRIOS_ARAPP_V2_HEADER_SIZE) != 1) {
            EVP_MD_CTX_free(ctx);
            return ALRIOS_ARAPP_V2_SIG_ERR_SIGN;
        }
    }
    if (EVP_DigestFinal_ex(ctx, out_digest, &dlen) != 1 || dlen != 64U) {
        EVP_MD_CTX_free(ctx);
        return ALRIOS_ARAPP_V2_SIG_ERR_SIGN;
    }
    EVP_MD_CTX_free(ctx);
    return ALRIOS_ARAPP_V2_SIG_OK;
}

int alrios_arapp_v2_sign_ed25519(
    uint8_t *package, size_t package_len,
    EVP_PKEY *ed25519_key
) {
    uint8_t tbs_digest[64];
    EVP_MD_CTX *mctx;
    uint8_t *sig_slot;
    size_t sig_len = ALRIOS_ARAPP_V2_ED25519_SIGNATURE_SIZE;
    int status;

    if (!package || !ed25519_key ||
        package_len < ALRIOS_ARAPP_V2_HEADER_SIZE) {
        return ALRIOS_ARAPP_V2_SIG_ERR_ARGUMENT;
    }
    status = compute_tbs_digest(package, package_len, tbs_digest);
    if (status != ALRIOS_ARAPP_V2_SIG_OK) {
        return status;
    }
    sig_slot = package + ALRIOS_ARAPP_V2_PREFIX_SIZE;
    mctx = EVP_MD_CTX_new();
    if (!mctx) {
        return ALRIOS_ARAPP_V2_SIG_ERR_SIGN;
    }
    if (EVP_DigestSignInit(mctx, NULL, NULL, NULL, ed25519_key) <= 0 ||
        EVP_DigestSign(mctx, sig_slot, &sig_len, tbs_digest,
                       sizeof(tbs_digest)) <= 0 ||
        sig_len != ALRIOS_ARAPP_V2_ED25519_SIGNATURE_SIZE) {
        EVP_MD_CTX_free(mctx);
        return ALRIOS_ARAPP_V2_SIG_ERR_SIGN;
    }
    EVP_MD_CTX_free(mctx);
    return ALRIOS_ARAPP_V2_SIG_OK;
}

int alrios_arapp_v2_verify_ed25519(
    const uint8_t *package, size_t package_len,
    EVP_PKEY *ed25519_pubkey
) {
    uint8_t tbs_digest[64];
    EVP_MD_CTX *mctx;
    const uint8_t *sig;
    int status;

    if (!package || !ed25519_pubkey ||
        package_len < ALRIOS_ARAPP_V2_HEADER_SIZE) {
        return ALRIOS_ARAPP_V2_SIG_ERR_ARGUMENT;
    }
    status = compute_tbs_digest(package, package_len, tbs_digest);
    if (status != ALRIOS_ARAPP_V2_SIG_OK) {
        return status;
    }
    sig = package + ALRIOS_ARAPP_V2_PREFIX_SIZE;
    mctx = EVP_MD_CTX_new();
    if (!mctx) {
        return ALRIOS_ARAPP_V2_SIG_ERR_VERIFY;
    }
    if (EVP_DigestVerifyInit(mctx, NULL, NULL, NULL, ed25519_pubkey) <= 0) {
        EVP_MD_CTX_free(mctx);
        return ALRIOS_ARAPP_V2_SIG_ERR_VERIFY;
    }
    if (EVP_DigestVerify(mctx, sig, ALRIOS_ARAPP_V2_ED25519_SIGNATURE_SIZE,
                         tbs_digest, sizeof(tbs_digest)) != 1) {
        EVP_MD_CTX_free(mctx);
        return ALRIOS_ARAPP_V2_SIG_ERR_TAMPERED;
    }
    EVP_MD_CTX_free(mctx);
    return ALRIOS_ARAPP_V2_SIG_OK;
}

#if defined(ALRIOS_HAVE_ML_DSA_65)
#include <openssl/core_names.h>
#include <openssl/params.h>

int alrios_arapp_v2_sign_ml_dsa_65(
    uint8_t *package, size_t package_len,
    EVP_PKEY *ml_dsa_key
) {
    uint8_t tbs_digest[64];
    EVP_MD_CTX *mctx;
    EVP_PKEY_CTX *pctx = NULL;
    uint8_t *sig_slot;
    size_t sig_len = ALRIOS_ARAPP_V2_ML_DSA_65_SIGNATURE_SIZE;
    int status;

    if (!package || !ml_dsa_key ||
        package_len < ALRIOS_ARAPP_V2_HEADER_SIZE) {
        return ALRIOS_ARAPP_V2_SIG_ERR_ARGUMENT;
    }
    status = compute_tbs_digest(package, package_len, tbs_digest);
    if (status != ALRIOS_ARAPP_V2_SIG_OK) {
        return status;
    }
    sig_slot = package + ALRIOS_ARAPP_V2_PREFIX_SIZE +
               ALRIOS_ARAPP_V2_ED25519_SIGNATURE_SIZE;

    OSSL_PARAM sign_params[3];
    int det_flag = 1;

    sign_params[0] = OSSL_PARAM_construct_octet_string(
        "context-string", (void *)"ALRIOS-ARAPP-V2", 15U);
    sign_params[1] = OSSL_PARAM_construct_int("deterministic", &det_flag);
    sign_params[2] = OSSL_PARAM_construct_end();

    mctx = EVP_MD_CTX_new();
    if (!mctx) {
        return ALRIOS_ARAPP_V2_SIG_ERR_SIGN;
    }
    if (EVP_DigestSignInit_ex(mctx, &pctx, NULL, NULL, NULL,
                              ml_dsa_key, sign_params) != 1) {
        EVP_MD_CTX_free(mctx);
        return ALRIOS_ARAPP_V2_SIG_ERR_SIGN;
    }
    if (EVP_DigestSign(mctx, sig_slot, &sig_len, tbs_digest,
                       sizeof(tbs_digest)) != 1 ||
        sig_len != ALRIOS_ARAPP_V2_ML_DSA_65_SIGNATURE_SIZE) {
        EVP_MD_CTX_free(mctx);
        return ALRIOS_ARAPP_V2_SIG_ERR_SIGN;
    }
    EVP_MD_CTX_free(mctx);
    return ALRIOS_ARAPP_V2_SIG_OK;
}

int alrios_arapp_v2_verify_ml_dsa_65(
    const uint8_t *package, size_t package_len,
    EVP_PKEY *ml_dsa_pubkey
) {
    uint8_t tbs_digest[64];
    EVP_MD_CTX *mctx;
    EVP_PKEY_CTX *pctx = NULL;
    const uint8_t *sig;
    int status;
    OSSL_PARAM verify_params[2];

    if (!package || !ml_dsa_pubkey ||
        package_len < ALRIOS_ARAPP_V2_HEADER_SIZE) {
        return ALRIOS_ARAPP_V2_SIG_ERR_ARGUMENT;
    }
    status = compute_tbs_digest(package, package_len, tbs_digest);
    if (status != ALRIOS_ARAPP_V2_SIG_OK) {
        return status;
    }
    sig = package + ALRIOS_ARAPP_V2_PREFIX_SIZE +
          ALRIOS_ARAPP_V2_ED25519_SIGNATURE_SIZE;

    verify_params[0] = OSSL_PARAM_construct_octet_string(
        "context-string", (void *)"ALRIOS-ARAPP-V2", 15U);
    verify_params[1] = OSSL_PARAM_construct_end();

    mctx = EVP_MD_CTX_new();
    if (!mctx) {
        return ALRIOS_ARAPP_V2_SIG_ERR_VERIFY;
    }
    if (EVP_DigestVerifyInit_ex(mctx, &pctx, NULL, NULL, NULL,
                                ml_dsa_pubkey, verify_params) != 1) {
        EVP_MD_CTX_free(mctx);
        return ALRIOS_ARAPP_V2_SIG_ERR_VERIFY;
    }
    if (EVP_DigestVerify(mctx, sig, ALRIOS_ARAPP_V2_ML_DSA_65_SIGNATURE_SIZE,
                         tbs_digest, sizeof(tbs_digest)) != 1) {
        EVP_MD_CTX_free(mctx);
        return ALRIOS_ARAPP_V2_SIG_ERR_TAMPERED;
    }
    EVP_MD_CTX_free(mctx);
    return ALRIOS_ARAPP_V2_SIG_OK;
}
#else
int alrios_arapp_v2_sign_ml_dsa_65(
    uint8_t *package, size_t package_len,
    EVP_PKEY *ml_dsa_key
) {
    (void)package; (void)package_len; (void)ml_dsa_key;
    return ALRIOS_ARAPP_V2_SIG_ERR_UNAVAILABLE;
}
int alrios_arapp_v2_verify_ml_dsa_65(
    const uint8_t *package, size_t package_len,
    EVP_PKEY *ml_dsa_pubkey
) {
    (void)package; (void)package_len; (void)ml_dsa_pubkey;
    return ALRIOS_ARAPP_V2_SIG_ERR_UNAVAILABLE;
}
#endif

int alrios_arapp_v2_verify_hybrid(
    const uint8_t *package, size_t package_len,
    EVP_PKEY *ed25519_pubkey,
    EVP_PKEY *ml_dsa_pubkey,
    int require_pqc
) {
    int ed_status;
    int ml_status;

    if (!package || !ed25519_pubkey ||
        package_len < ALRIOS_ARAPP_V2_HEADER_SIZE) {
        return ALRIOS_ARAPP_V2_SIG_ERR_ARGUMENT;
    }
    ed_status = alrios_arapp_v2_verify_ed25519(package, package_len,
                                                ed25519_pubkey);
    if (ed_status != ALRIOS_ARAPP_V2_SIG_OK) {
        return ed_status;
    }
    if (!ml_dsa_pubkey) {
        if (require_pqc) {
            return ALRIOS_ARAPP_V2_SIG_ERR_POLICY;
        }
        return ALRIOS_ARAPP_V2_SIG_OK;
    }
    ml_status = alrios_arapp_v2_verify_ml_dsa_65(package, package_len,
                                                  ml_dsa_pubkey);
    if (ml_status == ALRIOS_ARAPP_V2_SIG_ERR_UNAVAILABLE && !require_pqc) {
        return ALRIOS_ARAPP_V2_SIG_OK;
    }
    return ml_status;
}
