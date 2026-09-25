/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */

#include "alrios/arapp_format.h"
#include "alrios/crypto_verify.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_basic_construction(void) {
    arapp_header_prefix_v1_t pfx;
    memset(&pfx, 0x11, sizeof(pfx));

    uint8_t buf[256];
    memset(buf, 0xAA, sizeof(buf));

    assert(alrios_build_preimage(&pfx, NULL, 0, NULL, 0, buf, sizeof(buf)) == 0);
    assert(memcmp(buf, &pfx, sizeof(pfx)) == 0);
    /* Remaining bytes in buffer untouched */
    assert(buf[sizeof(pfx)] == 0xAA);
}

static void test_full_stream_concatenation(void) {
    arapp_header_prefix_v1_t pfx;
    for (size_t i = 0; i < sizeof(pfx); i++) {
        ((uint8_t *)&pfx)[i] = (uint8_t)(i & 0xFF);
    }

    const uint8_t manifest[] = "{\"app_id\":\"alrios.pki.test\",\"version\":\"1.0.0\"}";
    const size_t m_len = sizeof(manifest) - 1;

    const uint8_t ct[64] = {
        0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
        0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
        0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67
    };
    const size_t c_len = sizeof(ct);

    const size_t expected_total = sizeof(pfx) + m_len + c_len;
    uint8_t out[512];
    size_t out_len = 0;

    assert(alrios_build_preimage_with_len(&pfx, manifest, m_len, ct, c_len, out, sizeof(out), &out_len) == 0);
    assert(out_len == expected_total);

    /* Verify Prefix section */
    assert(memcmp(out, &pfx, sizeof(pfx)) == 0);

    /* Verify Manifest section */
    assert(memcmp(out + sizeof(pfx), manifest, m_len) == 0);

    /* Verify Ciphertext section */
    assert(memcmp(out + sizeof(pfx) + m_len, ct, c_len) == 0);

    /* Test exact capacity match */
    uint8_t exact_out[136 + sizeof(manifest) - 1 + sizeof(ct)];
    assert(alrios_build_preimage(&pfx, manifest, m_len, ct, c_len, exact_out, sizeof(exact_out)) == 0);
    assert(memcmp(exact_out, out, expected_total) == 0);
}

static void test_bounds_and_overflow(void) {
    arapp_header_prefix_v1_t pfx;
    memset(&pfx, 0x22, sizeof(pfx));

    const uint8_t manifest[] = "manifest10";
    const uint8_t ct[] = "ciphertext_20_bytes";
    const size_t total = sizeof(pfx) + sizeof(manifest) + sizeof(ct);

    uint8_t buf[256];

    /* Buffer capacity 1 byte smaller than needed */
    assert(alrios_build_preimage(&pfx, manifest, sizeof(manifest), ct, sizeof(ct), buf, total - 1) == -2);

    /* Buffer capacity 0 */
    assert(alrios_build_preimage(&pfx, manifest, sizeof(manifest), ct, sizeof(ct), buf, 0) == -2);

    /* Buffer smaller than prefix alone */
    assert(alrios_build_preimage(&pfx, NULL, 0, NULL, 0, buf, sizeof(pfx) - 1) == -2);

    /* Arithmetic overflow checks */
    assert(alrios_build_preimage(&pfx, manifest, (size_t)-1, ct, sizeof(ct), buf, sizeof(buf)) == -2);
    assert(alrios_build_preimage(&pfx, manifest, sizeof(manifest), ct, (size_t)-1, buf, sizeof(buf)) == -2);
    assert(alrios_build_preimage(&pfx, manifest, (size_t)-50, ct, 100, buf, sizeof(buf)) == -2);
}

static void test_fail_closed_validation(void) {
    arapp_header_prefix_v1_t pfx;
    memset(&pfx, 0x33, sizeof(pfx));
    uint8_t buf[256];
    const uint8_t valid_bytes[] = "12345678";
    const size_t valid_len = sizeof(valid_bytes);

    /* Null prefix */
    assert(alrios_build_preimage(NULL, valid_bytes, valid_len, valid_bytes, valid_len, buf, sizeof(buf)) == -1);

    /* Null output */
    assert(alrios_build_preimage(&pfx, valid_bytes, valid_len, valid_bytes, valid_len, NULL, sizeof(buf)) == -1);

    /* Manifest pointer NULL with non-zero length */
    assert(alrios_build_preimage(&pfx, NULL, valid_len, valid_bytes, valid_len, buf, sizeof(buf)) == -1);

    /* Ciphertext pointer NULL with non-zero length */
    assert(alrios_build_preimage(&pfx, valid_bytes, valid_len, NULL, valid_len, buf, sizeof(buf)) == -1);
}

static void test_buffer_overlap_protection(void) {
    uint8_t storage[512];
    arapp_header_prefix_v1_t *pfx = (arapp_header_prefix_v1_t *)storage;
    memset(pfx, 0x44, sizeof(*pfx));

    uint8_t *manifest = storage + sizeof(*pfx);
    memset(manifest, 0x55, 32);

    /* Passing output buffer that overlaps with pfx */
    assert(alrios_build_preimage(pfx, NULL, 0, NULL, 0, (uint8_t *)pfx, sizeof(storage)) == -1);

    /* Passing output buffer that overlaps with manifest */
    uint8_t out_buf[256];
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

    const uint8_t ct[] = "ALRIOS_ENCRYPTED_PAYLOAD_TEST_DATA_STREAM_STREAMING";
    const size_t c_len = sizeof(ct) - 1;

    /* Build full preimage buffer */
    uint8_t full_preimage[512];
    size_t full_len = 0;
    assert(alrios_build_preimage_with_len(&pfx, manifest, m_len, ct, c_len,
                                         full_preimage, sizeof(full_preimage), &full_len) == 0);

    /* Compute standard SHA-512 over concatenated preimage */
    uint8_t direct_digest[SHA512_DIGEST_LEN];
    assert(alrios_sha512(full_preimage, full_len, direct_digest) == ALRIOS_CRYPTO_OK);

    /* Compute streaming SHA-512 via alrios_preimage_compute_digest */
    uint8_t stream_digest[ARAPP_SHA512_LEN];
    assert(alrios_preimage_compute_digest(&pfx, manifest, m_len, ct, c_len, stream_digest) == 0);

    /* Verify exact bitwise equivalence between streaming and concatenated digests */
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
