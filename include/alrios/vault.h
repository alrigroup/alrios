/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#ifndef ALRIOS_VAULT_H
#define ALRIOS_VAULT_H

#include <stdint.h>
#include <stddef.h>

#define ALRIOS_VAULT_MAX_ENTRIES 128
#define ALRIOS_VAULT_KEY_MAX     64
#define ALRIOS_VAULT_VAL_MAX     1024

typedef struct alrios_vault_entry {
    char key[ALRIOS_VAULT_KEY_MAX];
    char value[ALRIOS_VAULT_VAL_MAX];
} alrios_vault_entry_t;

typedef struct alrios_vault {
    size_t count;
    alrios_vault_entry_t entries[ALRIOS_VAULT_MAX_ENTRIES];
} alrios_vault_t;

int alrios_vault_init(alrios_vault_t *v);
int alrios_vault_set(alrios_vault_t *v, const char *key, const char *val);
const char *alrios_vault_get(const alrios_vault_t *v, const char *key);
int alrios_vault_build_envp(const alrios_vault_t *v, char ***out_envp);
void alrios_vault_free_envp(char **envp, size_t count);

int alrios_vault_store_encrypt(const alrios_vault_t *vault, const uint8_t key[32], const uint8_t iv[12], uint8_t *out_ciphertext, size_t *out_ciphertext_len, uint8_t out_tag[16]);
int alrios_vault_store_decrypt(const uint8_t *ciphertext, size_t ciphertext_len, const uint8_t key[32], const uint8_t iv[12], const uint8_t tag[16], alrios_vault_t *out_vault);

int alrios_vault_enforce_schema(const alrios_vault_t *vault, const char *const *required_keys, size_t required_count);
int alrios_vault_inject_filtered(const alrios_vault_t *vault, const char *const *authorized_keys, size_t authorized_count, const char *const *required_keys, size_t required_count, char ***out_envp);

#endif /* ALRIOS_VAULT_H */
