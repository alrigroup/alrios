/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */

#include "alrios/crypto_verify.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

void alrios_explicit_zeroize(void *ptr, size_t len) {
    if (!ptr || len == 0) {
        return;
    }
    volatile unsigned char *p = (volatile unsigned char *)ptr;
    while (len--) {
        *p++ = 0x00;
    }
#if defined(_MSC_VER)
    _ReadWriteBarrier();
#elif defined(__GNUC__) || defined(__clang__)
    __asm__ __volatile__("" : : "r"(ptr) : "memory");
#endif
}

int alrios_constant_time_memcmp(const void *a, const void *b, size_t len) {
    if (!a || !b) {
        return -1;
    }
    const volatile unsigned char *ua = (const volatile unsigned char *)a;
    const volatile unsigned char *ub = (const volatile unsigned char *)b;
    unsigned char result = 0;

    for (size_t i = 0; i < len; i++) {
        result |= (ua[i] ^ ub[i]);
    }

    /* Branchless constant-time conversion: 0 if equal, -1 if any byte differs */
    unsigned int diff = (unsigned int)result;
    diff |= diff >> 4;
    diff |= diff >> 2;
    diff |= diff >> 1;
    diff &= 1U;

    return -((int)diff);
}
