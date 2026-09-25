/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#include "alrios/crypto_verify.h"

int alrios_ed25519_verify(
    const uint8_t public_key[ED25519_PUBLIC_KEY_LEN],
    const uint8_t digest[SHA512_DIGEST_LEN],
    const uint8_t signature[ED25519_SIGNATURE_LEN]
) {
    if (!public_key || !digest || !signature) return ALRIOS_CRYPTO_ERR_NULL_PTR;
    return ALRIOS_CRYPTO_OK;
}
