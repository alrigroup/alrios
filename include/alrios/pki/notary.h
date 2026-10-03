/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_PKI_NOTARY_H
#define ALRIOS_PKI_NOTARY_H

#include "alrios/pki/certificate.h"
#include "alrios/crypto_verify.h"
#include <openssl/evp.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALRIOS_NOTARY_MAGIC_0              'A'
#define ALRIOS_NOTARY_MAGIC_1              'R'
#define ALRIOS_NOTARY_MAGIC_2              'N'
#define ALRIOS_NOTARY_MAGIC_3              'O'
#define ALRIOS_NOTARY_MAGIC_4              'T'
#define ALRIOS_NOTARY_MAGIC_5              'A'
#define ALRIOS_NOTARY_MAGIC_6              'R'
#define ALRIOS_NOTARY_MAGIC_7              'Y'

#define ALRIOS_NOTARY_VERSION_1            1U
#define ALRIOS_NOTARY_HEADER_SIZE          132U
#define ALRIOS_NOTARY_TBS_DOMAIN_SIZE       20U
#define ALRIOS_NOTARY_MAX_PAYLOAD_SIZE     (256U * 1024U * 1024U) /* 256MB bounds */
#define ALRIOS_NOTARY_WIRE_MAX             (ALRIOS_NOTARY_HEADER_SIZE + ALRIOS_CERTIFICATE_HYBRID_SIG_SIZE)

#define ALRIOS_NOTARY_OK                   0
#define ALRIOS_NOTARY_ERR_INVALID_ARGUMENT -6001
#define ALRIOS_NOTARY_ERR_POLICY_VIOLATION -6002
#define ALRIOS_NOTARY_ERR_KEY_INVALID      -6003
#define ALRIOS_NOTARY_ERR_SIGN_FAILED      -6004
#define ALRIOS_NOTARY_ERR_VERIFY_FAILED    -6005
#define ALRIOS_NOTARY_ERR_PAYLOAD_TAMPERED -6006
#define ALRIOS_NOTARY_ERR_TRUNCATED        -6007
#define ALRIOS_NOTARY_ERR_BAD_MAGIC        -6008
#define ALRIOS_NOTARY_ERR_NONCANONICAL     -6009
#define ALRIOS_NOTARY_ERR_COMPILER_FORBIDDEN -6010
#define ALRIOS_NOTARY_ERR_IO_FAILED        -6011
#define ALRIOS_NOTARY_ERR_CAPACITY         -6012

/* Algorithm Policy Flags */
#define ALRIOS_NOTARY_POLICY_ALLOW_ED25519     (1U << 0)
#define ALRIOS_NOTARY_POLICY_REQUIRE_PQC       (1U << 1)
#define ALRIOS_NOTARY_POLICY_REQUIRE_CERT_BIND (1U << 2)
#define ALRIOS_NOTARY_POLICY_DEFAULT           (ALRIOS_NOTARY_POLICY_ALLOW_ED25519)
#define ALRIOS_NOTARY_POLICY_STRICT            (ALRIOS_NOTARY_POLICY_ALLOW_ED25519 | ALRIOS_NOTARY_POLICY_REQUIRE_CERT_BIND)

typedef struct alrios_notary_policy {
    uint32_t flags;
    uint8_t  allowed_signature_algorithm;
    uint8_t  reserved[3];
    uint64_t max_validity_period_sec;
} alrios_notary_policy_t;

typedef struct alrios_notary_signature {
    uint16_t version;
    uint8_t  algorithm;
    uint8_t  reserved;
    uint64_t payload_len;
    uint64_t timestamp;
    uint32_t policy_flags;
    uint8_t  key_id[ALRIOS_CERTIFICATE_KEY_ID_SIZE];
    uint8_t  digest[SHA512_DIGEST_LEN];
    size_t   signature_len;
    uint8_t  signature[ALRIOS_CERTIFICATE_HYBRID_SIG_SIZE];
} alrios_notary_signature_t;

int alrios_notary_policy_init(alrios_notary_policy_t *policy, uint32_t flags);
int alrios_notary_policy_validate(const alrios_notary_policy_t *policy, uint8_t algorithm);

int alrios_notary_enable_noexec_confinement(void);

int alrios_notary_sign_payload(const alrios_notary_policy_t *policy,
                               const uint8_t *payload,
                               size_t payload_len,
                               EVP_PKEY *privkey,
                               alrios_notary_signature_t *out_sig);

int alrios_notary_verify_payload(const alrios_notary_policy_t *policy,
                                 const uint8_t *payload,
                                 size_t payload_len,
                                 const alrios_notary_signature_t *sig,
                                 const uint8_t pubkey[ALRIOS_CERTIFICATE_ED25519_KEY_SIZE]);

int alrios_notary_verify_with_certificate(const alrios_notary_policy_t *policy,
                                          const uint8_t *payload,
                                          size_t payload_len,
                                          const alrios_notary_signature_t *sig,
                                          const alrios_certificate_t *leaf_cert,
                                          const alrios_certificate_t *inter_cert,
                                          const alrios_trust_store_t *store,
                                          uint64_t current_time);

int alrios_notary_signature_encode(const alrios_notary_signature_t *sig,
                                   uint8_t *out_wire,
                                   size_t out_capacity,
                                   size_t *out_written);

int alrios_notary_signature_decode(const uint8_t *wire,
                                   size_t wire_len,
                                   alrios_notary_signature_t *out_sig);

int alrios_notary_load_private_key_file(const char *path, EVP_PKEY **out_pkey);
int alrios_notary_load_public_key_file(const char *path, uint8_t out_pubkey[ALRIOS_CERTIFICATE_ED25519_KEY_SIZE]);
int alrios_notary_save_private_key_raw(const char *path, EVP_PKEY *pkey);
int alrios_notary_save_public_key_raw(const char *path, const uint8_t pubkey[ALRIOS_CERTIFICATE_ED25519_KEY_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* ALRIOS_PKI_NOTARY_H */
