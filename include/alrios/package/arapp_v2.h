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

#ifdef __cplusplus
extern "C" {
#endif

#define ALRIOS_ARAPP_V2_MAGIC "ALRIGROUP@ARAPP"
#define ALRIOS_ARAPP_V2_MAGIC_SIZE 16U
#define ALRIOS_ARAPP_V2_VERSION 2U

#define ALRIOS_ARAPP_V2_PREFIX_SIZE 144U
#define ALRIOS_ARAPP_V2_SIGNATURES_SIZE 3392U
#define ALRIOS_ARAPP_V2_HEADER_SIZE \
    (ALRIOS_ARAPP_V2_PREFIX_SIZE + ALRIOS_ARAPP_V2_SIGNATURES_SIZE)
#define ALRIOS_ARAPP_V2_MAX_MANIFEST_SIZE (1024U * 1024U)
#define ALRIOS_ARAPP_V2_MAX_CLOCK_SKEW_SECONDS 300U

#define ALRIOS_ARAPP_V2_ARCH_X86_64  0x0001U
#define ALRIOS_ARAPP_V2_ARCH_AARCH64 0x0002U
#define ALRIOS_ARAPP_V2_ARCH_RISCV64 0x0003U

#define ALRIOS_ARAPP_V2_FLAG_PROFILE_SOVEREIGN   (1U << 0)
#define ALRIOS_ARAPP_V2_FLAG_PROFILE_ENTERPRISE  (1U << 1)
#define ALRIOS_ARAPP_V2_FLAG_PROFILE_PERFORMANCE (1U << 2)
#define ALRIOS_ARAPP_V2_FLAG_RING_SOVEREIGN      (1U << 3)
#define ALRIOS_ARAPP_V2_FLAG_RING_DEVMODE        (1U << 4)
#define ALRIOS_ARAPP_V2_FLAG_ENCRYPTED_GCM       (1U << 5)
#define ALRIOS_ARAPP_V2_FLAG_PQC_HYBRID_SIG      (1U << 6)
#define ALRIOS_ARAPP_V2_FLAG_STRIPPED_SYMBOLS    (1U << 7)
#define ALRIOS_ARAPP_V2_FLAGS_MASK               0x000000ffU

#define ALRIOS_ARAPP_V2_GCM_IV_SIZE 12U
#define ALRIOS_ARAPP_V2_GCM_TAG_SIZE 16U
#define ALRIOS_ARAPP_V2_SHA512_SIZE 64U
#define ALRIOS_ARAPP_V2_ED25519_SIGNATURE_SIZE 64U
#define ALRIOS_ARAPP_V2_ML_DSA_65_SIGNATURE_SIZE 3309U
#define ALRIOS_ARAPP_V2_ML_DSA_65_ALIGNED_SIZE 3328U
#define ALRIOS_ARAPP_V2_SIGNATURE_PADDING_SIZE \
    (ALRIOS_ARAPP_V2_ML_DSA_65_ALIGNED_SIZE - \
     ALRIOS_ARAPP_V2_ML_DSA_65_SIGNATURE_SIZE)

#define ALRIOS_ARAPP_V2_OK                  0
#define ALRIOS_ARAPP_V2_ERR_ARGUMENT       -3001
#define ALRIOS_ARAPP_V2_ERR_TRUNCATED      -3002
#define ALRIOS_ARAPP_V2_ERR_MAGIC          -3003
#define ALRIOS_ARAPP_V2_ERR_VERSION        -3004
#define ALRIOS_ARAPP_V2_ERR_ARCH           -3005
#define ALRIOS_ARAPP_V2_ERR_FLAGS          -3006
#define ALRIOS_ARAPP_V2_ERR_LAYOUT         -3007
#define ALRIOS_ARAPP_V2_ERR_MANIFEST       -3008
#define ALRIOS_ARAPP_V2_ERR_TIME           -3009
#define ALRIOS_ARAPP_V2_ERR_PADDING        -3010
#define ALRIOS_ARAPP_V2_ERR_MEMORY         -3011

#define ALRIOS_ARAPP_V2_SIG_OK               0
#define ALRIOS_ARAPP_V2_SIG_ERR_ARGUMENT    -3101
#define ALRIOS_ARAPP_V2_SIG_ERR_KEY         -3102
#define ALRIOS_ARAPP_V2_SIG_ERR_SIGN        -3103
#define ALRIOS_ARAPP_V2_SIG_ERR_VERIFY      -3104
#define ALRIOS_ARAPP_V2_SIG_ERR_POLICY      -3105
#define ALRIOS_ARAPP_V2_SIG_ERR_TAMPERED    -3106
#define ALRIOS_ARAPP_V2_SIG_ERR_UNAVAILABLE -3107

/*
 * V2 wire integers are big-endian. The view borrows data from the input buffer,
 * which must remain alive and unchanged while any returned pointer is in use.
 */
typedef struct alrios_arapp_v2_view {
    uint16_t target_arch;
    uint32_t flags;
    uint64_t issued_at;
    uint64_t expires_at;
    const uint8_t *prefix;
    size_t prefix_len;
    size_t header_len;
    const uint8_t *ed25519_signature;
    const uint8_t *ml_dsa_65_signature;
    const uint8_t *manifest;
    size_t manifest_len;
    const uint8_t *ciphertext;
    size_t ciphertext_len;
    const uint8_t *gcm_iv;
    const uint8_t *gcm_tag;
    const uint8_t *cleartext_sha512;
} alrios_arapp_v2_view_t;

/*
 * Performs structural, policy, canonical-manifest and timestamp validation.
 * out_view is zeroed on every failure when it is non-NULL.
 */
int alrios_arapp_v2_validate(
    const uint8_t *data,
    size_t data_len,
    uint64_t now,
    uint64_t max_clock_skew,
    alrios_arapp_v2_view_t *out_view
);

struct evp_pkey_st;
typedef struct evp_pkey_st EVP_PKEY;

int alrios_arapp_v2_sign_ed25519(
    uint8_t *package,
    size_t package_len,
    EVP_PKEY *ed25519_key
);

int alrios_arapp_v2_verify_ed25519(
    const uint8_t *package,
    size_t package_len,
    EVP_PKEY *ed25519_pubkey
);

int alrios_arapp_v2_sign_ml_dsa_65(
    uint8_t *package,
    size_t package_len,
    EVP_PKEY *ml_dsa_key
);

int alrios_arapp_v2_verify_ml_dsa_65(
    const uint8_t *package,
    size_t package_len,
    EVP_PKEY *ml_dsa_pubkey
);

int alrios_arapp_v2_verify_hybrid(
    const uint8_t *package,
    size_t package_len,
    EVP_PKEY *ed25519_pubkey,
    EVP_PKEY *ml_dsa_pubkey,
    int require_pqc
);

#ifdef __cplusplus
}
#endif

#endif /* ALRIOS_PACKAGE_ARAPP_V2_H */
