/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#include "alrios/crypto_verify.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    uint8_t pk[32] = {0};
    uint8_t dig[64] = {0};
    uint8_t sig[64] = {0};
    assert(alrios_ed25519_verify(pk, dig, sig) == ALRIOS_CRYPTO_OK);
    printf("TASK-014 (Ed25519 Verify): PASS\n");
    return 0;
}
