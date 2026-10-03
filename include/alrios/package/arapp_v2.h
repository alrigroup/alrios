/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_PACKAGE_ARAPP_V2_H
#define ALRIOS_PACKAGE_ARAPP_V2_H

#include <stddef.h>
#include <stdint.h>
#include "alrios/pki/certificate.h"
#include <openssl/evp.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ARAPP_V2_MAGIC_STRING              "ALRIGROUP@ARAPP"
#define ARAPP_V2_MAGIC_LEN                 16U
#define ARAPP_V2_FORMAT_VERSION            0x0002U

#define ARAPP_V2_ARCH_X86_64               0x0001U
#define ARAPP_V2_ARCH_AARCH64              0x0002U
#define ARAPP_V2_ARCH_RISCV64              0x0003U

#define ARAPP_V2_FLAG_PROFILE_SOVEREIGN    (1U << 0)
#define ARAPP_V2_FLAG_PROFILE_ENTERPRISE   (1U << 1)
#define ARAPP_V2_FLAG_PROFILE_PERFORMANCE  (1U << 2)
#define ARAPP_V2_FLAG_RING_SOVEREIGN       (1U << 3)
#define ARAPP_V2_FLAG_RING_DEVMODE         (1U << 4)
#define ARAPP_V2_FLAG_ENCRYPTED_GCM        (1U << 5)
#define ARAPP_V2_FLAG_PQC_HYBRID_SIG       (1U << 6)
#define ARAPP_V2_FLAG_STRIPPED_SYMBOLS     (1U << 7)

#define ARAPP_V2_GCM_IV_LEN                12U
#define ARAPP_V2_GCM_TAG_LEN               16U
#define ARAPP_V2_SHA512_LEN                64U
#define ARAPP_V2_ED25519_SIG_LEN           64U
#define ARAPP_V2_ML_DSA_65_SIG_LEN         3309U
#define ARAPP_V2_ML_DSA_65_ALIGNED_LEN     3328U
#define ARAPP_V2_SIGNATURE_PADDING_LEN     19U

#define ARAPP_V2_HEADER_PREFIX_SIZE        144U
#define ARAPP_V2_SIGNATURES_SIZE           3392U
#define ARAPP_V2_FILE_HEADER_SIZE          3536U

#define ARAPP_V2_OK                        0
#define ARAPP_V2_ERR_INVALID_ARGUMENT     -6001
#define ARAPP_V2_ERR_TRUNCATED            -6002
#define ARAPP_V2_ERR_BAD_MAGIC            -6003
#define ARAPP_V2_ERR_VERSION              -6004
#define ARAPP_V2_ERR_ARCH                 -6005
#define ARAPP_V2_ERR_HEADER_SIZE          -6006
#define ARAPP_V2_ERR_OFFSET_OVERFLOW      -6007
#define ARAPP_V2_ERR_TRAILING_BYTES       -6008
#define ARAPP_V2_ERR_NONCANONICAL_MANIFEST -6009
#define ARAPP_V2_ERR_MANIFEST_SCHEMA      -6010
#define ARAPP_V2_ERR_TIMESTAMP_ORDER      -6011
#define ARAPP_V2_ERR_TIMESTAMP_FUTURE     -6012
#define ARAPP_V2_ERR_TIMESTAMP_EXPIRED    -6013
#define ARAPP_V2_ERR_SIGNATURE_INVALID    -6014
#define ARAPP_V2_ERR_MEMORY               -6015
#define ARAPP_V2_ERR_OVERLAP              -6016

#pragma pack(push, 1)

typedef struct arapp_v2_header_prefix {
    uint8_t  magic[ARAPP_V2_MAGIC_LEN];
    uint16_t format_version;
    uint16_t target_arch;
    uint32_t container_flags;
    uint64_t timestamp_issued;
    uint64_t timestamp_expiry;
    uint32_t header_size_bytes;
    uint32_t entitlements_json_length;
    uint32_t ciphertext_size_bytes;
    uint8_t  aes_gcm_iv[ARAPP_V2_GCM_IV_LEN];
    uint8_t  aes_gcm_tag[ARAPP_V2_GCM_TAG_LEN];
    uint8_t  cleartext_sha512[ARAPP_V2_SHA512_LEN];
} arapp_v2_header_prefix_t;

typedef struct arapp_v2_signatures {
    uint8_t sig_ed25519[ARAPP_V2_ED25519_SIG_LEN];
    uint8_t sig_ml_dsa_65[ARAPP_V2_ML_DSA_65_SIG_LEN];
    uint8_t alignment_padding[ARAPP_V2_SIGNATURE_PADDING_LEN];
} arapp_v2_signatures_t;

typedef struct arapp_v2_file_header {
    arapp_v2_header_prefix_t prefix;
    arapp_v2_signatures_t    signatures;
} arapp_v2_file_header_t;

#pragma pack(pop)

_Static_assert(sizeof(arapp_v2_header_prefix_t) == 144, "arapp_v2_header_prefix_t must be 144 bytes");
_Static_assert(sizeof(arapp_v2_signatures_t) == 3392, "arapp_v2_signatures_t must be 3392 bytes");
_Static_assert(sizeof(arapp_v2_file_header_t) == 3536, "arapp_v2_file_header_t must be 3536 bytes");
_Static_assert(offsetof(arapp_v2_file_header_t, prefix) == 0, "prefix offset must be 0");
_Static_assert(offsetof(arapp_v2_file_header_t, signatures) == 144, "signatures offset must be 144");
_Static_assert(offsetof(arapp_v2_header_prefix_t, magic) == 0, "magic offset must be 0");
_Static_assert(offsetof(arapp_v2_header_prefix_t, format_version) == 16, "format_version offset must be 16");
_Static_assert(offsetof(arapp_v2_header_prefix_t, target_arch) == 18, "target_arch offset must be 18");
_Static_assert(offsetof(arapp_v2_header_prefix_t, container_flags) == 20, "container_flags offset must be 20");
_Static_assert(offsetof(arapp_v2_header_prefix_t, timestamp_issued) == 24, "timestamp_issued offset must be 24");
_Static_assert(offsetof(arapp_v2_header_prefix_t, timestamp_expiry) == 32, "timestamp_expiry offset must be 32");
_Static_assert(offsetof(arapp_v2_header_prefix_t, header_size_bytes) == 40, "header_size_bytes offset must be 40");
_Static_assert(offsetof(arapp_v2_header_prefix_t, entitlements_json_length) == 44, "entitlements_json_length offset must be 44");
_Static_assert(offsetof(arapp_v2_header_prefix_t, ciphertext_size_bytes) == 48, "ciphertext_size_bytes offset must be 48");
_Static_assert(offsetof(arapp_v2_header_prefix_t, aes_gcm_iv) == 52, "aes_gcm_iv offset must be 52");
_Static_assert(offsetof(arapp_v2_header_prefix_t, aes_gcm_tag) == 64, "aes_gcm_tag offset must be 64");
_Static_assert(offsetof(arapp_v2_header_prefix_t, cleartext_sha512) == 80, "cleartext_sha512 offset must be 80");
_Static_assert(offsetof(arapp_v2_signatures_t, sig_ed25519) == 0, "sig_ed25519 offset must be 0");
_Static_assert(offsetof(arapp_v2_signatures_t, sig_ml_dsa_65) == 64, "sig_ml_dsa_65 offset must be 64");
_Static_assert(offsetof(arapp_v2_signatures_t, alignment_padding) == 3373, "alignment_padding offset must be 3373");

typedef struct arapp_v2_package {
    arapp_v2_header_prefix_t prefix;

    uint16_t format_version;
    uint16_t target_arch;
    uint32_t container_flags;
    uint64_t timestamp_issued;
    uint64_t timestamp_expiry;
    uint32_t header_size_bytes;
    uint32_t entitlements_json_length;
    uint32_t ciphertext_size_bytes;

    const arapp_v2_signatures_t *signatures;
    const char *manifest_json;
    size_t manifest_len;
    const uint8_t *ciphertext;
    size_t ciphertext_len;

    size_t header_offset;
    size_t manifest_offset;
    size_t ciphertext_offset;
    size_t total_size;

    uint8_t has_signatures;
    uint8_t manifest_is_canonical;
    uint8_t timestamps_valid;
} arapp_v2_package_t;

int arapp_v2_read_header(const uint8_t *buffer, size_t size, arapp_v2_package_t *out_pkg);

int arapp_v2_validate_offsets(const uint8_t *buffer, size_t size, arapp_v2_package_t *out_pkg);

int arapp_v2_validate_anti_replay(const arapp_v2_package_t *pkg, uint64_t current_time, uint64_t max_skew_sec);

int arapp_v2_validate_manifest(const arapp_v2_package_t *pkg);

int arapp_v2_read(const uint8_t *buffer, size_t size, arapp_v2_package_t *out_pkg);

int arapp_v2_read_and_validate(const uint8_t *buffer, size_t size, uint64_t current_time, arapp_v2_package_t *out_pkg);

int arapp_v2_compute_digest(const arapp_v2_package_t *pkg, uint8_t out_digest[ARAPP_V2_SHA512_LEN]);

int arapp_v2_verify_signatures(const arapp_v2_package_t *pkg, const uint8_t *ed25519_pubkey, const uint8_t *ml_dsa_65_pubkey);

int arapp_v2_build(const arapp_v2_header_prefix_t *prefix,
                   const arapp_v2_signatures_t *signatures,
                   const char *manifest_json, size_t manifest_len,
                   const uint8_t *ciphertext, size_t ciphertext_len,
                   uint8_t *out_buffer, size_t out_capacity,
                   size_t *out_written);

int arapp_v2_sign(arapp_v2_package_t *pkg,
                  EVP_PKEY *ed25519_privkey,
                  const uint8_t *ml_dsa_65_privkey,
                  arapp_v2_signatures_t *out_sigs);

int arapp_v2_verify_package_with_chain(const uint8_t *buffer, size_t size,
                                       const alrios_certificate_t *leaf_cert,
                                       const alrios_certificate_t *inter_cert,
                                       const alrios_trust_store_t *store,
                                       const uint8_t *ml_dsa_65_pubkey,
                                       uint64_t current_time,
                                       arapp_v2_package_t *out_pkg);

#ifdef __cplusplus
}
#endif

#endif
