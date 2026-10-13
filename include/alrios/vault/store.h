/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_VAULT_STORE_H
#define ALRIOS_VAULT_STORE_H

#include <stddef.h>
#include <stdint.h>

#define ALRIOS_VAULT_OK                   0
#define ALRIOS_VAULT_ERR_ARGUMENT        -3601
#define ALRIOS_VAULT_ERR_CRYPTO          -3602
#define ALRIOS_VAULT_ERR_SCHEMA          -3603
#define ALRIOS_VAULT_ERR_IO              -3604
#define ALRIOS_VAULT_ERR_NOT_FOUND       -3605

#ifdef __cplusplus
extern "C" {
#endif

typedef struct alrios_vault_store {
    uint8_t *encrypted_blob;
    size_t blob_len;
    uint8_t salt[16];
    uint8_t iv[12];
    uint8_t tag[16];
} alrios_vault_store_t;

int alrios_vault_encrypt_store(
    const char *json_secrets,
    const uint8_t *master_key,
    size_t key_len,
    alrios_vault_store_t *out_store
);

int alrios_vault_decrypt_and_inject(
    const alrios_vault_store_t *store,
    const uint8_t *master_key,
    size_t key_len,
    const char *const *authorized_keys,
    size_t num_authorized,
    char *out_env_buf,
    size_t env_buf_len
);

void alrios_vault_store_free(alrios_vault_store_t *store);

#ifdef __cplusplus
}
#endif

#endif /* ALRIOS_VAULT_STORE_H */
