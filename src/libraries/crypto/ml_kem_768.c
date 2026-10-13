/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/crypto/kem.h"
#include "alrios/crypto_verify.h"

#include <string.h>

#if defined(ALRIOS_HAVE_ML_KEM_768)
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/params.h>
#endif

static void ml_kem_clear_keypair(
    uint8_t out_public_key[ML_KEM_768_PUBLIC_KEY_LEN],
    uint8_t out_secret_key[ML_KEM_768_SECRET_KEY_LEN]
) {
    alrios_explicit_zeroize(out_public_key, ML_KEM_768_PUBLIC_KEY_LEN);
    alrios_explicit_zeroize(out_secret_key, ML_KEM_768_SECRET_KEY_LEN);
}

static void ml_kem_clear_encapsulation(
    uint8_t out_ciphertext[ML_KEM_768_CIPHERTEXT_LEN],
    uint8_t out_shared_secret[ML_KEM_768_SHARED_SECRET_LEN]
) {
    alrios_explicit_zeroize(out_ciphertext, ML_KEM_768_CIPHERTEXT_LEN);
    alrios_explicit_zeroize(out_shared_secret, ML_KEM_768_SHARED_SECRET_LEN);
}

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

#if defined(ALRIOS_HAVE_ML_KEM_768)
static int ml_kem_export_keypair(
    EVP_PKEY *key,
    uint8_t out_public_key[ML_KEM_768_PUBLIC_KEY_LEN],
    uint8_t out_secret_key[ML_KEM_768_SECRET_KEY_LEN]
) {
    size_t public_key_len = ML_KEM_768_PUBLIC_KEY_LEN;
    size_t secret_key_len = ML_KEM_768_SECRET_KEY_LEN;

    if (EVP_PKEY_get_octet_string_param(
            key,
            OSSL_PKEY_PARAM_PUB_KEY,
            out_public_key,
            ML_KEM_768_PUBLIC_KEY_LEN,
            &public_key_len
        ) != 1 || public_key_len != ML_KEM_768_PUBLIC_KEY_LEN) {
        ml_kem_clear_keypair(out_public_key, out_secret_key);
        return ALRIOS_CRYPTO_ERR_INTERNAL;
    }

    if (EVP_PKEY_get_octet_string_param(
            key,
            OSSL_PKEY_PARAM_PRIV_KEY,
            out_secret_key,
            ML_KEM_768_SECRET_KEY_LEN,
            &secret_key_len
        ) != 1 || secret_key_len != ML_KEM_768_SECRET_KEY_LEN) {
        ml_kem_clear_keypair(out_public_key, out_secret_key);
        return ALRIOS_CRYPTO_ERR_INTERNAL;
    }

    return ALRIOS_CRYPTO_OK;
}

static int ml_kem_generate_keypair(
    const uint8_t *seed,
    uint8_t out_public_key[ML_KEM_768_PUBLIC_KEY_LEN],
    uint8_t out_secret_key[ML_KEM_768_SECRET_KEY_LEN]
) {
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, "ML-KEM-768", NULL);
    EVP_PKEY *key = NULL;
    int result = ALRIOS_CRYPTO_ERR_INTERNAL;

    if (!ctx) {
        return ALRIOS_CRYPTO_ERR_UNAVAILABLE;
    }
    if (EVP_PKEY_keygen_init(ctx) != 1) {
        EVP_PKEY_CTX_free(ctx);
        return ALRIOS_CRYPTO_ERR_INTERNAL;
    }

    if (seed) {
        OSSL_PARAM params[] = {
            OSSL_PARAM_construct_octet_string(
                OSSL_PKEY_PARAM_ML_KEM_SEED,
                (void *)(uintptr_t)seed,
                ML_KEM_768_KEYGEN_SEED_LEN
            ),
            OSSL_PARAM_construct_end()
        };
        if (EVP_PKEY_CTX_set_params(ctx, params) != 1) {
            EVP_PKEY_CTX_free(ctx);
            return ALRIOS_CRYPTO_ERR_INTERNAL;
        }
    }

    if (EVP_PKEY_generate(ctx, &key) != 1 || !key) {
        EVP_PKEY_CTX_free(ctx);
        return ALRIOS_CRYPTO_ERR_INTERNAL;
    }

    result = ml_kem_export_keypair(key, out_public_key, out_secret_key);
    EVP_PKEY_free(key);
    EVP_PKEY_CTX_free(ctx);
    return result;
}

static EVP_PKEY *ml_kem_import_public_key(
    const uint8_t public_key[ML_KEM_768_PUBLIC_KEY_LEN]
) {
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, "ML-KEM-768", NULL);
    EVP_PKEY *key = NULL;
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_octet_string(
            OSSL_PKEY_PARAM_PUB_KEY,
            (void *)(uintptr_t)public_key,
            ML_KEM_768_PUBLIC_KEY_LEN
        ),
        OSSL_PARAM_construct_end()
    };

    if (!ctx) {
        return NULL;
    }
    if (EVP_PKEY_fromdata_init(ctx) != 1 ||
        EVP_PKEY_fromdata(ctx, &key, EVP_PKEY_PUBLIC_KEY, params) != 1) {
        EVP_PKEY_free(key);
        key = NULL;
    }
    EVP_PKEY_CTX_free(ctx);
    return key;
}

static EVP_PKEY *ml_kem_import_secret_key(
    const uint8_t secret_key[ML_KEM_768_SECRET_KEY_LEN]
) {
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, "ML-KEM-768", NULL);
    EVP_PKEY *key = NULL;
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_octet_string(
            OSSL_PKEY_PARAM_PRIV_KEY,
            (void *)(uintptr_t)secret_key,
            ML_KEM_768_SECRET_KEY_LEN
        ),
        OSSL_PARAM_construct_end()
    };

    if (!ctx) {
        return NULL;
    }
    if (EVP_PKEY_fromdata_init(ctx) != 1 ||
        EVP_PKEY_fromdata(ctx, &key, EVP_PKEY_KEYPAIR, params) != 1) {
        EVP_PKEY_free(key);
        key = NULL;
    }
    EVP_PKEY_CTX_free(ctx);
    return key;
}

static int ml_kem_encapsulate(
    const uint8_t public_key[ML_KEM_768_PUBLIC_KEY_LEN],
    const uint8_t *seed,
    uint8_t out_ciphertext[ML_KEM_768_CIPHERTEXT_LEN],
    uint8_t out_shared_secret[ML_KEM_768_SHARED_SECRET_LEN]
) {
    EVP_PKEY *key = ml_kem_import_public_key(public_key);
    EVP_PKEY_CTX *ctx = NULL;
    size_t ciphertext_len = ML_KEM_768_CIPHERTEXT_LEN;
    size_t shared_secret_len = ML_KEM_768_SHARED_SECRET_LEN;
    int initialized;
    int result = ALRIOS_CRYPTO_ERR_INTERNAL;

    if (!key) {
        ml_kem_clear_encapsulation(out_ciphertext, out_shared_secret);
        return ALRIOS_CRYPTO_ERR_INVALID_KEY;
    }

    ctx = EVP_PKEY_CTX_new(key, NULL);
    if (!ctx) {
        EVP_PKEY_free(key);
        ml_kem_clear_encapsulation(out_ciphertext, out_shared_secret);
        return ALRIOS_CRYPTO_ERR_ALLOC_FAIL;
    }

    if (seed) {
        OSSL_PARAM params[] = {
            OSSL_PARAM_construct_octet_string(
                OSSL_KEM_PARAM_IKME,
                (void *)(uintptr_t)seed,
                ML_KEM_768_ENCAPS_SEED_LEN
            ),
            OSSL_PARAM_construct_end()
        };
        initialized = EVP_PKEY_encapsulate_init(ctx, params);
    } else {
        initialized = EVP_PKEY_encapsulate_init(ctx, NULL);
    }

    if (initialized == 1 &&
        EVP_PKEY_encapsulate(
            ctx,
            out_ciphertext,
            &ciphertext_len,
            out_shared_secret,
            &shared_secret_len
        ) == 1 &&
        ciphertext_len == ML_KEM_768_CIPHERTEXT_LEN &&
        shared_secret_len == ML_KEM_768_SHARED_SECRET_LEN) {
        result = ALRIOS_CRYPTO_OK;
    } else {
        ml_kem_clear_encapsulation(out_ciphertext, out_shared_secret);
    }

    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(key);
    return result;
}
#endif

int alrios_ml_kem_768_keypair(
    uint8_t out_public_key[ML_KEM_768_PUBLIC_KEY_LEN],
    uint8_t out_secret_key[ML_KEM_768_SECRET_KEY_LEN]
) {
    if (!out_public_key || !out_secret_key) {
        return ALRIOS_CRYPTO_ERR_NULL_PTR;
    }
#if defined(ALRIOS_HAVE_ML_KEM_768)
    return ml_kem_generate_keypair(NULL, out_public_key, out_secret_key);
#else
    ml_kem_clear_keypair(out_public_key, out_secret_key);
    return ALRIOS_CRYPTO_ERR_UNAVAILABLE;
#endif
}

int alrios_ml_kem_768_keypair_from_seed(
    const uint8_t seed[ML_KEM_768_KEYGEN_SEED_LEN],
    uint8_t out_public_key[ML_KEM_768_PUBLIC_KEY_LEN],
    uint8_t out_secret_key[ML_KEM_768_SECRET_KEY_LEN]
) {
    if (!seed || !out_public_key || !out_secret_key) {
        return ALRIOS_CRYPTO_ERR_NULL_PTR;
    }
#if defined(ALRIOS_HAVE_ML_KEM_768)
    return ml_kem_generate_keypair(seed, out_public_key, out_secret_key);
#else
    ml_kem_clear_keypair(out_public_key, out_secret_key);
    return ALRIOS_CRYPTO_ERR_UNAVAILABLE;
#endif
}

int alrios_ml_kem_768_encaps(
    const uint8_t public_key[ML_KEM_768_PUBLIC_KEY_LEN],
    uint8_t out_ciphertext[ML_KEM_768_CIPHERTEXT_LEN],
    uint8_t out_shared_secret[ML_KEM_768_SHARED_SECRET_LEN]
) {
    if (!public_key || !out_ciphertext || !out_shared_secret) {
        return ALRIOS_CRYPTO_ERR_NULL_PTR;
    }
#if defined(ALRIOS_HAVE_ML_KEM_768)
    return ml_kem_encapsulate(public_key, NULL, out_ciphertext, out_shared_secret);
#else
    ml_kem_clear_encapsulation(out_ciphertext, out_shared_secret);
    return ALRIOS_CRYPTO_ERR_UNAVAILABLE;
#endif
}

int alrios_ml_kem_768_encaps_from_seed(
    const uint8_t public_key[ML_KEM_768_PUBLIC_KEY_LEN],
    const uint8_t seed[ML_KEM_768_ENCAPS_SEED_LEN],
    uint8_t out_ciphertext[ML_KEM_768_CIPHERTEXT_LEN],
    uint8_t out_shared_secret[ML_KEM_768_SHARED_SECRET_LEN]
) {
    if (!public_key || !seed || !out_ciphertext || !out_shared_secret) {
        return ALRIOS_CRYPTO_ERR_NULL_PTR;
    }
#if defined(ALRIOS_HAVE_ML_KEM_768)
    return ml_kem_encapsulate(public_key, seed, out_ciphertext, out_shared_secret);
#else
    ml_kem_clear_encapsulation(out_ciphertext, out_shared_secret);
    return ALRIOS_CRYPTO_ERR_UNAVAILABLE;
#endif
}

int alrios_ml_kem_768_decaps(
    const uint8_t secret_key[ML_KEM_768_SECRET_KEY_LEN],
    const uint8_t ciphertext[ML_KEM_768_CIPHERTEXT_LEN],
    uint8_t out_shared_secret[ML_KEM_768_SHARED_SECRET_LEN]
) {
    if (!secret_key || !ciphertext || !out_shared_secret) {
        return ALRIOS_CRYPTO_ERR_NULL_PTR;
    }
#if defined(ALRIOS_HAVE_ML_KEM_768)
    EVP_PKEY *key = ml_kem_import_secret_key(secret_key);
    EVP_PKEY_CTX *ctx = NULL;
    size_t shared_secret_len = ML_KEM_768_SHARED_SECRET_LEN;
    int result = ALRIOS_CRYPTO_ERR_INTERNAL;

    if (!key) {
        alrios_explicit_zeroize(out_shared_secret, ML_KEM_768_SHARED_SECRET_LEN);
        return ALRIOS_CRYPTO_ERR_INVALID_KEY;
    }

    ctx = EVP_PKEY_CTX_new(key, NULL);
    if (!ctx) {
        EVP_PKEY_free(key);
        alrios_explicit_zeroize(out_shared_secret, ML_KEM_768_SHARED_SECRET_LEN);
        return ALRIOS_CRYPTO_ERR_ALLOC_FAIL;
    }

    if (EVP_PKEY_decapsulate_init(ctx, NULL) == 1 &&
        EVP_PKEY_decapsulate(
            ctx,
            out_shared_secret,
            &shared_secret_len,
            ciphertext,
            ML_KEM_768_CIPHERTEXT_LEN
        ) == 1 &&
        shared_secret_len == ML_KEM_768_SHARED_SECRET_LEN) {
        result = ALRIOS_CRYPTO_OK;
    } else {
        alrios_explicit_zeroize(out_shared_secret, ML_KEM_768_SHARED_SECRET_LEN);
    }

    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(key);
    return result;
#else
    alrios_explicit_zeroize(out_shared_secret, ML_KEM_768_SHARED_SECRET_LEN);
    return ALRIOS_CRYPTO_ERR_UNAVAILABLE;
#endif
}
