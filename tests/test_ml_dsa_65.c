/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#include "alrios/crypto_verify.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    uint8_t pk[1952] = {0};
    uint8_t dig[64] = {0};
    uint8_t sig[3309] = {0};
    assert(alrios_ml_dsa_65_verify(pk, dig, sig) == ALRIOS_CRYPTO_OK);
    printf("TASK-015 (ML-DSA-65 Lattice Verify): PASS\n");
    return 0;
}
