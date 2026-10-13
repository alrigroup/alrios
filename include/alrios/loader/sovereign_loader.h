/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_SOVEREIGN_LOADER_H
#define ALRIOS_SOVEREIGN_LOADER_H

#include "alrios/pki/certificate.h"

#include <openssl/evp.h>
#include <stddef.h>
#include <stdint.h>

#define ALRIOS_SOVEREIGN_OK                  0
#define ALRIOS_SOVEREIGN_ERR_ARGUMENT       -3401
#define ALRIOS_SOVEREIGN_ERR_VERIFY         -3402
#define ALRIOS_SOVEREIGN_ERR_DECRYPT        -3403
#define ALRIOS_SOVEREIGN_ERR_MEMFD          -3404
#define ALRIOS_SOVEREIGN_ERR_SEAL           -3405
#define ALRIOS_SOVEREIGN_ERR_IO             -3406
#define ALRIOS_SOVEREIGN_ERR_PLATFORM       -3407
#define ALRIOS_SOVEREIGN_ERR_POLICY         -3408

#define ALRIOS_SOVEREIGN_POLICY_ALLOW_UNSIGNED (1U << 0)
#define ALRIOS_SOVEREIGN_POLICY_REQUIRE_PQC     (1U << 1)
#define ALRIOS_SOVEREIGN_POLICY_MASK \
    (ALRIOS_SOVEREIGN_POLICY_ALLOW_UNSIGNED | \
     ALRIOS_SOVEREIGN_POLICY_REQUIRE_PQC)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct alrios_sovereign_keys {
    EVP_PKEY *ed25519_pubkey;
    EVP_PKEY *ml_dsa_pubkey;
    const uint8_t *aes_key;
    size_t aes_key_len;
    const alrios_certificate_t *leaf_certificate;
    const alrios_certificate_t *intermediate_certificate;
    const alrios_trust_store_t *trust_store;
} alrios_sovereign_keys_t;

/*
 * Validates the envelope and its signing chain before decrypting or creating
 * the returned descriptor. Signed packages always require a trusted
 * Leaf -> Intermediate -> Root chain. Unsigned packages are accepted only
 * when ALRIOS_SOVEREIGN_POLICY_ALLOW_UNSIGNED is explicitly set.
 */
int alrios_sovereign_loader_stage_memfd(
    const uint8_t *package_data,
    size_t package_len,
    uint64_t now,
    uint32_t policy_flags,
    const alrios_sovereign_keys_t *keys,
    int *out_memfd
);

int alrios_sovereign_execute_package(
    const uint8_t *package_data,
    size_t package_len,
    uint64_t now,
    int require_pqc,
    const alrios_sovereign_keys_t *keys,
    char *const argv[],
    char *const envp[],
    int *out_child_status
);

#ifdef __cplusplus
}
#endif

#endif /* ALRIOS_SOVEREIGN_LOADER_H */
