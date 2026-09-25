/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */

#include "alrios/arapp_format.h"
#include "alrios/crypto_verify.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

_Static_assert(sizeof(arapp_header_prefix_v1_t) == 136,
               "Pre-image prefix structure must be exactly 136 bytes");

/**
 * @brief Checks if two memory buffers [a, a + a_len) and [b, b + b_len) overlap.
 * Safe against pointer integer wrapping.
 */
static inline int buffers_overlap(const void *a, size_t a_len, const void *b, size_t b_len) {
    if (!a || !b || a_len == 0 || b_len == 0) {
        return 0;
    }
    uintptr_t a_start = (uintptr_t)a;
    uintptr_t a_end = a_start + a_len;
    uintptr_t b_start = (uintptr_t)b;
    uintptr_t b_end = b_start + b_len;

    /* Detect pointer arithmetic wrap-around */
    if (a_end < a_start || b_end < b_start) {
        return 1;
    }
    return (a_start < b_end) && (b_start < a_end);
}

int alrios_build_preimage_with_len(
    const arapp_header_prefix_v1_t *pfx,
    const uint8_t *manifest, size_t m_len,
    const uint8_t *ct, size_t c_len,
    uint8_t *out_preimage, size_t max_out,
    size_t *out_len
) {
    /* 1. Strict Zero-Trust Argument & Pointer Validation */
    if (!pfx || !out_preimage) {
        return -1;
    }
    if (m_len > 0 && !manifest) {
        return -1;
    }
    if (c_len > 0 && !ct) {
        return -1;
    }

    /* 2. Arithmetic Overflow & Capacity Checks */
    const size_t pfx_len = sizeof(*pfx);
    if (m_len > SIZE_MAX - pfx_len) {
        return -2;
    }
    size_t prefix_manifest_len = pfx_len + m_len;
    if (c_len > SIZE_MAX - prefix_manifest_len) {
        return -2;
    }
    size_t total_len = prefix_manifest_len + c_len;

    if (total_len > max_out) {
        return -2;
    }

    /* 3. Memory Overlap & Pointer Aliasing Protection */
    if (buffers_overlap(out_preimage, total_len, pfx, pfx_len) ||
        (m_len > 0 && buffers_overlap(out_preimage, total_len, manifest, m_len)) ||
        (c_len > 0 && buffers_overlap(out_preimage, total_len, ct, c_len))) {
        return -1;
    }

    /* 4. Deterministic Pre-image Stream Concatenation: Prefix || Manifest || Ciphertext */
    memcpy(out_preimage, pfx, pfx_len);

    if (m_len > 0) {
        memcpy(out_preimage + pfx_len, manifest, m_len);
    }

    if (c_len > 0) {
        memcpy(out_preimage + prefix_manifest_len, ct, c_len);
    }

    if (out_len) {
        *out_len = total_len;
    }

    return 0;
}

int alrios_build_preimage(
    const arapp_header_prefix_v1_t *pfx,
    const uint8_t *manifest, size_t m_len,
    const uint8_t *ct, size_t c_len,
    uint8_t *out_preimage, size_t max_out
) {
    return alrios_build_preimage_with_len(
        pfx, manifest, m_len, ct, c_len, out_preimage, max_out, NULL
    );
}

int alrios_preimage_compute_digest(
    const arapp_header_prefix_v1_t *pfx,
    const uint8_t *manifest, size_t m_len,
    const uint8_t *ct, size_t c_len,
    uint8_t out_digest[ARAPP_SHA512_LEN]
) {
    /* 1. Strict Zero-Trust Argument Validation */
    if (!pfx || !out_digest) {
        return -1;
    }
    if (m_len > 0 && !manifest) {
        return -1;
    }
    if (c_len > 0 && !ct) {
        return -1;
    }

    alrios_sha512_ctx_t ctx;
    if (alrios_sha512_init(&ctx) != ALRIOS_CRYPTO_OK) {
        return -1;
    }

    if (alrios_sha512_update(&ctx, (const uint8_t *)pfx, sizeof(*pfx)) != ALRIOS_CRYPTO_OK) {
        alrios_explicit_zeroize(&ctx, sizeof(ctx));
        return -1;
    }

    if (m_len > 0) {
        if (alrios_sha512_update(&ctx, manifest, m_len) != ALRIOS_CRYPTO_OK) {
            alrios_explicit_zeroize(&ctx, sizeof(ctx));
            return -1;
        }
    }

    if (c_len > 0) {
        if (alrios_sha512_update(&ctx, ct, c_len) != ALRIOS_CRYPTO_OK) {
            alrios_explicit_zeroize(&ctx, sizeof(ctx));
            return -1;
        }
    }

    if (alrios_sha512_final(&ctx, out_digest) != ALRIOS_CRYPTO_OK) {
        alrios_explicit_zeroize(&ctx, sizeof(ctx));
        return -1;
    }

    alrios_explicit_zeroize(&ctx, sizeof(ctx));
    return 0;
}
