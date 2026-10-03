/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_PKI_CERTIFICATE_H
#define ALRIOS_PKI_CERTIFICATE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALRIOS_CERTIFICATE_VERSION_1             1U
#define ALRIOS_CERTIFICATE_HEADER_SIZE_V1       92U
#define ALRIOS_CERTIFICATE_TBS_DOMAIN_SIZE       18U
#define ALRIOS_CERTIFICATE_SUBJECT_MAX           64U
#define ALRIOS_CERTIFICATE_SERIAL_SIZE           16U
#define ALRIOS_CERTIFICATE_KEY_ID_SIZE           32U
#define ALRIOS_CERTIFICATE_ED25519_KEY_SIZE       32U
#define ALRIOS_CERTIFICATE_ML_DSA_65_KEY_SIZE   1952U
#define ALRIOS_CERTIFICATE_HYBRID_KEY_SIZE      1984U
#define ALRIOS_CERTIFICATE_ED25519_SIG_SIZE       64U
#define ALRIOS_CERTIFICATE_ML_DSA_65_SIG_SIZE   3309U
#define ALRIOS_CERTIFICATE_HYBRID_SIG_SIZE      3373U
#define ALRIOS_CERTIFICATE_WIRE_MAX             5513U
#define ALRIOS_CERTIFICATE_TBS_MAX              2074U
#define ALRIOS_TRUST_STORE_MAX_ANCHORS             8U

#define ALRIOS_CERTIFICATE_OK                    0
#define ALRIOS_CERTIFICATE_ERR_INVALID_ARGUMENT -4001
#define ALRIOS_CERTIFICATE_ERR_TRUNCATED        -4002
#define ALRIOS_CERTIFICATE_ERR_BAD_MAGIC        -4003
#define ALRIOS_CERTIFICATE_ERR_VERSION          -4004
#define ALRIOS_CERTIFICATE_ERR_LENGTH           -4005
#define ALRIOS_CERTIFICATE_ERR_ALGORITHM        -4006
#define ALRIOS_CERTIFICATE_ERR_FIELD            -4007
#define ALRIOS_CERTIFICATE_ERR_NONCANONICAL     -4008
#define ALRIOS_CERTIFICATE_ERR_CAPACITY         -4009
#define ALRIOS_CERTIFICATE_ERR_NOT_FOUND        -4010
#define ALRIOS_CERTIFICATE_ERR_DUPLICATE        -4011
#define ALRIOS_CERTIFICATE_ERR_SEALED           -4012
#define ALRIOS_CERTIFICATE_ERR_STATE            -4013
#define ALRIOS_CERTIFICATE_ERR_OVERLAP          -4014

typedef enum alrios_certificate_role {
    ALRIOS_CERTIFICATE_ROLE_ROOT = 1,
    ALRIOS_CERTIFICATE_ROLE_INTERMEDIATE = 2,
    ALRIOS_CERTIFICATE_ROLE_LEAF = 3
} alrios_certificate_role_t;

typedef enum alrios_public_key_algorithm {
    ALRIOS_PUBLIC_KEY_ALGORITHM_ED25519 = 1,
    ALRIOS_PUBLIC_KEY_ALGORITHM_ML_DSA_65 = 2,
    ALRIOS_PUBLIC_KEY_ALGORITHM_HYBRID_ED25519_ML_DSA_65 = 3
} alrios_public_key_algorithm_t;

typedef enum alrios_signature_algorithm {
    ALRIOS_SIGNATURE_ALGORITHM_ED25519 = 1,
    ALRIOS_SIGNATURE_ALGORITHM_ML_DSA_65 = 2,
    ALRIOS_SIGNATURE_ALGORITHM_HYBRID_ED25519_ML_DSA_65 = 3
} alrios_signature_algorithm_t;

typedef struct alrios_certificate {
    uint16_t version;
    uint8_t role;
    uint8_t public_key_algorithm;
    uint8_t signature_algorithm;
    uint8_t serial[ALRIOS_CERTIFICATE_SERIAL_SIZE];
    uint8_t issuer_key_id[ALRIOS_CERTIFICATE_KEY_ID_SIZE];
    uint64_t not_before;
    uint64_t not_after;
    size_t subject_len;
    uint8_t subject[ALRIOS_CERTIFICATE_SUBJECT_MAX];
    size_t public_key_len;
    uint8_t public_key[ALRIOS_CERTIFICATE_HYBRID_KEY_SIZE];
    size_t signature_len;
    uint8_t signature[ALRIOS_CERTIFICATE_HYBRID_SIG_SIZE];
} alrios_certificate_t;

/* Source-level API object only; never persist or transmit this native layout. */
typedef struct alrios_trust_store {
    alrios_certificate_t anchors[ALRIOS_TRUST_STORE_MAX_ANCHORS];
    size_t anchor_count;
    uint8_t sealed;
} alrios_trust_store_t;

/*
 * V1 wire encoding is canonical and entirely big-endian. Decoding consumes the
 * complete input: trailing bytes, reserved bits/bytes and unknown algorithms
 * are rejected. Output objects are zeroed on every failure.
 */
int alrios_certificate_decode(const uint8_t *wire,
                              size_t wire_len,
                              alrios_certificate_t *out_certificate);

int alrios_certificate_encode(const alrios_certificate_t *certificate,
                              uint8_t *out_wire,
                              size_t out_capacity,
                              size_t *out_written);

/*
 * Canonical signed bytes:
 * "ALRIOS-CERT-TBS-V1" || V1 header || subject || public key.
 * The header retains the final certificate total length and signature length,
 * binding both to the signature while excluding only the signature payload.
 */
int alrios_certificate_encode_tbs(const alrios_certificate_t *certificate,
                                  uint8_t *out_tbs,
                                  size_t out_capacity,
                                  size_t *out_written);

int alrios_certificate_public_key_id(const alrios_certificate_t *certificate,
                                     uint8_t out_key_id[ALRIOS_CERTIFICATE_KEY_ID_SIZE]);

void alrios_trust_store_init(alrios_trust_store_t *store);
int alrios_trust_store_add_anchor(alrios_trust_store_t *store,
                                  const alrios_certificate_t *certificate);
int alrios_trust_store_seal(alrios_trust_store_t *store);
int alrios_trust_store_lookup(const alrios_trust_store_t *store,
                              const uint8_t key_id[ALRIOS_CERTIFICATE_KEY_ID_SIZE],
                              const alrios_certificate_t **out_certificate);
void alrios_trust_store_clear(alrios_trust_store_t *store);

#ifdef __cplusplus
}
#endif

#endif /* ALRIOS_PKI_CERTIFICATE_H */
