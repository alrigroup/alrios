/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/vault.h"
#include "alrios/crypto_verify.h"
#include <string.h>
#include <stdlib.h>

int alrios_vault_enforce_schema(const alrios_vault_t *vault, const char *const *required_keys, size_t required_count) {
    if (!vault) {
        return -1;
    }
    if (!required_keys || required_count == 0) {
        return 0;
    }
    for (size_t i = 0; i < required_count; i++) {
        if (!required_keys[i]) {
            return -2;
        }
        const char *val = alrios_vault_get(vault, required_keys[i]);
        if (!val) {
            return -3;
        }
    }
    return 0;
}

int alrios_vault_inject_filtered(const alrios_vault_t *vault, const char *const *authorized_keys, size_t authorized_count, const char *const *required_keys, size_t required_count, char ***out_envp) {
    if (!vault || !out_envp) {
        return -1;
    }
    *out_envp = NULL;

    int schema_rc = alrios_vault_enforce_schema(vault, required_keys, required_count);
    if (schema_rc != 0) {
        return schema_rc;
    }

    alrios_vault_t filtered;
    if (alrios_vault_init(&filtered) != 0) {
        return -2;
    }

    if (!authorized_keys || authorized_count == 0) {
        for (size_t i = 0; i < vault->count; i++) {
            if (alrios_vault_set(&filtered, vault->entries[i].key, vault->entries[i].value) != 0) {
                return -3;
            }
        }
    } else {
        for (size_t i = 0; i < vault->count; i++) {
            int authorized = 0;
            for (size_t j = 0; j < authorized_count; j++) {
                if (authorized_keys[j] && strcmp(vault->entries[i].key, authorized_keys[j]) == 0) {
                    authorized = 1;
                    break;
                }
            }
            if (authorized) {
                if (alrios_vault_set(&filtered, vault->entries[i].key, vault->entries[i].value) != 0) {
                    return -3;
                }
            }
        }
    }

    int build_rc = alrios_vault_build_envp(&filtered, out_envp);
    alrios_explicit_zeroize(&filtered, sizeof(filtered));
    return build_rc;
}
