/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */

#include "alrios/arapp_format.h"
#include "alrios/crypto_verify.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int alrios_build_preimage(
    const arapp_header_prefix_v1_t *pfx,
    const uint8_t *manifest, size_t m_len,
    const uint8_t *ct, size_t c_len,
    uint8_t *out_preimage, size_t max_out
);

int alrios_build_preimage_with_len(
    const arapp_header_prefix_v1_t *pfx,
    const uint8_t *manifest, size_t m_len,
    const uint8_t *ct, size_t c_len,
    uint8_t *out_preimage, size_t max_out,
    size_t *out_len
);

int alrios_preimage_compute_digest(
    const arapp_header_prefix_v1_t *pfx,
    const uint8_t *manifest, size_t m_len,
    const uint8_t *ct, size_t c_len,
    uint8_t out_digest[SHA512_DIGEST_LEN]
);

static void test_basic_construction(void) {
    arapp_header_prefix_v1_t pfx;
    memset(&pfx, 0x11, sizeof(pfx));
    uint8_t buf[256];
    (void)buf;
    assert(alrios_build_preimage(&pfx, NULL, 0, NULL, 0, buf, sizeof(buf)) == 0);
    assert(memcmp(buf, &pfx, sizeof(pfx)) == 0);
}

static void test_full_stream_concatenation(void) {
    arapp_header_prefix_v1_t pfx;
    memset(&pfx, 0xAA, sizeof(pfx));
    const uint8_t manifest[] = "{\"app\":\"test_app\"}";
    const size_t m_len = sizeof(manifest) - 1;

    const uint8_t ct[32] = {
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
        0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
        0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67
    };
    const size_t c_len = sizeof(ct);

    const size_t expected_total = sizeof(pfx) + m_len + c_len;
    (void)expected_total;
    uint8_t out[512];
    (void)out;
    size_t out_len = 0;
    (void)out_len;

    assert(alrios_build_preimage_with_len(&pfx, manifest, m_len, ct, c_len, out, sizeof(out), &out_len) == 0);
    assert(out_len == expected_total);

    assert(memcmp(out, &pfx, sizeof(pfx)) == 0);
    assert(memcmp(out + sizeof(pfx), manifest, m_len) == 0);
    assert(memcmp(out + sizeof(pfx) + m_len, ct, c_len) == 0);

    uint8_t exact_out[136 + sizeof(manifest) - 1 + sizeof(ct)];
    (void)exact_out;
    assert(alrios_build_preimage(&pfx, manifest, m_len, ct, c_len, exact_out, sizeof(exact_out)) == 0);
    assert(memcmp(exact_out, out, expected_total) == 0);
}

static void test_bounds_and_overflow(void) {
    arapp_header_prefix_v1_t pfx;
    memset(&pfx, 0x22, sizeof(pfx));

    const uint8_t manifest[] = "manifest10";
    const uint8_t ct[] = "ciphertext_20_bytes";
    const size_t total = sizeof(pfx) + sizeof(manifest) + sizeof(ct);
    (void)total;

    uint8_t buf[256];
    (void)buf;

    assert(alrios_build_preimage(&pfx, manifest, sizeof(manifest), ct, sizeof(ct), buf, total - 1) == -2);
    assert(alrios_build_preimage(&pfx, manifest, sizeof(manifest), ct, sizeof(ct), buf, 0) == -2);
    assert(alrios_build_preimage(&pfx, NULL, 0, NULL, 0, buf, sizeof(pfx) - 1) == -2);

    assert(alrios_build_preimage(&pfx, manifest, (size_t)-1, ct, sizeof(ct), buf, sizeof(buf)) == -2);
    assert(alrios_build_preimage(&pfx, manifest, sizeof(manifest), ct, (size_t)-1, buf, sizeof(buf)) == -2);
    assert(alrios_build_preimage(&pfx, manifest, (size_t)-50, ct, 100, buf, sizeof(buf)) == -2);
}

static void test_fail_closed_validation(void) {
    arapp_header_prefix_v1_t pfx;
    memset(&pfx, 0x33, sizeof(pfx));
    uint8_t buf[256];
    (void)buf;
    const uint8_t valid_bytes[] = "12345678";
    const size_t valid_len = sizeof(valid_bytes);
    (void)valid_len;

    assert(alrios_build_preimage(NULL, valid_bytes, valid_len, valid_bytes, valid_len, buf, sizeof(buf)) == -1);
    assert(alrios_build_preimage(&pfx, valid_bytes, valid_len, valid_bytes, valid_len, NULL, sizeof(buf)) == -1);
    assert(alrios_build_preimage(&pfx, NULL, valid_len, valid_bytes, valid_len, buf, sizeof(buf)) == -1);
    assert(alrios_build_preimage(&pfx, valid_bytes, valid_len, NULL, valid_len, buf, sizeof(buf)) == -1);
}

static void test_buffer_overlap_protection(void) {
    uint8_t storage[512];
    arapp_header_prefix_v1_t *pfx = (arapp_header_prefix_v1_t *)storage;
    memset(pfx, 0x44, sizeof(*pfx));

    uint8_t *manifest = storage + sizeof(*pfx);
    memset(manifest, 0x55, 32);

    assert(alrios_build_preimage(pfx, NULL, 0, NULL, 0, (uint8_t *)pfx, sizeof(storage)) == -1);

    uint8_t out_buf[256];
    (void)out_buf;
    assert(alrios_build_preimage((const arapp_header_prefix_v1_t *)out_buf, manifest, 32, NULL, 0,
                                 manifest, sizeof(storage)) == -1);
}

static void test_streaming_digest_equivalence(void) {
    arapp_header_prefix_v1_t pfx;
    for (size_t i = 0; i < sizeof(pfx); i++) {
        ((uint8_t *)&pfx)[i] = (uint8_t)(0xA0 + (i % 17));
    }

    const uint8_t manifest[] = "{\"ring\":\"sovereign_trust\",\"vault\":\"enabled\"}";
    const size_t m_len = sizeof(manifest) - 1;
    (void)m_len;

    const uint8_t ct[] = "ALRIOS_ENCRYPTED_PAYLOAD_TEST_DATA_STREAM_STREAMING";
    const size_t c_len = sizeof(ct) - 1;
    (void)c_len;

    uint8_t full_preimage[512];
    (void)full_preimage;
    size_t full_len = 0;
    (void)full_len;
    assert(alrios_build_preimage_with_len(&pfx, manifest, m_len, ct, c_len,
                                         full_preimage, sizeof(full_preimage), &full_len) == 0);

    uint8_t direct_digest[SHA512_DIGEST_LEN];
    (void)direct_digest;
    assert(alrios_sha512(full_preimage, full_len, direct_digest) == ALRIOS_CRYPTO_OK);

    uint8_t stream_digest[ARAPP_SHA512_LEN];
    (void)stream_digest;
    assert(alrios_preimage_compute_digest(&pfx, manifest, m_len, ct, c_len, stream_digest) == 0);

    assert(alrios_constant_time_memcmp(direct_digest, stream_digest, SHA512_DIGEST_LEN) == 0);
}

int main(void) {
    test_basic_construction();
    test_full_stream_concatenation();
    test_bounds_and_overflow();
    test_fail_closed_validation();
    test_buffer_overlap_protection();
    test_streaming_digest_equivalence();

    printf("TASK-023 (Pre-Image Construction): PASS\n");
    return 0;
}
