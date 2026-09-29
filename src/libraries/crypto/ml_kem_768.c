/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/crypto/kem.h"

#include <string.h>

#if defined(ALRIOS_HAVE_ML_KEM_768)
#include <openssl/evp.h>
#include <openssl/core_names.h>
#include <openssl/params.h>
#endif

int alrios_ml_kem_768_provider_available(void) {
#if defined(ALRIOS_HAVE_ML_KEM_768)
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, "ML-KEM-768", NULL);
    if (!ctx) {
        return 0;
    }
    EVP_PKEY_CTX_free(ctx);
    return 1;
#else
    return 0;
#endif
}

int alrios_ml_kem_768_keypair(
    uint8_t out_public_key[ML_KEM_768_PUBLIC_KEY_LEN],
    uint8_t out_secret_key[ML_KEM_768_SECRET_KEY_LEN]
) {
    if (!out_public_key || !out_secret_key) {
        return ALRIOS_CRYPTO_ERR_NULL_PTR;
    }

#if defined(ALRIOS_HAVE_ML_KEM_768)
    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_from_name(NULL, "ML-KEM-768", NULL);
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

    size_t pub_len = ML_KEM_768_PUBLIC_KEY_LEN;
    size_t priv_len = ML_KEM_768_SECRET_KEY_LEN;

    if (EVP_PKEY_get_octet_string_param(pkey, OSSL_PKEY_PARAM_PUB_KEY,
                                        out_public_key, ML_KEM_768_PUBLIC_KEY_LEN,
                                        &pub_len) <= 0 ||
        pub_len != ML_KEM_768_PUBLIC_KEY_LEN) {
        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(pctx);
        alrios_explicit_zeroize(out_public_key, ML_KEM_768_PUBLIC_KEY_LEN);
        alrios_explicit_zeroize(out_secret_key, ML_KEM_768_SECRET_KEY_LEN);
        return ALRIOS_CRYPTO_ERR_INTERNAL;
    }

    if (EVP_PKEY_get_octet_string_param(pkey, OSSL_PKEY_PARAM_PRIV_KEY,
                                        out_secret_key, ML_KEM_768_SECRET_KEY_LEN,
                                        &priv_len) <= 0 ||
        priv_len != ML_KEM_768_SECRET_KEY_LEN) {
        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(pctx);
        alrios_explicit_zeroize(out_public_key, ML_KEM_768_PUBLIC_KEY_LEN);
        alrios_explicit_zeroize(out_secret_key, ML_KEM_768_SECRET_KEY_LEN);
        return ALRIOS_CRYPTO_ERR_INTERNAL;
    }

    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(pctx);
    return ALRIOS_CRYPTO_OK;
#else
    memset(out_public_key, 0, ML_KEM_768_PUBLIC_KEY_LEN);
    memset(out_secret_key, 0, ML_KEM_768_SECRET_KEY_LEN);
    return ALRIOS_CRYPTO_ERR_UNAVAILABLE;
#endif
}

int alrios_ml_kem_768_keypair_from_seed(
    const uint8_t seed[ML_KEM_768_SEED_LEN],
    uint8_t out_public_key[ML_KEM_768_PUBLIC_KEY_LEN],
    uint8_t out_secret_key[ML_KEM_768_SECRET_KEY_LEN]
) {
    if (!seed || !out_public_key || !out_secret_key) {
        return ALRIOS_CRYPTO_ERR_NULL_PTR;
    }

#if defined(ALRIOS_HAVE_ML_KEM_768)
    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_from_name(NULL, "ML-KEM-768", NULL);
    if (!pctx) {
        return ALRIOS_CRYPTO_ERR_UNAVAILABLE;
    }
    if (EVP_PKEY_keygen_init(pctx) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return ALRIOS_CRYPTO_ERR_INTERNAL;
    }

    OSSL_PARAM kparams[] = {
        OSSL_PARAM_construct_octet_string("seed", (void *)(uintptr_t)seed, ML_KEM_768_SEED_LEN),
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

    size_t pub_len = ML_KEM_768_PUBLIC_KEY_LEN;
    size_t priv_len = ML_KEM_768_SECRET_KEY_LEN;

    if (EVP_PKEY_get_octet_string_param(pkey, OSSL_PKEY_PARAM_PUB_KEY,
                                        out_public_key, ML_KEM_768_PUBLIC_KEY_LEN,
                                        &pub_len) <= 0 ||
        pub_len != ML_KEM_768_PUBLIC_KEY_LEN) {
        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(pctx);
        alrios_explicit_zeroize(out_public_key, ML_KEM_768_PUBLIC_KEY_LEN);
        alrios_explicit_zeroize(out_secret_key, ML_KEM_768_SECRET_KEY_LEN);
        return ALRIOS_CRYPTO_ERR_INTERNAL;
    }

    if (EVP_PKEY_get_octet_string_param(pkey, OSSL_PKEY_PARAM_PRIV_KEY,
                                        out_secret_key, ML_KEM_768_SECRET_KEY_LEN,
                                        &priv_len) <= 0 ||
        priv_len != ML_KEM_768_SECRET_KEY_LEN) {
        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(pctx);
        alrios_explicit_zeroize(out_public_key, ML_KEM_768_PUBLIC_KEY_LEN);
        alrios_explicit_zeroize(out_secret_key, ML_KEM_768_SECRET_KEY_LEN);
        return ALRIOS_CRYPTO_ERR_INTERNAL;
    }

    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(pctx);
    return ALRIOS_CRYPTO_OK;
#else
    (void)seed;
    memset(out_public_key, 0, ML_KEM_768_PUBLIC_KEY_LEN);
    memset(out_secret_key, 0, ML_KEM_768_SECRET_KEY_LEN);
    return ALRIOS_CRYPTO_ERR_UNAVAILABLE;
#endif
}

int alrios_ml_kem_768_encapsulate(
    const uint8_t public_key[ML_KEM_768_PUBLIC_KEY_LEN],
    uint8_t out_ciphertext[ML_KEM_768_CIPHERTEXT_LEN],
    uint8_t out_shared_secret[ML_KEM_768_SHARED_SECRET_LEN]
) {
    if (!public_key || !out_ciphertext || !out_shared_secret) {
        return ALRIOS_CRYPTO_ERR_NULL_PTR;
    }

#if defined(ALRIOS_HAVE_ML_KEM_768)
    EVP_PKEY_CTX *fromdata = EVP_PKEY_CTX_new_from_name(NULL, "ML-KEM-768", NULL);
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
                                          ML_KEM_768_PUBLIC_KEY_LEN),
        OSSL_PARAM_construct_end()
    };
    EVP_PKEY *key = NULL;
    if (EVP_PKEY_fromdata(fromdata, &key, EVP_PKEY_PUBLIC_KEY, key_params) != 1 || !key) {
        EVP_PKEY_CTX_free(fromdata);
        return ALRIOS_CRYPTO_ERR_INVALID_KEY;
    }
    EVP_PKEY_CTX_free(fromdata);

    EVP_PKEY_CTX *enc_ctx = EVP_PKEY_CTX_new_from_pkey(NULL, key, NULL);
    if (!enc_ctx) {
        EVP_PKEY_free(key);
        return ALRIOS_CRYPTO_ERR_ALLOC_FAIL;
    }

    if (EVP_PKEY_encapsulate_init(enc_ctx, NULL) != 1) {
        EVP_PKEY_CTX_free(enc_ctx);
        EVP_PKEY_free(key);
        return ALRIOS_CRYPTO_ERR_INTERNAL;
    }

    size_t ct_len = ML_KEM_768_CIPHERTEXT_LEN;
    size_t ss_len = ML_KEM_768_SHARED_SECRET_LEN;
    int enc_res = EVP_PKEY_encapsulate(enc_ctx, out_ciphertext, &ct_len,
                                       out_shared_secret, &ss_len);
    EVP_PKEY_CTX_free(enc_ctx);
    EVP_PKEY_free(key);

    if (enc_res != 1 || ct_len != ML_KEM_768_CIPHERTEXT_LEN || ss_len != ML_KEM_768_SHARED_SECRET_LEN) {
        alrios_explicit_zeroize(out_ciphertext, ML_KEM_768_CIPHERTEXT_LEN);
        alrios_explicit_zeroize(out_shared_secret, ML_KEM_768_SHARED_SECRET_LEN);
        return ALRIOS_CRYPTO_ERR_INTERNAL;
    }

    return ALRIOS_CRYPTO_OK;
#else
    memset(out_ciphertext, 0, ML_KEM_768_CIPHERTEXT_LEN);
    memset(out_shared_secret, 0, ML_KEM_768_SHARED_SECRET_LEN);
    return ALRIOS_CRYPTO_ERR_UNAVAILABLE;
#endif
}

int alrios_ml_kem_768_decapsulate(
    const uint8_t secret_key[ML_KEM_768_SECRET_KEY_LEN],
    const uint8_t ciphertext[ML_KEM_768_CIPHERTEXT_LEN],
    uint8_t out_shared_secret[ML_KEM_768_SHARED_SECRET_LEN]
) {
    if (!secret_key || !ciphertext || !out_shared_secret) {
        return ALRIOS_CRYPTO_ERR_NULL_PTR;
    }

#if defined(ALRIOS_HAVE_ML_KEM_768)
    EVP_PKEY_CTX *fromdata = EVP_PKEY_CTX_new_from_name(NULL, "ML-KEM-768", NULL);
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
                                          ML_KEM_768_SECRET_KEY_LEN),
        OSSL_PARAM_construct_end()
    };
    EVP_PKEY *key = NULL;
    if (EVP_PKEY_fromdata(fromdata, &key, EVP_PKEY_KEYPAIR, key_params) != 1 || !key) {
        EVP_PKEY_CTX_free(fromdata);
        return ALRIOS_CRYPTO_ERR_INVALID_KEY;
    }
    EVP_PKEY_CTX_free(fromdata);

    EVP_PKEY_CTX *dec_ctx = EVP_PKEY_CTX_new_from_pkey(NULL, key, NULL);
    if (!dec_ctx) {
        EVP_PKEY_free(key);
        return ALRIOS_CRYPTO_ERR_ALLOC_FAIL;
    }

    if (EVP_PKEY_decapsulate_init(dec_ctx, NULL) != 1) {
        EVP_PKEY_CTX_free(dec_ctx);
        EVP_PKEY_free(key);
        return ALRIOS_CRYPTO_ERR_INTERNAL;
    }

    size_t ss_len = ML_KEM_768_SHARED_SECRET_LEN;
    int dec_res = EVP_PKEY_decapsulate(dec_ctx, out_shared_secret, &ss_len,
                                       ciphertext, ML_KEM_768_CIPHERTEXT_LEN);
    EVP_PKEY_CTX_free(dec_ctx);
    EVP_PKEY_free(key);

    if (dec_res != 1 || ss_len != ML_KEM_768_SHARED_SECRET_LEN) {
        alrios_explicit_zeroize(out_shared_secret, ML_KEM_768_SHARED_SECRET_LEN);
        return ALRIOS_CRYPTO_ERR_INTERNAL;
    }

    return ALRIOS_CRYPTO_OK;
#else
    (void)ciphertext;
    memset(out_shared_secret, 0, ML_KEM_768_SHARED_SECRET_LEN);
    return ALRIOS_CRYPTO_ERR_UNAVAILABLE;
#endif
}
