/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_PKI_ISSUER_H
#define ALRIOS_PKI_ISSUER_H

#include "alrios/pki/certificate.h"
#include <openssl/evp.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALRIOS_ISSUER_OK                       0
#define ALRIOS_ISSUER_ERR_INVALID_ARGUMENT    -5001
#define ALRIOS_ISSUER_ERR_KEYGEN_FAILED       -5002
#define ALRIOS_ISSUER_ERR_SIGN_FAILED         -5003
#define ALRIOS_ISSUER_ERR_ENCODE_FAILED       -5004
#define ALRIOS_ISSUER_ERR_VERIFY_FAILED       -5005
#define ALRIOS_ISSUER_ERR_IO_FAILED           -5006
#define ALRIOS_ISSUER_ERR_EXPIRED             -5007
#define ALRIOS_ISSUER_ERR_CHAIN_INVALID       -5008

/*
 * Issues a self-signed Root CA certificate with Ed25519.
 * Private key is populated into out_pkey (must be freed by caller via EVP_PKEY_free).
 */
int alrios_pki_issue_root(const char *subject,
                          uint64_t valid_days,
                          EVP_PKEY **out_pkey,
                          alrios_certificate_t *out_cert,
                          uint8_t *out_wire,
                          size_t out_capacity,
                          size_t *out_wire_len);

/*
 * Issues an Intermediate CA certificate signed by the Root CA.
 * Generates an Ed25519 keypair for the intermediate.
 */
int alrios_pki_issue_intermediate(const char *subject,
                                  uint64_t valid_days,
                                  const alrios_certificate_t *root_cert,
                                  EVP_PKEY *root_pkey,
                                  EVP_PKEY **out_inter_pkey,
                                  alrios_certificate_t *out_cert,
                                  uint8_t *out_wire,
                                  size_t out_capacity,
                                  size_t *out_wire_len);

/*
 * Issues a Leaf Application certificate signed by the Intermediate CA.
 */
int alrios_pki_issue_leaf(const char *subject,
                          uint64_t valid_days,
                          const alrios_certificate_t *inter_cert,
                          EVP_PKEY *inter_pkey,
                          EVP_PKEY **out_leaf_pkey,
                          alrios_certificate_t *out_cert,
                          uint8_t *out_wire,
                          size_t out_capacity,
                          size_t *out_wire_len);

/*
 * Verifies a certificate chain against an immutable trust store:
 * Leaf -> Intermediate -> Root (must be in store).
 */
int alrios_pki_verify_chain(const alrios_trust_store_t *store,
                            const alrios_certificate_t *leaf_cert,
                            const alrios_certificate_t *inter_cert,
                            uint64_t current_time);

#ifdef __cplusplus
}
#endif

#endif /* ALRIOS_PKI_ISSUER_H */
