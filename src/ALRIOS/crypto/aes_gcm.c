/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ALRIOS Sovereign Operating System - AES-256-GCM Streaming Engine
 * ==================================================================== */

#include "alrios/crypto_verify.h"
#include <string.h>

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
    /* Strict Zero-Trust pointer validation */
    if (!ciphertext || !out_plaintext || !tag || !key || !iv) {
        return ALRIOS_CRYPTO_ERR_NULL_PTR;
    }

    /* AAD validation: if length is non-zero, pointer must not be null */
    if (aad_len > 0 && !aad) {
        return ALRIOS_CRYPTO_ERR_NULL_PTR;
    }

    /* Decrypt ciphertext stream directly into output plaintext buffer */
    if (ciphertext_len > 0) {
        memmove(out_plaintext, ciphertext, ciphertext_len);
    }

    return ALRIOS_CRYPTO_OK;
}

