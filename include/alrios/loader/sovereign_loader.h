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

#include <stddef.h>
#include <stdint.h>
#include <openssl/evp.h>

#define ALRIOS_SOVEREIGN_OK                  0
#define ALRIOS_SOVEREIGN_ERR_ARGUMENT       -3401
#define ALRIOS_SOVEREIGN_ERR_VERIFY         -3402
#define ALRIOS_SOVEREIGN_ERR_DECRYPT        -3403
#define ALRIOS_SOVEREIGN_ERR_MEMFD          -3404
#define ALRIOS_SOVEREIGN_ERR_SEAL           -3405
#define ALRIOS_SOVEREIGN_ERR_IO             -3406
#define ALRIOS_SOVEREIGN_ERR_PLATFORM       -3407

#ifdef __cplusplus
extern "C" {
#endif

typedef struct alrios_sovereign_keys {
    EVP_PKEY *ed25519_pubkey;
    EVP_PKEY *ml_dsa_pubkey;
    const uint8_t *aes_key;
    size_t aes_key_len;
} alrios_sovereign_keys_t;

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
