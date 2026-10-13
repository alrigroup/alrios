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
#define ALRIOS_VAULT_ERR_ISOLATION       -3606
#define ALRIOS_VAULT_ERR_MEMFD           -3607
#define ALRIOS_VAULT_ERR_FORMAT          -3608

#define ALRIOS_VAULT_SALT_LEN            16U
#define ALRIOS_VAULT_IV_LEN              12U
#define ALRIOS_VAULT_TAG_LEN             16U
#define ALRIOS_VAULT_KEY_LEN             32U
#define ALRIOS_VAULT_DEFAULT_PBKDF2_ITERS 100000U

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

/*
 * Derives a 32-byte AES-256 key from a passphrase and salt using PBKDF2-HMAC-SHA256.
 */
int alrios_vault_derive_key_pbkdf2(
    const void *passphrase,
    size_t passphrase_len,
    const uint8_t salt[16],
    uint32_t iterations,
    uint8_t out_key[32]
);

/*
 * Derives a 32-byte subkey using HKDF-SHA256 from a master key, salt, and domain info.
 */
int alrios_vault_derive_key_hkdf(
    const uint8_t *master_key,
    size_t master_key_len,
    const uint8_t salt[16],
    const char *info,
    uint8_t out_key[32]
);

/*
 * Encrypts secrets into an authenticated AES-256-GCM vault store.
 * If key_len == 32, master_key is used directly.
 * If key_len != 32, a 32-byte key is derived from master_key and salt via PBKDF2.
 */
int alrios_vault_encrypt_store(
    const char *json_secrets,
    const uint8_t *master_key,
    size_t key_len,
    alrios_vault_store_t *out_store
);

/*
 * Encrypts secrets into out_store with key derived from passphrase via PBKDF2.
 */
int alrios_vault_seal_with_passphrase(
    const char *json_secrets,
    const uint8_t *passphrase,
    size_t passphrase_len,
    uint32_t iterations,
    alrios_vault_store_t *out_store
);

/*
 * Decrypts vault store and injects secrets into out_env_buf (format: KEY=VALUE\n).
 * Filters secrets using authorized_keys when num_authorized > 0.
 */
int alrios_vault_decrypt_and_inject(
    const alrios_vault_store_t *store,
    const uint8_t *master_key,
    size_t key_len,
    const char *const *authorized_keys,
    size_t num_authorized,
    char *out_env_buf,
    size_t env_buf_len
);

/*
 * Decrypts vault store and injects secrets into out_env_buf with strict per-app isolation.
 * If app_id is non-NULL, only secrets belonging to app_id are injected;
 * secrets belonging to other app IDs are strictly excluded.
 */
int alrios_vault_decrypt_and_inject_for_app(
    const alrios_vault_store_t *store,
    const uint8_t *master_key,
    size_t key_len,
    const char *app_id,
    const char *const *authorized_keys,
    size_t num_authorized,
    char *out_env_buf,
    size_t env_buf_len
);

/*
 * Sovereign loader injection helper:
 * Decrypts secrets isolated for app_id and materializes a NULL-terminated POSIX
 * envp array in volatile memory for direct handover into sovereign loader execution.
 */
int alrios_vault_inject_envp_for_app(
    const alrios_vault_store_t *store,
    const uint8_t *master_key,
    size_t key_len,
    const char *app_id,
    const char *const *authorized_keys,
    size_t num_authorized,
    char ***out_envp,
    size_t *out_count
);

/*
 * Securely zeroizes and frees an envp array produced by alrios_vault_inject_envp_for_app.
 */
void alrios_vault_store_free_envp(char **envp, size_t count);

/*
 * Sovereign loader injection helper:
 * Decrypts secrets isolated for app_id and writes them into an anonymous, sealed
 * Linux memfd with zero filesystem exposure. The returned descriptor has all seals
 * applied (F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE) and is rewound to 0.
 */
int alrios_vault_inject_memfd_for_app(
    const alrios_vault_store_t *store,
    const uint8_t *master_key,
    size_t key_len,
    const char *app_id,
    const char *const *authorized_keys,
    size_t num_authorized,
    int *out_memfd
);

/*
 * Persists an encrypted vault store to the host filesystem with mode 0600.
 */
int alrios_vault_store_save_file(
    const char *path,
    const alrios_vault_store_t *store
);

/*
 * Loads an encrypted vault store from the host filesystem.
 */
int alrios_vault_store_load_file(
    const char *path,
    alrios_vault_store_t *out_store
);

/*
 * Explicitly zeroizes all secret fields and frees allocated memory in store.
 */
void alrios_vault_store_free(alrios_vault_store_t *store);

#ifdef __cplusplus
}
#endif

#endif /* ALRIOS_VAULT_STORE_H */
