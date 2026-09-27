/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/crypto_verify.h"

#include <string.h>

#if defined(ALRIOS_HAVE_ML_DSA_65)
#include <openssl/evp.h>
#include <openssl/core_names.h>
#include <openssl/params.h>
#endif

int alrios_ml_dsa_65_provider_available(void) {
#if defined(ALRIOS_HAVE_ML_DSA_65)
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, "ML-DSA-65", NULL);
    if (!ctx) {
        return 0;
    }
    EVP_PKEY_CTX_free(ctx);
    return 1;
#else
    return 0;
#endif
}

int alrios_ml_dsa_65_keypair(
    uint8_t out_public_key[ML_DSA_65_PUBLIC_KEY_LEN],
    uint8_t out_secret_key[ML_DSA_65_SECRET_KEY_LEN]
) {
    if (!out_public_key || !out_secret_key) {
        return ALRIOS_CRYPTO_ERR_NULL_PTR;
    }

#if defined(ALRIOS_HAVE_ML_DSA_65)
    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_from_name(NULL, "ML-DSA-65", NULL);
    if (!pctx) {
        return ALRIOS_CRYPTO_ERR_UNAVAILABLE;
    }
    if (EVP_PKEY_keygen_init(pctx) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return ALRIOS_CRYPTO_ERR_INTERNAL;
    }

    EVP_PKEY *pkey = NULL;
    if (EVP_PKEY_generate(pctx, &pkey) <= 0 || !pkey) {
        EVP_PKEY_CTX_free(pctx);
        return ALRIOS_CRYPTO_ERR_INTERNAL;
    }

    size_t pub_len = ML_DSA_65_PUBLIC_KEY_LEN;
    size_t priv_len = ML_DSA_65_SECRET_KEY_LEN;

    if (EVP_PKEY_get_octet_string_param(pkey, OSSL_PKEY_PARAM_PUB_KEY,
                                        out_public_key, ML_DSA_65_PUBLIC_KEY_LEN,
                                        &pub_len) <= 0 ||
        pub_len != ML_DSA_65_PUBLIC_KEY_LEN) {
        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(pctx);
        alrios_explicit_zeroize(out_public_key, ML_DSA_65_PUBLIC_KEY_LEN);
        alrios_explicit_zeroize(out_secret_key, ML_DSA_65_SECRET_KEY_LEN);
        return ALRIOS_CRYPTO_ERR_INTERNAL;
    }

    if (EVP_PKEY_get_octet_string_param(pkey, OSSL_PKEY_PARAM_PRIV_KEY,
                                        out_secret_key, ML_DSA_65_SECRET_KEY_LEN,
                                        &priv_len) <= 0 ||
        priv_len != ML_DSA_65_SECRET_KEY_LEN) {
        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(pctx);
        alrios_explicit_zeroize(out_public_key, ML_DSA_65_PUBLIC_KEY_LEN);
        alrios_explicit_zeroize(out_secret_key, ML_DSA_65_SECRET_KEY_LEN);
        return ALRIOS_CRYPTO_ERR_INTERNAL;
    }

    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(pctx);
    return ALRIOS_CRYPTO_OK;
#else
    memset(out_public_key, 0, ML_DSA_65_PUBLIC_KEY_LEN);
    memset(out_secret_key, 0, ML_DSA_65_SECRET_KEY_LEN);
    return ALRIOS_CRYPTO_ERR_UNAVAILABLE;
#endif
}

int alrios_ml_dsa_65_keypair_from_seed(
    const uint8_t seed[ML_DSA_65_SEED_LEN],
    uint8_t out_public_key[ML_DSA_65_PUBLIC_KEY_LEN],
    uint8_t out_secret_key[ML_DSA_65_SECRET_KEY_LEN]
) {
    if (!seed || !out_public_key || !out_secret_key) {
        return ALRIOS_CRYPTO_ERR_NULL_PTR;
    }

#if defined(ALRIOS_HAVE_ML_DSA_65)
    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_from_name(NULL, "ML-DSA-65", NULL);
    if (!pctx) {
        return ALRIOS_CRYPTO_ERR_UNAVAILABLE;
    }
    if (EVP_PKEY_keygen_init(pctx) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return ALRIOS_CRYPTO_ERR_INTERNAL;
    }

    OSSL_PARAM kparams[] = {
        OSSL_PARAM_construct_octet_string("seed", (void *)(uintptr_t)seed, ML_DSA_65_SEED_LEN),
        OSSL_PARAM_construct_end()
    };
    if (EVP_PKEY_CTX_set_params(pctx, kparams) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return ALRIOS_CRYPTO_ERR_INTERNAL;
    }

    EVP_PKEY *pkey = NULL;
    if (EVP_PKEY_generate(pctx, &pkey) <= 0 || !pkey) {
        EVP_PKEY_CTX_free(pctx);
        return ALRIOS_CRYPTO_ERR_INTERNAL;
    }

    size_t pub_len = ML_DSA_65_PUBLIC_KEY_LEN;
    size_t priv_len = ML_DSA_65_SECRET_KEY_LEN;

    if (EVP_PKEY_get_octet_string_param(pkey, OSSL_PKEY_PARAM_PUB_KEY,
                                        out_public_key, ML_DSA_65_PUBLIC_KEY_LEN,
                                        &pub_len) <= 0 ||
        pub_len != ML_DSA_65_PUBLIC_KEY_LEN) {
        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(pctx);
        alrios_explicit_zeroize(out_public_key, ML_DSA_65_PUBLIC_KEY_LEN);
        alrios_explicit_zeroize(out_secret_key, ML_DSA_65_SECRET_KEY_LEN);
        return ALRIOS_CRYPTO_ERR_INTERNAL;
    }

    if (EVP_PKEY_get_octet_string_param(pkey, OSSL_PKEY_PARAM_PRIV_KEY,
                                        out_secret_key, ML_DSA_65_SECRET_KEY_LEN,
                                        &priv_len) <= 0 ||
        priv_len != ML_DSA_65_SECRET_KEY_LEN) {
        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(pctx);
        alrios_explicit_zeroize(out_public_key, ML_DSA_65_PUBLIC_KEY_LEN);
        alrios_explicit_zeroize(out_secret_key, ML_DSA_65_SECRET_KEY_LEN);
        return ALRIOS_CRYPTO_ERR_INTERNAL;
    }

    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(pctx);
    return ALRIOS_CRYPTO_OK;
#else
    (void)seed;
    memset(out_public_key, 0, ML_DSA_65_PUBLIC_KEY_LEN);
    memset(out_secret_key, 0, ML_DSA_65_SECRET_KEY_LEN);
    return ALRIOS_CRYPTO_ERR_UNAVAILABLE;
#endif
}

int alrios_ml_dsa_65_sign_ctx(
    const uint8_t secret_key[ML_DSA_65_SECRET_KEY_LEN],
    const uint8_t *msg,
    size_t msg_len,
    const uint8_t *context,
    size_t context_len,
    int deterministic,
    uint8_t out_signature[ML_DSA_65_SIGNATURE_LEN]
) {
    if (!secret_key || !out_signature) {
        return ALRIOS_CRYPTO_ERR_NULL_PTR;
    }
    if (!msg && msg_len > 0U) {
        return ALRIOS_CRYPTO_ERR_NULL_PTR;
    }
    if (context_len > ML_DSA_65_CONTEXT_MAX_LEN) {
        return ALRIOS_CRYPTO_ERR_INVALID_KEY;
    }

#if defined(ALRIOS_HAVE_ML_DSA_65)
    EVP_PKEY_CTX *fromdata = EVP_PKEY_CTX_new_from_name(NULL, "ML-DSA-65", NULL);
    if (!fromdata) {
        return ALRIOS_CRYPTO_ERR_UNAVAILABLE;
    }
    if (EVP_PKEY_fromdata_init(fromdata) != 1) {
        EVP_PKEY_CTX_free(fromdata);
        return ALRIOS_CRYPTO_ERR_INTERNAL;
    }

    OSSL_PARAM key_params[] = {
        OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PRIV_KEY,
                                          (void *)(uintptr_t)secret_key,
                                          ML_DSA_65_SECRET_KEY_LEN),
        OSSL_PARAM_construct_end()
    };
    EVP_PKEY *key = NULL;
    if (EVP_PKEY_fromdata(fromdata, &key, EVP_PKEY_KEYPAIR, key_params) != 1 || !key) {
        EVP_PKEY_CTX_free(fromdata);
        return ALRIOS_CRYPTO_ERR_INVALID_KEY;
    }
    EVP_PKEY_CTX_free(fromdata);

    EVP_MD_CTX *md_ctx = EVP_MD_CTX_new();
    if (!md_ctx) {
        EVP_PKEY_free(key);
        return ALRIOS_CRYPTO_ERR_ALLOC_FAIL;
    }

    OSSL_PARAM sign_params[3];
    int param_idx = 0;
    if (context && context_len > 0U) {
        sign_params[param_idx++] = OSSL_PARAM_construct_octet_string(
            "context-string", (void *)(uintptr_t)context, context_len
        );
    }
    int det_flag = deterministic ? 1 : 0;
    sign_params[param_idx++] = OSSL_PARAM_construct_int("deterministic", &det_flag);
    sign_params[param_idx] = OSSL_PARAM_construct_end();

    EVP_PKEY_CTX *pctx = NULL;
    if (EVP_DigestSignInit_ex(md_ctx, &pctx, NULL, NULL, NULL, key, sign_params) != 1) {
        EVP_MD_CTX_free(md_ctx);
        EVP_PKEY_free(key);
        return ALRIOS_CRYPTO_ERR_INTERNAL;
    }

    size_t sig_len = ML_DSA_65_SIGNATURE_LEN;
    int sign_res = EVP_DigestSign(md_ctx, out_signature, &sig_len, msg, msg_len);
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(key);

    if (sign_res != 1 || sig_len != ML_DSA_65_SIGNATURE_LEN) {
        alrios_explicit_zeroize(out_signature, ML_DSA_65_SIGNATURE_LEN);
        return ALRIOS_CRYPTO_ERR_INTERNAL;
    }

    return ALRIOS_CRYPTO_OK;
#else
    (void)msg;
    (void)msg_len;
    (void)context;
    (void)context_len;
    (void)deterministic;
    memset(out_signature, 0, ML_DSA_65_SIGNATURE_LEN);
    return ALRIOS_CRYPTO_ERR_UNAVAILABLE;
#endif
}

int alrios_ml_dsa_65_sign_msg(
    const uint8_t secret_key[ML_DSA_65_SECRET_KEY_LEN],
    const uint8_t *msg,
    size_t msg_len,
    uint8_t out_signature[ML_DSA_65_SIGNATURE_LEN]
) {
    return alrios_ml_dsa_65_sign_ctx(secret_key, msg, msg_len, NULL, 0U, 1, out_signature);
}

int alrios_ml_dsa_65_sign(
    const uint8_t secret_key[ML_DSA_65_SECRET_KEY_LEN],
    const uint8_t digest[SHA512_DIGEST_LEN],
    uint8_t out_signature[ML_DSA_65_SIGNATURE_LEN]
) {
    if (!digest) {
        return ALRIOS_CRYPTO_ERR_NULL_PTR;
    }
    return alrios_ml_dsa_65_sign_msg(secret_key, digest, SHA512_DIGEST_LEN, out_signature);
}

int alrios_ml_dsa_65_verify_ctx(
    const uint8_t public_key[ML_DSA_65_PUBLIC_KEY_LEN],
    const uint8_t *msg,
    size_t msg_len,
    const uint8_t *context,
    size_t context_len,
    const uint8_t signature[ML_DSA_65_SIGNATURE_LEN]
) {
    if (!public_key || !signature) {
        return ALRIOS_CRYPTO_ERR_NULL_PTR;
    }
    if (!msg && msg_len > 0U) {
        return ALRIOS_CRYPTO_ERR_NULL_PTR;
    }
    if (context_len > ML_DSA_65_CONTEXT_MAX_LEN) {
        return ALRIOS_CRYPTO_ERR_INVALID_KEY;
    }

#if defined(ALRIOS_HAVE_ML_DSA_65)
    EVP_PKEY_CTX *fromdata = EVP_PKEY_CTX_new_from_name(NULL, "ML-DSA-65", NULL);
    if (!fromdata) {
        return ALRIOS_CRYPTO_ERR_UNAVAILABLE;
    }
    if (EVP_PKEY_fromdata_init(fromdata) != 1) {
        EVP_PKEY_CTX_free(fromdata);
        return ALRIOS_CRYPTO_ERR_INTERNAL;
    }

    OSSL_PARAM key_params[] = {
        OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY,
                                          (void *)(uintptr_t)public_key,
                                          ML_DSA_65_PUBLIC_KEY_LEN),
        OSSL_PARAM_construct_end()
    };
    EVP_PKEY *key = NULL;
    if (EVP_PKEY_fromdata(fromdata, &key, EVP_PKEY_PUBLIC_KEY, key_params) != 1 || !key) {
        EVP_PKEY_CTX_free(fromdata);
        return ALRIOS_CRYPTO_ERR_INVALID_KEY;
    }
    EVP_PKEY_CTX_free(fromdata);

    EVP_MD_CTX *md_ctx = EVP_MD_CTX_new();
    if (!md_ctx) {
        EVP_PKEY_free(key);
        return ALRIOS_CRYPTO_ERR_ALLOC_FAIL;
    }

    OSSL_PARAM verify_params[2];
    int param_idx = 0;
    if (context && context_len > 0U) {
        verify_params[param_idx++] = OSSL_PARAM_construct_octet_string(
            "context-string", (void *)(uintptr_t)context, context_len
        );
    }
    verify_params[param_idx] = OSSL_PARAM_construct_end();

    EVP_PKEY_CTX *pctx = NULL;
    int init_res = (param_idx > 0)
        ? EVP_DigestVerifyInit_ex(md_ctx, &pctx, NULL, NULL, NULL, key, verify_params)
        : EVP_DigestVerifyInit(md_ctx, NULL, NULL, NULL, key);

    int result = ALRIOS_CRYPTO_ERR_INTERNAL;
    if (init_res == 1) {
        int verify = EVP_DigestVerify(md_ctx, signature, ML_DSA_65_SIGNATURE_LEN, msg, msg_len);
        result = (verify == 1) ? ALRIOS_CRYPTO_OK : ALRIOS_CRYPTO_ERR_BAD_SIG_PQC;
    }

    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(key);
    return result;
#else
    (void)msg;
    (void)msg_len;
    (void)context;
    (void)context_len;
    return ALRIOS_CRYPTO_ERR_UNAVAILABLE;
#endif
}

int alrios_ml_dsa_65_verify_msg(
    const uint8_t public_key[ML_DSA_65_PUBLIC_KEY_LEN],
    const uint8_t *msg,
    size_t msg_len,
    const uint8_t signature[ML_DSA_65_SIGNATURE_LEN]
) {
    return alrios_ml_dsa_65_verify_ctx(public_key, msg, msg_len, NULL, 0U, signature);
}

int alrios_ml_dsa_65_verify(
    const uint8_t public_key[ML_DSA_65_PUBLIC_KEY_LEN],
    const uint8_t digest[SHA512_DIGEST_LEN],
    const uint8_t signature[ML_DSA_65_SIGNATURE_LEN]
) {
    if (!digest) {
        return ALRIOS_CRYPTO_ERR_NULL_PTR;
    }
    return alrios_ml_dsa_65_verify_msg(public_key, digest, SHA512_DIGEST_LEN, signature);
}
