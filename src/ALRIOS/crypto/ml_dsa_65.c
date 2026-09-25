/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#include "alrios/crypto_verify.h"

#if defined(ALRIOS_HAVE_ML_DSA_65)
#include <openssl/evp.h>
#include <openssl/core_names.h>
#endif

int alrios_ml_dsa_65_verify(
    const uint8_t public_key[ML_DSA_65_PUBLIC_KEY_LEN],
    const uint8_t digest[SHA512_DIGEST_LEN],
    const uint8_t signature[ML_DSA_65_SIGNATURE_LEN]
) {
    if (!public_key || !digest || !signature) {
        return ALRIOS_CRYPTO_ERR_NULL_PTR;
    }

#if defined(ALRIOS_HAVE_ML_DSA_65)
    EVP_PKEY_CTX *fromdata = EVP_PKEY_CTX_new_from_name(NULL, "ML-DSA-65", NULL);
    if (!fromdata) return ALRIOS_CRYPTO_ERR_UNAVAILABLE;
    if (EVP_PKEY_fromdata_init(fromdata) != 1) {
        EVP_PKEY_CTX_free(fromdata);
        return ALRIOS_CRYPTO_ERR_INTERNAL;
    }

    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY,
                                          (void *)(uintptr_t)public_key,
                                          ML_DSA_65_PUBLIC_KEY_LEN),
        OSSL_PARAM_construct_end()
    };
    EVP_PKEY *key = NULL;
    if (EVP_PKEY_fromdata(fromdata, &key, EVP_PKEY_PUBLIC_KEY, params) != 1) {
        EVP_PKEY_CTX_free(fromdata);
        return ALRIOS_CRYPTO_ERR_INVALID_KEY;
    }
    EVP_PKEY_CTX_free(fromdata);

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        EVP_PKEY_free(key);
        return ALRIOS_CRYPTO_ERR_ALLOC_FAIL;
    }

    int result = ALRIOS_CRYPTO_ERR_INTERNAL;
    if (EVP_DigestVerifyInit(ctx, NULL, NULL, NULL, key) == 1) {
        int verify = EVP_DigestVerify(ctx, signature, ML_DSA_65_SIGNATURE_LEN,
                                     digest, SHA512_DIGEST_LEN);
        result = verify == 1 ? ALRIOS_CRYPTO_OK : ALRIOS_CRYPTO_ERR_BAD_SIG_PQC;
    }

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(key);
    return result;
#else
    (void)public_key;
    (void)digest;
    (void)signature;
    return ALRIOS_CRYPTO_ERR_UNAVAILABLE;
#endif
}
