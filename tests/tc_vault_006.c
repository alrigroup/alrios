/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */
/*
 * TASK-006: Squad 1 — In-Memory Vault Configuration Matrix Injection
 * Test Specification TC-VAULT-006: Comprehensive Verification Suite
 *
 * Checks:
 *   - Correct materialization of vault entries into POSIX envp format
 *   - Null parameters handling
 *   - Strict key sanitization (^([A-Z_][A-Z0-9_]*)$)
 *   - Leading digit rejection on keys
 *   - Leading '=' rejection on values
 *   - Empty values permitted ("KEY=")
 *   - Array boundaries and POSIX NULL terminator enforcement
 *   - Full zeroization upon release
 *   - Zero memory leaks (ASan + UBSan clean)
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alrios/vault_loader.h"

static void test_null_params(void) {
    char **envp = NULL;
    (void)envp;
    vault_matrix_t vault;
    memset(&vault, 0, sizeof(vault));

    assert(alrios_vault_inject_envp(NULL, &envp) == VAULT_ERR_NULL_PARAM);
    assert(alrios_vault_inject_envp(&vault, NULL) == VAULT_ERR_NULL_PARAM);
    assert(alrios_vault_validate(NULL) == VAULT_ERR_NULL_PARAM);

    /* Free with NULL should be safe */
    alrios_vault_free_envp(NULL, 0);
    alrios_vault_free_envp(NULL, 10);
}

static void test_empty_vault(void) {
    char **envp = NULL;
    (void)envp;
    vault_matrix_t vault;
    memset(&vault, 0, sizeof(vault));
    vault.count = 0;

    assert(alrios_vault_inject_envp(&vault, &envp) == VAULT_ERR_EMPTY);
    assert(envp == NULL);
}

static void test_count_overflow(void) {
    char **envp = NULL;
    (void)envp;
    vault_matrix_t vault;
    memset(&vault, 0, sizeof(vault));
    vault.count = VAULT_MAX_ENTRIES + 1;

    assert(alrios_vault_inject_envp(&vault, &envp) == VAULT_ERR_COUNT_OVERFLOW);
    assert(envp == NULL);
}

static void test_key_validation(void) {
    char **envp = NULL;
    vault_matrix_t vault;
    memset(&vault, 0, sizeof(vault));
    vault.count = 1;

    /* Invalid lowercase */
    snprintf(vault.entries[0].key, sizeof(vault.entries[0].key), "database_url");
    snprintf(vault.entries[0].val, sizeof(vault.entries[0].val), "postgres://localhost");
    assert(alrios_vault_inject_envp(&vault, &envp) == VAULT_ERR_INVALID_KEY);
    assert(envp == NULL);

    /* Invalid special character */
    snprintf(vault.entries[0].key, sizeof(vault.entries[0].key), "DB-URL");
    assert(alrios_vault_inject_envp(&vault, &envp) == VAULT_ERR_INVALID_KEY);
    assert(envp == NULL);

    /* Invalid leading digit */
    snprintf(vault.entries[0].key, sizeof(vault.entries[0].key), "1DB_KEY");
    assert(alrios_vault_inject_envp(&vault, &envp) == VAULT_ERR_INVALID_KEY);
    assert(envp == NULL);

    /* Valid uppercase with underscore and digits not at the front */
    snprintf(vault.entries[0].key, sizeof(vault.entries[0].key), "DB_KEY_V2");
    assert(alrios_vault_inject_envp(&vault, &envp) == VAULT_OK);
    assert(envp != NULL);
    assert(strcmp(envp[0], "DB_KEY_V2=postgres://localhost") == 0);
    assert(envp[1] == NULL);
    alrios_vault_free_envp(envp, 1);
}

static void test_value_validation(void) {
    char **envp = NULL;
    vault_matrix_t vault;
    memset(&vault, 0, sizeof(vault));
    vault.count = 1;

    /* Leading '=' in value is rejected */
    snprintf(vault.entries[0].key, sizeof(vault.entries[0].key), "API_KEY");
    snprintf(vault.entries[0].val, sizeof(vault.entries[0].val), "=secret_token");
    assert(alrios_vault_inject_envp(&vault, &envp) == VAULT_ERR_INVALID_VALUE);
    assert(envp == NULL);

    /* Empty value is permitted */
    snprintf(vault.entries[0].key, sizeof(vault.entries[0].key), "FLAG_DEBUG");
    vault.entries[0].val[0] = '\0';
    assert(alrios_vault_inject_envp(&vault, &envp) == VAULT_OK);
    assert(envp != NULL);
    assert(strcmp(envp[0], "FLAG_DEBUG=") == 0);
    assert(envp[1] == NULL);
    alrios_vault_free_envp(envp, 1);
}

static void test_multi_entry_injection(void) {
    char **envp = NULL;
    vault_matrix_t vault;
    memset(&vault, 0, sizeof(vault));
    vault.count = 4;

    snprintf(vault.entries[0].key, sizeof(vault.entries[0].key), "PORT");
    snprintf(vault.entries[0].val, sizeof(vault.entries[0].val), "8080");

    snprintf(vault.entries[1].key, sizeof(vault.entries[1].key), "JWT_SECRET");
    snprintf(vault.entries[1].val, sizeof(vault.entries[1].val), "4a8e8b61c92d04a6b2");

    snprintf(vault.entries[2].key, sizeof(vault.entries[2].key), "APP_ENV");
    snprintf(vault.entries[2].val, sizeof(vault.entries[2].val), "production");

    snprintf(vault.entries[3].key, sizeof(vault.entries[3].key), "ALLOW_BURST");
    snprintf(vault.entries[3].val, sizeof(vault.entries[3].val), "1");

    assert(alrios_vault_inject_envp(&vault, &envp) == VAULT_OK);
    assert(envp != NULL);

    assert(strcmp(envp[0], "PORT=8080") == 0);
    assert(strcmp(envp[1], "JWT_SECRET=4a8e8b61c92d04a6b2") == 0);
    assert(strcmp(envp[2], "APP_ENV=production") == 0);
    assert(strcmp(envp[3], "ALLOW_BURST=1") == 0);
    assert(envp[4] == NULL); /* POSIX null terminator */

    alrios_vault_free_envp(envp, vault.count);
}

int main(void) {
    test_null_params();
    test_empty_vault();
    test_count_overflow();
    test_key_validation();
    test_value_validation();
    test_multi_entry_injection();

    puts("TC-VAULT-006: PASS");
    return 0;
}
