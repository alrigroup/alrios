/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */

#ifndef ALRIOS_ARAPP_FORMAT_H
#define ALRIOS_ARAPP_FORMAT_H

#include <stdint.h>
#include <stddef.h>

#define ARAPP_MAGIC_0 0x41 /* 'A' */
#define ARAPP_MAGIC_1 0x4C /* 'L' */
#define ARAPP_MAGIC_2 0x52 /* 'R' */
#define ARAPP_MAGIC_3 0x49 /* 'I' */
#define ARAPP_MAGIC_4 0x4F /* 'O' */
#define ARAPP_MAGIC_5 0x53 /* 'S' */

#define ARAPP_FLAG_PRODUCTION    (1U << 0)
#define ARAPP_FLAG_ENCRYPTED     (1U << 1)
#define ARAPP_FLAG_PQC_ENFORCED  (1U << 2)
#define ARAPP_FLAG_WASM_SANDBOX  (1U << 3)
#define ARAPP_FLAG_MEMLOCK_ONLY  (1U << 4)

#define ARAPP_ED25519_SIG_LEN       64
#define ARAPP_ML_DSA_65_SIG_LEN     3309
#define ARAPP_ML_DSA_65_PADDED_LEN  3328 /* 3309 + 19 pad bytes */
#define ARAPP_GCM_IV_LEN            12
#define ARAPP_GCM_TAG_LEN           16
#define ARAPP_SHA512_LEN            64

#pragma pack(push, 1)

typedef struct arapp_header_prefix_v1 {
    uint8_t  magic[6];
    uint16_t version;
    uint16_t target_arch;
    uint16_t reserved_alignment;
    uint32_t flags;
    uint64_t timestamp_issued;
    uint64_t timestamp_expiry;
    uint32_t header_size_bytes;
    uint32_t entitlements_json_length;
    uint32_t ciphertext_size_bytes;
    uint8_t  aes_gcm_iv[ARAPP_GCM_IV_LEN];
    uint8_t  aes_gcm_tag[ARAPP_GCM_TAG_LEN];
    uint8_t  cleartext_sha512[ARAPP_SHA512_LEN];
} arapp_header_prefix_v1_t;

typedef struct arapp_signatures_v1 {
    uint8_t  sig_ed25519[ARAPP_ED25519_SIG_LEN];
    uint8_t  sig_ml_dsa_65[ARAPP_ML_DSA_65_SIG_LEN];
    uint8_t  padding[ARAPP_ML_DSA_65_PADDED_LEN - ARAPP_ML_DSA_65_SIG_LEN];
} arapp_signatures_v1_t;

typedef struct arapp_header_v1 {
    arapp_header_prefix_v1_t prefix;
    arapp_signatures_v1_t    signatures;
} arapp_header_v1_t;

#pragma pack(pop)

_Static_assert(sizeof(arapp_header_prefix_v1_t) == 136, "arapp_header_prefix_v1_t must be exactly 136 bytes");
_Static_assert(sizeof(arapp_signatures_v1_t) == 3392, "arapp_signatures_v1_t must be exactly 3392 bytes");
_Static_assert(sizeof(arapp_header_v1_t) == 3528, "arapp_header_v1_t layout invariant violated (expected 3528 bytes)");
_Static_assert(offsetof(arapp_header_v1_t, prefix) == 0, "arapp_header_v1_t prefix offset must be 0");
_Static_assert(offsetof(arapp_header_v1_t, signatures) == 136, "arapp_header_v1_t signatures offset must be 136");
_Static_assert(offsetof(arapp_header_prefix_v1_t, magic) == 0, "magic offset must be 0");
_Static_assert(offsetof(arapp_header_prefix_v1_t, version) == 6, "version offset must be 6");
_Static_assert(offsetof(arapp_header_prefix_v1_t, target_arch) == 8, "target_arch offset must be 8");
_Static_assert(offsetof(arapp_header_prefix_v1_t, reserved_alignment) == 10, "reserved_alignment offset must be 10");
_Static_assert(offsetof(arapp_header_prefix_v1_t, flags) == 12, "flags offset must be 12");
_Static_assert(offsetof(arapp_header_prefix_v1_t, timestamp_issued) == 16, "timestamp_issued offset must be 16");
_Static_assert(offsetof(arapp_header_prefix_v1_t, timestamp_expiry) == 24, "timestamp_expiry offset must be 24");
_Static_assert(offsetof(arapp_header_prefix_v1_t, header_size_bytes) == 32, "header_size_bytes offset must be 32");
_Static_assert(offsetof(arapp_header_prefix_v1_t, entitlements_json_length) == 36, "entitlements_json_length offset must be 36");
_Static_assert(offsetof(arapp_header_prefix_v1_t, ciphertext_size_bytes) == 40, "ciphertext_size_bytes offset must be 40");
_Static_assert(offsetof(arapp_header_prefix_v1_t, aes_gcm_iv) == 44, "aes_gcm_iv offset must be 44");
_Static_assert(offsetof(arapp_header_prefix_v1_t, aes_gcm_tag) == 56, "aes_gcm_tag offset must be 56");
_Static_assert(offsetof(arapp_header_prefix_v1_t, cleartext_sha512) == 72, "cleartext_sha512 offset must be 72");
_Static_assert(offsetof(arapp_signatures_v1_t, sig_ed25519) == 0, "sig_ed25519 offset must be 0");
_Static_assert(offsetof(arapp_signatures_v1_t, sig_ml_dsa_65) == 64, "sig_ml_dsa_65 offset must be 64");
_Static_assert(offsetof(arapp_signatures_v1_t, padding) == 3373, "padding offset must be 3373");

/**
 * @brief Constructs the canonical pre-image signature stream M:
 *        M = Header Prefix (136 bytes) || UTF-8 Entitlements || Raw Ciphertext
 *
 * @param pfx          Pointer to valid 136-byte arapp header prefix
 * @param manifest     Pointer to canonical manifest bytes (may be NULL iff m_len == 0)
 * @param m_len        Length in bytes of manifest
 * @param ct           Pointer to raw ciphertext bytes (may be NULL iff c_len == 0)
 * @param c_len        Length in bytes of ciphertext
 * @param out_preimage Pointer to output destination buffer
 * @param max_out      Maximum capacity of out_preimage buffer
 * @return 0 on success, -1 on invalid argument/overlap, -2 on buffer overflow/insufficient capacity
 */
int alrios_build_preimage(
    const arapp_header_prefix_v1_t *pfx,
    const uint8_t *manifest, size_t m_len,
    const uint8_t *ct, size_t c_len,
    uint8_t *out_preimage, size_t max_out
);

/**
 * @brief Extended variant of alrios_build_preimage reporting the exact total length written.
 */
int alrios_build_preimage_with_len(
    const arapp_header_prefix_v1_t *pfx,
    const uint8_t *manifest, size_t m_len,
    const uint8_t *ct, size_t c_len,
    uint8_t *out_preimage, size_t max_out,
    size_t *out_len
);

/**
 * @brief Computes SHA-512 digest H = SHA-512(M) directly via streaming without requiring
 *        full in-memory concatenation of the pre-image buffer.
 */
int alrios_preimage_compute_digest(
    const arapp_header_prefix_v1_t *pfx,
    const uint8_t *manifest, size_t m_len,
    const uint8_t *ct, size_t c_len,
    uint8_t out_digest[ARAPP_SHA512_LEN]
);

#endif /* ALRIOS_ARAPP_FORMAT_H */
