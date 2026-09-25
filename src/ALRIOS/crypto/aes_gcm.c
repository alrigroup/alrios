/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ALRIOS Sovereign Operating System - AES-256-GCM Streaming Engine
 * ==================================================================== */

#include "alrios/crypto_verify.h"

#include <limits.h>
#include <openssl/evp.h>

int alrios_aes256_gcm_decrypt(
    const uint8_t *ciphertext,
    size_t ciphertext_len,
    const uint8_t *aad,
    size_t aad_len,
    const uint8_t tag[AES256_GCM_TAG_LEN],
    const uint8_t key[AES256_KEY_LEN],
    const uint8_t iv[AES256_GCM_IV_LEN],
    uint8_t *out_plaintext
) {
    if (!ciphertext || !out_plaintext || !tag || !key || !iv) {
        return ALRIOS_CRYPTO_ERR_NULL_PTR;
    }
    if (aad_len > 0U && !aad) {
        return ALRIOS_CRYPTO_ERR_NULL_PTR;
    }
    if (ciphertext_len > (size_t)INT_MAX || aad_len > (size_t)INT_MAX) {
        return ALRIOS_CRYPTO_ERR_INTERNAL;
    }

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return ALRIOS_CRYPTO_ERR_ALLOC_FAIL;
    }

    int output_len = 0;
    int final_len = 0;
    int result = ALRIOS_CRYPTO_ERR_INTERNAL;

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, AES256_GCM_IV_LEN, NULL) != 1 ||
        EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
        goto cleanup;
    }

    if (aad_len > 0U &&
        EVP_DecryptUpdate(ctx, NULL, &output_len, aad, (int)aad_len) != 1) {
        goto cleanup;
    }

    if (ciphertext_len > 0U &&
        EVP_DecryptUpdate(ctx, out_plaintext, &output_len,
                          ciphertext, (int)ciphertext_len) != 1) {
        goto cleanup;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG,
                            AES256_GCM_TAG_LEN, (void *)(uintptr_t)tag) != 1) {
        goto cleanup;
    }

    if (EVP_DecryptFinal_ex(ctx, out_plaintext + output_len, &final_len) != 1) {
        alrios_explicit_zeroize(out_plaintext, ciphertext_len);
        result = ALRIOS_CRYPTO_ERR_GCM_AUTH_FAIL;
        goto cleanup;
    }

    result = ALRIOS_CRYPTO_OK;

cleanup:
    EVP_CIPHER_CTX_free(ctx);
    return result;
}

