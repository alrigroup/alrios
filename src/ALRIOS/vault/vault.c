/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */

#include "alrios/vault.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int alrios_vault_init(alrios_vault_t *v) {
    if (!v) return -1;
    memset(v, 0, sizeof(*v));
    return 0;
}

int alrios_vault_set(alrios_vault_t *v, const char *key, const char *val) {
    if (!v || !key || !val || v->count >= ALRIOS_VAULT_MAX_ENTRIES) return -1;
    for (size_t i = 0; i < v->count; i++) {
        if (strcmp(v->entries[i].key, key) == 0) {
            strncpy(v->entries[i].value, val, ALRIOS_VAULT_VAL_MAX - 1);
            return 0;
        }
    }
    strncpy(v->entries[v->count].key, key, ALRIOS_VAULT_KEY_MAX - 1);
    strncpy(v->entries[v->count].value, val, ALRIOS_VAULT_VAL_MAX - 1);
    v->count++;
    return 0;
}

const char *alrios_vault_get(const alrios_vault_t *v, const char *key) {
    if (!v || !key) return NULL;
    for (size_t i = 0; i < v->count; i++) {
        if (strcmp(v->entries[i].key, key) == 0) return v->entries[i].value;
    }
    return NULL;
}

int alrios_vault_build_envp(const alrios_vault_t *v, char ***out_envp) {
    if (!v || !out_envp) return -1;
    char **envp = calloc(v->count + 1, sizeof(char *));
    if (!envp) return -2;

    for (size_t i = 0; i < v->count; i++) {
        size_t len = strlen(v->entries[i].key) + strlen(v->entries[i].value) + 2;
        envp[i] = malloc(len);
        if (!envp[i]) return -3;
        snprintf(envp[i], len, "%s=%s", v->entries[i].key, v->entries[i].value);
    }
    envp[v->count] = NULL;
    *out_envp = envp;
    return 0;
}

void alrios_vault_free_envp(char **envp, size_t count) {
    if (!envp) return;
    for (size_t i = 0; i < count; i++) {
        if (envp[i]) {
            free(envp[i]);
        }
    }
    free(envp);
}
