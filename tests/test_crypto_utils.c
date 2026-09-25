/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */

#include "alrios/crypto_verify.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int main(void) {
    uint8_t buf1[64];
    uint8_t buf2[64];
    memset(buf1, 0xA5, sizeof(buf1));
    memset(buf2, 0xA5, sizeof(buf2));

    assert(alrios_constant_time_memcmp(buf1, buf2, sizeof(buf1)) == 0);

    buf2[10] ^= 0x01;
    assert(alrios_constant_time_memcmp(buf1, buf2, sizeof(buf1)) != 0);

    alrios_explicit_zeroize(buf1, sizeof(buf1));
    for (size_t i = 0; i < sizeof(buf1); i++) {
        assert(buf1[i] == 0x00);
    }

    printf("TASK-001 (Crypto Utils): PASS\n");
    return 0;
}
