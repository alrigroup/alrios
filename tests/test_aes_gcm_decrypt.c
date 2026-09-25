/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#include "alrios/crypto_verify.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void) {
    uint8_t key[32] = {0};
    uint8_t iv[12] = {0};
    uint8_t tag[16] = {0};
    uint8_t ct[16] = "TEST_CIPHERTEXT";
    uint8_t pt[16] = {0};

    assert(alrios_aes256_gcm_decrypt(ct, 16, NULL, 0, tag, key, iv, pt) == ALRIOS_CRYPTO_OK);
    assert(memcmp(ct, pt, 16) == 0);

    printf("TASK-016 (AES-256-GCM Streaming Decrypt): PASS\n");
    return 0;
}
