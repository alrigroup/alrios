/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_CRYPTO_KEM_H
#define ALRIOS_CRYPTO_KEM_H

#include <stdint.h>
#include <stddef.h>

#include "alrios/crypto_verify.h"

#define ML_KEM_768_PUBLIC_KEY_LEN       1184
#define ML_KEM_768_SECRET_KEY_LEN       2400
#define ML_KEM_768_CIPHERTEXT_LEN       1088
#define ML_KEM_768_SHARED_SECRET_LEN    32
#define ML_KEM_768_SEED_LEN             64

int alrios_ml_kem_768_provider_available(void);

int alrios_ml_kem_768_keypair(
    uint8_t out_public_key[ML_KEM_768_PUBLIC_KEY_LEN],
    uint8_t out_secret_key[ML_KEM_768_SECRET_KEY_LEN]
);

int alrios_ml_kem_768_keypair_from_seed(
    const uint8_t seed[ML_KEM_768_SEED_LEN],
    uint8_t out_public_key[ML_KEM_768_PUBLIC_KEY_LEN],
    uint8_t out_secret_key[ML_KEM_768_SECRET_KEY_LEN]
);

int alrios_ml_kem_768_encapsulate(
    const uint8_t public_key[ML_KEM_768_PUBLIC_KEY_LEN],
    uint8_t out_ciphertext[ML_KEM_768_CIPHERTEXT_LEN],
    uint8_t out_shared_secret[ML_KEM_768_SHARED_SECRET_LEN]
);

int alrios_ml_kem_768_decapsulate(
    const uint8_t secret_key[ML_KEM_768_SECRET_KEY_LEN],
    const uint8_t ciphertext[ML_KEM_768_CIPHERTEXT_LEN],
    uint8_t out_shared_secret[ML_KEM_768_SHARED_SECRET_LEN]
);

#endif /* ALRIOS_CRYPTO_KEM_H */
