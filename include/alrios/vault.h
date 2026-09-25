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

#endif /* ALRIOS_VAULT_H */
