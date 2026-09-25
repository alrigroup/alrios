/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#include "alrios/crypto_verify.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    uint8_t public_key[ML_DSA_65_PUBLIC_KEY_LEN] = {0};
    uint8_t digest[SHA512_DIGEST_LEN] = {0};
    uint8_t signature[ML_DSA_65_SIGNATURE_LEN] = {0};

#if defined(ALRIOS_HAVE_ML_DSA_65)
    int result = alrios_ml_dsa_65_verify(public_key, digest, signature);
    assert(result == ALRIOS_CRYPTO_ERR_INVALID_KEY ||
           result == ALRIOS_CRYPTO_ERR_BAD_SIG_PQC ||
           result == ALRIOS_CRYPTO_ERR_INTERNAL);
#else
    assert(alrios_ml_dsa_65_verify(public_key, digest, signature) ==
           ALRIOS_CRYPTO_ERR_UNAVAILABLE);
#endif

    printf("TASK-015 (ML-DSA-65 Capability Gate): PASS\n");
    return 0;
}
