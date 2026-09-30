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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_CHECK(cond) do { \
    if (!(cond)) { \
        (void)fprintf(stderr, "FAIL: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

int main(void) {
    alrios_vault_t vault;
    TEST_CHECK(alrios_vault_init(&vault) == 0);
    TEST_CHECK(alrios_vault_set(&vault, "DB_URL", "postgres://secure") == 0);
    TEST_CHECK(alrios_vault_set(&vault, "API_SECRET", "super_secret_token") == 0);
    TEST_CHECK(alrios_vault_set(&vault, "PUBLIC_PARAM", "hello") == 0);

    uint8_t key[32];
    uint8_t iv[12];
    memset(key, 0x42, sizeof(key));
    memset(iv, 0x11, sizeof(iv));

    uint8_t ciphertext[4096];
    size_t ciphertext_len = 0;
    uint8_t tag[16];

    TEST_CHECK(alrios_vault_store_encrypt(&vault, key, iv, ciphertext, &ciphertext_len, tag) == 0);
    TEST_CHECK(ciphertext_len > 0);

    alrios_vault_t decrypted_vault;
    TEST_CHECK(alrios_vault_store_decrypt(ciphertext, ciphertext_len, key, iv, tag, &decrypted_vault) == 0);
    TEST_CHECK(decrypted_vault.count == 3);
    TEST_CHECK(strcmp(alrios_vault_get(&decrypted_vault, "DB_URL"), "postgres://secure") == 0);

    const char *required[] = { "DB_URL", "API_SECRET" };
    TEST_CHECK(alrios_vault_enforce_schema(&decrypted_vault, required, 2) == 0);

    const char *missing_required[] = { "DB_URL", "MISSING_KEY" };
    TEST_CHECK(alrios_vault_enforce_schema(&decrypted_vault, missing_required, 2) != 0);

    const char *authorized[] = { "DB_URL", "PUBLIC_PARAM" };
    char **envp = NULL;
    TEST_CHECK(alrios_vault_inject_filtered(&decrypted_vault, authorized, 2, required, 2, &envp) == 0);
    TEST_CHECK(envp != NULL);
    TEST_CHECK(strcmp(envp[0], "DB_URL=postgres://secure") == 0);
    TEST_CHECK(strcmp(envp[1], "PUBLIC_PARAM=hello") == 0);
    TEST_CHECK(envp[2] == NULL);

    alrios_vault_free_envp(envp, 2);

    alrios_vault_t overflow_vault;
    TEST_CHECK(alrios_vault_init(&overflow_vault) == 0);
    overflow_vault.count = ALRIOS_VAULT_MAX_ENTRIES + 1;
    TEST_CHECK(alrios_vault_store_encrypt(&overflow_vault, key, iv, ciphertext, &ciphertext_len, tag) != 0);

    overflow_vault.count = SIZE_MAX / sizeof(alrios_vault_entry_t) + 2;
    TEST_CHECK(alrios_vault_store_encrypt(&overflow_vault, key, iv, ciphertext, &ciphertext_len, tag) != 0);

    (void)printf("TEST_LOADER_INJECTION: PASS\n");
    return 0;
}
