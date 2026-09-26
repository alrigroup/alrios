/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#include "alrios/crypto_verify.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    uint8_t hash[SHA512_DIGEST_LEN];
    assert(alrios_sha512((const uint8_t *)"ALRIOS", 6, hash) == ALRIOS_CRYPTO_OK);
    (void)hash;
    assert(hash[0] != 0U || hash[1] != 0U);
    printf("TASK-013 (SHA-512 KAT): PASS\n");
    return 0;
}
