/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */

#ifndef ALRIOS_CRYPTO_VERIFY_H
#define ALRIOS_CRYPTO_VERIFY_H

#include <stdint.h>
#include <stddef.h>

#define ALRIOS_CRYPTO_OK                    0
#define ALRIOS_CRYPTO_ERR_NULL_PTR         -1001
#define ALRIOS_CRYPTO_ERR_BAD_SIG_CLASSIC  -1002
#define ALRIOS_CRYPTO_ERR_BAD_SIG_PQC      -1003
#define ALRIOS_CRYPTO_ERR_GCM_AUTH_FAIL    -1004
#define ALRIOS_CRYPTO_ERR_ALLOC_FAIL       -1005
#define ALRIOS_CRYPTO_ERR_UNAVAILABLE      -1006
#define ALRIOS_CRYPTO_ERR_INVALID_KEY      -1007
#define ALRIOS_CRYPTO_ERR_INTERNAL         -1008

#define ALRIOS_CRYPTO_CAP_ED25519           (1U << 0)
#define ALRIOS_CRYPTO_CAP_AES_256_GCM       (1U << 1)
#define ALRIOS_CRYPTO_CAP_ML_DSA_65         (1U << 2)

#define ED25519_PUBLIC_KEY_LEN             32
#define ED25519_SIGNATURE_LEN              64
#define ML_DSA_65_PUBLIC_KEY_LEN           1952
#define ML_DSA_65_SIGNATURE_LEN            3309
#define AES256_KEY_LEN                     32
#define AES256_GCM_IV_LEN                  12
#define AES256_GCM_TAG_LEN                 16
#define SHA512_DIGEST_LEN                  64

void alrios_explicit_zeroize(void *ptr, size_t len);

int alrios_constant_time_memcmp(const void *a, const void *b, size_t len);

uint32_t alrios_crypto_capabilities(void);
int alrios_crypto_has_capability(uint32_t capability);

/* FIPS 180-4 SHA-512 Streaming Context */
typedef struct alrios_sha512_ctx {
    uint64_t state[8];
    uint64_t count[2];  /* Number of bytes processed (128-bit integer, count[0]=low, count[1]=high) */
    uint8_t  buffer[128];
} alrios_sha512_ctx_t;

int alrios_sha512_init(alrios_sha512_ctx_t *ctx);
int alrios_sha512_update(alrios_sha512_ctx_t *ctx, const uint8_t *data, size_t len);
int alrios_sha512_final(alrios_sha512_ctx_t *ctx, uint8_t out_hash[SHA512_DIGEST_LEN]);

int alrios_sha512(const uint8_t *data, size_t len, uint8_t out_hash[SHA512_DIGEST_LEN]);

int alrios_ed25519_verify(
    const uint8_t public_key[ED25519_PUBLIC_KEY_LEN],
    const uint8_t digest[SHA512_DIGEST_LEN],
    const uint8_t signature[ED25519_SIGNATURE_LEN]
);

int alrios_ml_dsa_65_verify(
    const uint8_t public_key[ML_DSA_65_PUBLIC_KEY_LEN],
    const uint8_t digest[SHA512_DIGEST_LEN],
    const uint8_t signature[ML_DSA_65_SIGNATURE_LEN]
);

int alrios_aes256_gcm_decrypt(
    const uint8_t *ciphertext,
    size_t ciphertext_len,
    const uint8_t *aad,
    size_t aad_len,
    const uint8_t tag[AES256_GCM_TAG_LEN],
    const uint8_t key[AES256_KEY_LEN],
    const uint8_t iv[AES256_GCM_IV_LEN],
    uint8_t *out_plaintext
);

#endif /* ALRIOS_CRYPTO_VERIFY_H */
