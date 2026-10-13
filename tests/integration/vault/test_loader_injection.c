/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/vault/store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond) do { \
    if (!(cond)) { \
        (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                      __FILE__, __LINE__, #cond); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void test_vault_encryption_and_injection(void) {
    uint8_t master_key[32];
    alrios_vault_store_t store;
    char secrets[] = "DB_PASS=secret123\nAPI_KEY=keyABC\nSECRET_TOKEN=xyz\n";
    const char *authorized[] = {"DB_PASS", "API_KEY"};
    char env_buf[512];
    int status;

    memset(master_key, 0x42, sizeof(master_key));
    memset(&store, 0, sizeof(store));

    status = alrios_vault_encrypt_store(secrets, master_key, sizeof(master_key), &store);
    CHECK(status == ALRIOS_VAULT_OK);
    CHECK(store.encrypted_blob != NULL);

    memset(env_buf, 0, sizeof(env_buf));
    status = alrios_vault_decrypt_and_inject(
        &store, master_key, sizeof(master_key),
        authorized, 2U, env_buf, sizeof(env_buf)
    );
    CHECK(status == ALRIOS_VAULT_OK);
    CHECK(strstr(env_buf, "DB_PASS=secret123") != NULL);
    CHECK(strstr(env_buf, "API_KEY=keyABC") != NULL);
    CHECK(strstr(env_buf, "SECRET_TOKEN=xyz") == NULL); /* unauthorized key filtered out */

    alrios_vault_store_free(&store);
}

int main(void) {
    test_vault_encryption_and_injection();
    (void)puts("test_loader_injection: PASS");
    return EXIT_SUCCESS;
}
