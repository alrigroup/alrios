/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "alrios/vault/store.h"
#include "alrios/crypto_verify.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef F_GET_SEALS
#define F_GET_SEALS 1034
#endif
#ifndef F_SEAL_SEAL
#define F_SEAL_SEAL 0x0001
#endif
#ifndef F_SEAL_SHRINK
#define F_SEAL_SHRINK 0x0002
#endif
#ifndef F_SEAL_GROW
#define F_SEAL_GROW 0x0004
#endif
#ifndef F_SEAL_WRITE
#define F_SEAL_WRITE 0x0008
#endif

#define CHECK(cond) do { \
    if (!(cond)) { \
        (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                      __FILE__, __LINE__, #cond); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static int bytes_all_zero(const uint8_t *bytes, size_t length) {
    size_t index;

    for (index = 0U; index < length; ++index) {
        if (bytes[index] != 0U) {
            return 0;
        }
    }
    return 1;
}

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
    CHECK(strstr(env_buf, "SECRET_TOKEN=xyz") == NULL);

    alrios_vault_store_free(&store);
}

static void test_encrypted_vault_host_io(void) {
    static const char *test_path = "/tmp/alrios_test_vault_host.bin";
    uint8_t master_key[32];
    alrios_vault_store_t store;
    alrios_vault_store_t loaded;
    struct stat st;
    char env_buf[256];
    int status;

    memset(master_key, 0x7a, sizeof(master_key));
    memset(&store, 0, sizeof(store));
    memset(&loaded, 0, sizeof(loaded));

    (void)unlink(test_path);
    status = alrios_vault_encrypt_store("PORT=8080\nHOST=127.0.0.1\n",
                                        master_key, sizeof(master_key), &store);
    CHECK(status == ALRIOS_VAULT_OK);

    status = alrios_vault_store_save_file(test_path, &store);
    CHECK(status == ALRIOS_VAULT_OK);

    CHECK(stat(test_path, &st) == 0);
    CHECK((st.st_mode & 0777) == 0600);

    status = alrios_vault_store_load_file(test_path, &loaded);
    CHECK(status == ALRIOS_VAULT_OK);
    CHECK(loaded.blob_len == store.blob_len);
    CHECK(memcmp(loaded.salt, store.salt, sizeof(store.salt)) == 0);
    CHECK(memcmp(loaded.iv, store.iv, sizeof(store.iv)) == 0);
    CHECK(memcmp(loaded.tag, store.tag, sizeof(store.tag)) == 0);

    memset(env_buf, 0, sizeof(env_buf));
    status = alrios_vault_decrypt_and_inject(&loaded, master_key, sizeof(master_key),
                                             NULL, 0U, env_buf, sizeof(env_buf));
    CHECK(status == ALRIOS_VAULT_OK);
    CHECK(strstr(env_buf, "PORT=8080") != NULL);
    CHECK(strstr(env_buf, "HOST=127.0.0.1") != NULL);

    (void)unlink(test_path);
    alrios_vault_store_free(&store);
    alrios_vault_store_free(&loaded);
}

static void test_tag_tampering_fail_closed(void) {
    uint8_t master_key[32];
    alrios_vault_store_t store;
    char env_buf[256];
    int status;

    memset(master_key, 0x55, sizeof(master_key));
    memset(&store, 0, sizeof(store));

    status = alrios_vault_encrypt_store("CRITICAL_SECRET=omega\n",
                                        master_key, sizeof(master_key), &store);
    CHECK(status == ALRIOS_VAULT_OK);

    /* 1. Tamper tag */
    store.tag[0] ^= 0xa5;
    memset(env_buf, 0xbb, sizeof(env_buf));
    status = alrios_vault_decrypt_and_inject(&store, master_key, sizeof(master_key),
                                             NULL, 0U, env_buf, sizeof(env_buf));
    CHECK(status == ALRIOS_VAULT_ERR_CRYPTO);
    CHECK(env_buf[0] == '\0');
    store.tag[0] ^= 0xa5;

    /* 2. Tamper ciphertext */
    store.encrypted_blob[0] ^= 0x5a;
    memset(env_buf, 0xbb, sizeof(env_buf));
    status = alrios_vault_decrypt_and_inject(&store, master_key, sizeof(master_key),
                                             NULL, 0U, env_buf, sizeof(env_buf));
    CHECK(status == ALRIOS_VAULT_ERR_CRYPTO);
    CHECK(env_buf[0] == '\0');
    store.encrypted_blob[0] ^= 0x5a;

    /* 3. Tamper IV */
    store.iv[0] ^= 0x33;
    memset(env_buf, 0xbb, sizeof(env_buf));
    status = alrios_vault_decrypt_and_inject(&store, master_key, sizeof(master_key),
                                             NULL, 0U, env_buf, sizeof(env_buf));
    CHECK(status == ALRIOS_VAULT_ERR_CRYPTO);
    CHECK(env_buf[0] == '\0');
    store.iv[0] ^= 0x33;

    /* 4. Wrong key */
    master_key[0] ^= 0xff;
    memset(env_buf, 0xbb, sizeof(env_buf));
    status = alrios_vault_decrypt_and_inject(&store, master_key, sizeof(master_key),
                                             NULL, 0U, env_buf, sizeof(env_buf));
    CHECK(status == ALRIOS_VAULT_ERR_CRYPTO);
    CHECK(env_buf[0] == '\0');

    alrios_vault_store_free(&store);
}

static void test_per_app_secret_isolation(void) {
    uint8_t master_key[32];
    alrios_vault_store_t store;
    char env_buf[512];
    int status;
    const char *auth_secret_name = "AUTH_KEY";
    const char *billing_secret_name = "STRIPE_KEY";

    memset(master_key, 0x88, sizeof(master_key));
    memset(&store, 0, sizeof(store));

    /* Store with line-formatted per-app secrets */
    const char *line_secrets =
        "com.alrigroup.auth:AUTH_KEY=auth_token_999\n"
        "com.alrigroup.auth:DB_PASS=auth_pass\n"
        "com.alrigroup.billing:STRIPE_KEY=sk_live_123\n"
        "com.alrigroup.billing:DB_PASS=billing_pass\n";

    status = alrios_vault_encrypt_store(line_secrets, master_key, sizeof(master_key), &store);
    CHECK(status == ALRIOS_VAULT_OK);

    /* App A can access only its own secrets */
    memset(env_buf, 0, sizeof(env_buf));
    status = alrios_vault_decrypt_and_inject_for_app(
        &store, master_key, sizeof(master_key),
        "com.alrigroup.auth", NULL, 0U, env_buf, sizeof(env_buf)
    );
    CHECK(status == ALRIOS_VAULT_OK);
    CHECK(strstr(env_buf, "AUTH_KEY=auth_token_999") != NULL);
    CHECK(strstr(env_buf, "DB_PASS=auth_pass") != NULL);
    CHECK(strstr(env_buf, "STRIPE_KEY") == NULL);
    CHECK(strstr(env_buf, "billing_pass") == NULL);

    /* App A cannot access App B's secret even when explicitly requested */
    memset(env_buf, 0, sizeof(env_buf));
    status = alrios_vault_decrypt_and_inject_for_app(
        &store, master_key, sizeof(master_key),
        "com.alrigroup.auth", &billing_secret_name, 1U, env_buf, sizeof(env_buf)
    );
    CHECK(status == ALRIOS_VAULT_ERR_NOT_FOUND);
    CHECK(env_buf[0] == '\0');

    /* App B can access only its own secrets */
    memset(env_buf, 0, sizeof(env_buf));
    status = alrios_vault_decrypt_and_inject_for_app(
        &store, master_key, sizeof(master_key),
        "com.alrigroup.billing", NULL, 0U, env_buf, sizeof(env_buf)
    );
    CHECK(status == ALRIOS_VAULT_OK);
    CHECK(strstr(env_buf, "STRIPE_KEY=sk_live_123") != NULL);
    CHECK(strstr(env_buf, "DB_PASS=billing_pass") != NULL);
    CHECK(strstr(env_buf, "AUTH_KEY") == NULL);
    CHECK(strstr(env_buf, "auth_pass") == NULL);

    /* App B cannot access App A's secret even when explicitly requested */
    memset(env_buf, 0, sizeof(env_buf));
    status = alrios_vault_decrypt_and_inject_for_app(
        &store, master_key, sizeof(master_key),
        "com.alrigroup.billing", &auth_secret_name, 1U, env_buf, sizeof(env_buf)
    );
    CHECK(status == ALRIOS_VAULT_ERR_NOT_FOUND);
    CHECK(env_buf[0] == '\0');

    /* Unauthorized app gets zero secrets */
    memset(env_buf, 0, sizeof(env_buf));
    status = alrios_vault_decrypt_and_inject_for_app(
        &store, master_key, sizeof(master_key),
        "com.alrigroup.unauthorized", NULL, 0U, env_buf, sizeof(env_buf)
    );
    CHECK(status == ALRIOS_VAULT_ERR_NOT_FOUND);
    CHECK(env_buf[0] == '\0');

    alrios_vault_store_free(&store);

    /* Now test JSON format with per-app objects */
    const char *json_secrets =
        "{\n"
        "  \"com.alri.service_a\": {\"KEY_A\": \"secretA\", \"COMMON\": \"valA\"},\n"
        "  \"com.alri.service_b\": {\"KEY_B\": \"secretB\", \"COMMON\": \"valB\"}\n"
        "}\n";

    status = alrios_vault_encrypt_store(json_secrets, master_key, sizeof(master_key), &store);
    CHECK(status == ALRIOS_VAULT_OK);

    memset(env_buf, 0, sizeof(env_buf));
    status = alrios_vault_decrypt_and_inject_for_app(
        &store, master_key, sizeof(master_key),
        "com.alri.service_a", NULL, 0U, env_buf, sizeof(env_buf)
    );
    CHECK(status == ALRIOS_VAULT_OK);
    CHECK(strstr(env_buf, "KEY_A=secretA") != NULL);
    CHECK(strstr(env_buf, "COMMON=valA") != NULL);
    CHECK(strstr(env_buf, "KEY_B") == NULL);
    CHECK(strstr(env_buf, "valB") == NULL);

    memset(env_buf, 0, sizeof(env_buf));
    status = alrios_vault_decrypt_and_inject_for_app(
        &store, master_key, sizeof(master_key),
        "com.alri.service_b", NULL, 0U, env_buf, sizeof(env_buf)
    );
    CHECK(status == ALRIOS_VAULT_OK);
    CHECK(strstr(env_buf, "KEY_B=secretB") != NULL);
    CHECK(strstr(env_buf, "COMMON=valB") != NULL);
    CHECK(strstr(env_buf, "KEY_A") == NULL);
    CHECK(strstr(env_buf, "valA") == NULL);

    alrios_vault_store_free(&store);
}

static void test_loader_injection_envp_and_memfd_zero_disk(void) {
    uint8_t master_key[32];
    alrios_vault_store_t store;
    char **envp = NULL;
    size_t envp_count = 0U;
    int memfd = -1;
    int status;
    int seals;
    char read_buf[256];
    ssize_t bytes_read;

    memset(master_key, 0x99, sizeof(master_key));
    memset(&store, 0, sizeof(store));

    const char *secrets =
        "com.alrigroup.svc:JWT_TOKEN=super_jwt_secret_token\n"
        "com.alrigroup.svc:REDIS_URL=redis://cluster\n";

    status = alrios_vault_encrypt_store(secrets, master_key, sizeof(master_key), &store);
    CHECK(status == ALRIOS_VAULT_OK);

    /* 1. In-memory envp injection for sovereign loader execution */
    status = alrios_vault_inject_envp_for_app(
        &store, master_key, sizeof(master_key),
        "com.alrigroup.svc", NULL, 0U,
        &envp, &envp_count
    );
    CHECK(status == ALRIOS_VAULT_OK);
    CHECK(envp != NULL);
    CHECK(envp_count == 2U);
    CHECK(envp[2] == NULL);
    CHECK((strcmp(envp[0], "JWT_TOKEN=super_jwt_secret_token") == 0 &&
           strcmp(envp[1], "REDIS_URL=redis://cluster") == 0) ||
          (strcmp(envp[1], "JWT_TOKEN=super_jwt_secret_token") == 0 &&
           strcmp(envp[0], "REDIS_URL=redis://cluster") == 0));

    alrios_vault_store_free_envp(envp, envp_count);
    envp = NULL;

    /* 2. Zero-disk anonymous sealed memfd injection */
    status = alrios_vault_inject_memfd_for_app(
        &store, master_key, sizeof(master_key),
        "com.alrigroup.svc", NULL, 0U,
        &memfd
    );
    CHECK(status == ALRIOS_VAULT_OK);
    CHECK(memfd >= 0);

    /* Verify all required seals are applied */
    seals = fcntl(memfd, F_GET_SEALS);
    CHECK(seals >= 0);
    CHECK((seals & (F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE)) ==
          (F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE));

    /* Verify write to sealed memfd fails */
    CHECK(write(memfd, "TAMPER", 6) == -1);
    CHECK(errno == EPERM);

    /* Verify content matches decrypted secrets */
    memset(read_buf, 0, sizeof(read_buf));
    bytes_read = read(memfd, read_buf, sizeof(read_buf) - 1);
    CHECK(bytes_read > 0);
    read_buf[bytes_read] = '\0';
    CHECK(strstr(read_buf, "JWT_TOKEN=super_jwt_secret_token") != NULL);
    CHECK(strstr(read_buf, "REDIS_URL=redis://cluster") != NULL);

    (void)close(memfd);
    alrios_vault_store_free(&store);
}

static void test_key_derivation_pbkdf2_and_hkdf(void) {
    uint8_t salt[16];
    uint8_t key1[32];
    uint8_t key2[32];
    uint8_t key3[32];
    const char *passphrase = "master_passphrase_sovereign_2026";
    alrios_vault_store_t store;
    char env_buf[256];
    int status;

    memset(salt, 0x12, sizeof(salt));
    memset(key1, 0, sizeof(key1));
    memset(key2, 0, sizeof(key2));
    memset(key3, 0, sizeof(key3));

    /* PBKDF2 derivation test */
    status = alrios_vault_derive_key_pbkdf2(passphrase, strlen(passphrase), salt, 1000U, key1);
    CHECK(status == ALRIOS_VAULT_OK);
    CHECK(!bytes_all_zero(key1, sizeof(key1)));

    status = alrios_vault_derive_key_pbkdf2(passphrase, strlen(passphrase), salt, 1000U, key2);
    CHECK(status == ALRIOS_VAULT_OK);
    CHECK(memcmp(key1, key2, sizeof(key1)) == 0);

    /* Different salt produces different key */
    salt[0] ^= 0x01;
    status = alrios_vault_derive_key_pbkdf2(passphrase, strlen(passphrase), salt, 1000U, key3);
    CHECK(status == ALRIOS_VAULT_OK);
    CHECK(memcmp(key1, key3, sizeof(key1)) != 0);
    salt[0] ^= 0x01;

    /* HKDF derivation test */
    memset(key1, 0, sizeof(key1));
    memset(key2, 0, sizeof(key2));
    status = alrios_vault_derive_key_hkdf((const uint8_t *)passphrase, strlen(passphrase),
                                          salt, "context_a", key1);
    CHECK(status == ALRIOS_VAULT_OK);
    status = alrios_vault_derive_key_hkdf((const uint8_t *)passphrase, strlen(passphrase),
                                          salt, "context_b", key2);
    CHECK(status == ALRIOS_VAULT_OK);
    CHECK(memcmp(key1, key2, sizeof(key1)) != 0);

    /* Vault seal with passphrase and PBKDF2 */
    memset(&store, 0, sizeof(store));
    status = alrios_vault_seal_with_passphrase(
        "KDF_SECRET=verified_passphrase\n",
        (const uint8_t *)passphrase, strlen(passphrase),
        1000U, &store
    );
    CHECK(status == ALRIOS_VAULT_OK);

    /* Decrypt using key derived from passphrase */
    memset(key1, 0, sizeof(key1));
    status = alrios_vault_derive_key_pbkdf2(passphrase, strlen(passphrase), store.salt, 1000U, key1);
    CHECK(status == ALRIOS_VAULT_OK);

    memset(env_buf, 0, sizeof(env_buf));
    status = alrios_vault_decrypt_and_inject(&store, key1, sizeof(key1),
                                             NULL, 0U, env_buf, sizeof(env_buf));
    CHECK(status == ALRIOS_VAULT_OK);
    CHECK(strstr(env_buf, "KDF_SECRET=verified_passphrase") != NULL);

    alrios_vault_store_free(&store);
    alrios_explicit_zeroize(key1, sizeof(key1));
    alrios_explicit_zeroize(key2, sizeof(key2));
    alrios_explicit_zeroize(key3, sizeof(key3));
}

static void test_zeroization_verification(void) {
    uint8_t buffer[64];
    alrios_vault_store_t store;
    uint8_t key[32];
    int status;

    memset(buffer, 0xef, sizeof(buffer));
    CHECK(!bytes_all_zero(buffer, sizeof(buffer)));
    alrios_explicit_zeroize(buffer, sizeof(buffer));
    CHECK(bytes_all_zero(buffer, sizeof(buffer)));

    memset(key, 0x33, sizeof(key));
    memset(&store, 0, sizeof(store));
    status = alrios_vault_encrypt_store("SECRET=to_be_zeroized\n", key, sizeof(key), &store);
    CHECK(status == ALRIOS_VAULT_OK);
    CHECK(store.encrypted_blob != NULL);
    CHECK(!bytes_all_zero(store.salt, sizeof(store.salt)));
    CHECK(!bytes_all_zero(store.iv, sizeof(store.iv)));
    CHECK(!bytes_all_zero(store.tag, sizeof(store.tag)));

    alrios_vault_store_free(&store);
    CHECK(store.encrypted_blob == NULL);
    CHECK(store.blob_len == 0U);
    CHECK(bytes_all_zero(store.salt, sizeof(store.salt)));
    CHECK(bytes_all_zero(store.iv, sizeof(store.iv)));
    CHECK(bytes_all_zero(store.tag, sizeof(store.tag)));
}

int main(void) {
    test_vault_encryption_and_injection();
    test_encrypted_vault_host_io();
    test_tag_tampering_fail_closed();
    test_per_app_secret_isolation();
    test_loader_injection_envp_and_memfd_zero_disk();
    test_key_derivation_pbkdf2_and_hkdf();
    test_zeroization_verification();

    (void)puts("test_loader_injection: PASS");
    return EXIT_SUCCESS;
}
