/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#include "alrios/crypto_verify.h"

#include <openssl/evp.h>

int alrios_ed25519_verify(
    const uint8_t public_key[ED25519_PUBLIC_KEY_LEN],
    const uint8_t digest[SHA512_DIGEST_LEN],
    const uint8_t signature[ED25519_SIGNATURE_LEN]
) {
    if (!public_key || !digest || !signature) {
        return ALRIOS_CRYPTO_ERR_NULL_PTR;
    }

    EVP_PKEY *key = EVP_PKEY_new_raw_public_key(
        EVP_PKEY_ED25519, NULL, public_key, ED25519_PUBLIC_KEY_LEN);
    if (!key) {
        return ALRIOS_CRYPTO_ERR_INVALID_KEY;
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        EVP_PKEY_free(key);
        return ALRIOS_CRYPTO_ERR_ALLOC_FAIL;
    }

    int result = ALRIOS_CRYPTO_ERR_INTERNAL;
    if (EVP_DigestVerifyInit(ctx, NULL, NULL, NULL, key) == 1) {
        int verify = EVP_DigestVerify(ctx, signature, ED25519_SIGNATURE_LEN,
                                     digest, SHA512_DIGEST_LEN);
        result = verify == 1 ? ALRIOS_CRYPTO_OK : ALRIOS_CRYPTO_ERR_BAD_SIG_CLASSIC;
    }

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(key);
    return result;
}
