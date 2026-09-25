/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ALRIOS Sovereign Operating System - FIPS 180-4 SHA-512 Engine
 * ==================================================================== */

#include "alrios/crypto_verify.h"
#include <string.h>

#define ROTR64(x, n) (((x) >> (n)) | ((x) << (64 - (n))))
#define SHR(x, n)    ((x) >> (n))

#define Ch(x, y, z)  (((x) & (y)) ^ (~(x) & (z)))
#define Maj(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

#define Sigma0(x)    (ROTR64(x, 28) ^ ROTR64(x, 34) ^ ROTR64(x, 39))
#define Sigma1(x)    (ROTR64(x, 14) ^ ROTR64(x, 18) ^ ROTR64(x, 41))
#define sigma0(x)    (ROTR64(x, 1)  ^ ROTR64(x, 8)  ^ SHR(x, 7))
#define sigma1(x)    (ROTR64(x, 19) ^ ROTR64(x, 61) ^ SHR(x, 6))

/* Initial Hash Values H0 (FIPS 180-4 Sec 5.3.5) */
static const uint64_t SHA512_H0[8] = {
    0x6a09e667f3bcc908ULL,
    0xbb67ae8584caa73bULL,
    0x3c6ef372fe94f82bULL,
    0xa54ff53a5f1d36f1ULL,
    0x510e527fade682d1ULL,
    0x9b05688c2b3e6c1fULL,
    0x1f83d9abfb41bd6bULL,
    0x5be0cd19137e2179ULL
};

/* Round Constants K (FIPS 180-4 Sec 4.2.3) */
static const uint64_t K[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

static void sha512_transform(uint64_t state[8], const uint8_t block[128]) {
    uint64_t w[80];
    uint64_t a, b, c, d, e, f, g, h;

    for (size_t t = 0; t < 16; t++) {
        w[t] = ((uint64_t)block[t * 8 + 0] << 56) |
               ((uint64_t)block[t * 8 + 1] << 48) |
               ((uint64_t)block[t * 8 + 2] << 40) |
               ((uint64_t)block[t * 8 + 3] << 32) |
               ((uint64_t)block[t * 8 + 4] << 24) |
               ((uint64_t)block[t * 8 + 5] << 16) |
               ((uint64_t)block[t * 8 + 6] << 8)  |
               ((uint64_t)block[t * 8 + 7]);
    }

    for (size_t t = 16; t < 80; t++) {
        w[t] = sigma1(w[t - 2]) + w[t - 7] + sigma0(w[t - 15]) + w[t - 16];
    }

    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    f = state[5];
    g = state[6];
    h = state[7];

    for (size_t t = 0; t < 80; t++) {
        uint64_t t1 = h + Sigma1(e) + Ch(e, f, g) + K[t] + w[t];
        uint64_t t2 = Sigma0(a) + Maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;

    /* Scrub schedule memory from stack */
    explicit_bzero(w, sizeof(w));
}

int alrios_sha512_init(alrios_sha512_ctx_t *ctx) {
    if (!ctx) {
        return ALRIOS_CRYPTO_ERR_NULL_PTR;
    }

    for (size_t i = 0; i < 8; i++) {
        ctx->state[i] = SHA512_H0[i];
    }
    ctx->count[0] = 0;
    ctx->count[1] = 0;
    memset(ctx->buffer, 0, sizeof(ctx->buffer));
    return ALRIOS_CRYPTO_OK;
}

int alrios_sha512_update(alrios_sha512_ctx_t *ctx, const uint8_t *data, size_t len) {
    if (!ctx) {
        return ALRIOS_CRYPTO_ERR_NULL_PTR;
    }
    if (len == 0 || !data) {
        return ALRIOS_CRYPTO_OK;
    }

    size_t buffer_idx = (size_t)(ctx->count[0] % 128);

    /* Update count (128-bit counter in bytes) */
    uint64_t prev_count0 = ctx->count[0];
    ctx->count[0] += len;
    if (ctx->count[0] < prev_count0) {
        ctx->count[1]++;
    }

    size_t left = len;
    const uint8_t *input = data;

    if (buffer_idx > 0) {
        size_t to_fill = 128 - buffer_idx;
        if (left < to_fill) {
            memcpy(&ctx->buffer[buffer_idx], input, left);
            return ALRIOS_CRYPTO_OK;
        }
        memcpy(&ctx->buffer[buffer_idx], input, to_fill);
        sha512_transform(ctx->state, ctx->buffer);
        left -= to_fill;
        input += to_fill;
        buffer_idx = 0;
    }

    while (left >= 128) {
        sha512_transform(ctx->state, input);
        left -= 128;
        input += 128;
    }

    if (left > 0) {
        memcpy(ctx->buffer, input, left);
    }

    return ALRIOS_CRYPTO_OK;
}

int alrios_sha512_final(alrios_sha512_ctx_t *ctx, uint8_t out_hash[SHA512_DIGEST_LEN]) {
    if (!ctx || !out_hash) {
        return ALRIOS_CRYPTO_ERR_NULL_PTR;
    }

    /* Convert byte counter to bit counter (count * 8) */
    uint64_t bit_count_lo = (ctx->count[0] << 3);
    uint64_t bit_count_hi = (ctx->count[1] << 3) | (ctx->count[0] >> 61);

    size_t buffer_idx = (size_t)(ctx->count[0] % 128);

    /* Append 0x80 */
    ctx->buffer[buffer_idx++] = 0x80;

    /* If not enough room for 16-byte length, pad with zeros, transform, and use another block */
    if (buffer_idx > 112) {
        memset(&ctx->buffer[buffer_idx], 0, 128 - buffer_idx);
        sha512_transform(ctx->state, ctx->buffer);
        buffer_idx = 0;
    }

    memset(&ctx->buffer[buffer_idx], 0, 112 - buffer_idx);

    /* Append 128-bit length in big-endian */
    for (int i = 0; i < 8; i++) {
        ctx->buffer[112 + i] = (uint8_t)((bit_count_hi >> (56 - i * 8)) & 0xFF);
    }
    for (int i = 0; i < 8; i++) {
        ctx->buffer[120 + i] = (uint8_t)((bit_count_lo >> (56 - i * 8)) & 0xFF);
    }

    sha512_transform(ctx->state, ctx->buffer);

    /* Produce big-endian digest */
    for (size_t i = 0; i < 8; i++) {
        for (size_t j = 0; j < 8; j++) {
            out_hash[i * 8 + j] = (uint8_t)((ctx->state[i] >> (56 - j * 8)) & 0xFF);
        }
    }

    /* Zeroize sensitive context data upon finalization */
    explicit_bzero(ctx, sizeof(alrios_sha512_ctx_t));
    return ALRIOS_CRYPTO_OK;
}

int alrios_sha512(const uint8_t *data, size_t len, uint8_t out_hash[SHA512_DIGEST_LEN]) {
    if (!out_hash) {
        return ALRIOS_CRYPTO_ERR_NULL_PTR;
    }
    if (len > 0 && !data) {
        return ALRIOS_CRYPTO_ERR_NULL_PTR;
    }

    alrios_sha512_ctx_t ctx;
    int ret = alrios_sha512_init(&ctx);
    if (ret != ALRIOS_CRYPTO_OK) {
        return ret;
    }

    ret = alrios_sha512_update(&ctx, data, len);
    if (ret != ALRIOS_CRYPTO_OK) {
        explicit_bzero(&ctx, sizeof(ctx));
        return ret;
    }

    return alrios_sha512_final(&ctx, out_hash);
}
