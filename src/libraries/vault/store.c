/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/vault/store.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <stdlib.h>
#include <string.h>

int alrios_vault_encrypt_store(
    const char *json_secrets,
    const uint8_t *master_key,
    size_t key_len,
    alrios_vault_store_t *out_store
) {
    EVP_CIPHER_CTX *ctx = NULL;
    int len = 0;
    int ciphertext_len = 0;
    size_t secrets_len;
    uint8_t *ciphertext = NULL;

    if (!json_secrets || !master_key || key_len != 32U || !out_store) {
        return ALRIOS_VAULT_ERR_ARGUMENT;
    }
    memset(out_store, 0, sizeof(*out_store));
    secrets_len = strlen(json_secrets);
    if (secrets_len == 0U) {
        return ALRIOS_VAULT_ERR_SCHEMA;
    }

    if (RAND_bytes(out_store->salt, sizeof(out_store->salt)) != 1 ||
        RAND_bytes(out_store->iv, sizeof(out_store->iv)) != 1) {
        return ALRIOS_VAULT_ERR_CRYPTO;
    }

    ciphertext = malloc(secrets_len);
    if (!ciphertext) {
        return ALRIOS_VAULT_ERR_IO;
    }

    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        free(ciphertext);
        return ALRIOS_VAULT_ERR_CRYPTO;
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, NULL) != 1 ||
        EVP_EncryptInit_ex(ctx, NULL, NULL, master_key, out_store->iv) != 1 ||
        EVP_EncryptUpdate(ctx, ciphertext, &len,
                          (const unsigned char *)json_secrets,
                          (int)secrets_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        free(ciphertext);
        return ALRIOS_VAULT_ERR_CRYPTO;
    }
    ciphertext_len = len;

    if (EVP_EncryptFinal_ex(ctx, ciphertext + ciphertext_len, &len) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16,
                          out_store->tag) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        free(ciphertext);
        return ALRIOS_VAULT_ERR_CRYPTO;
    }
    ciphertext_len += len;

    EVP_CIPHER_CTX_free(ctx);
    out_store->encrypted_blob = ciphertext;
    out_store->blob_len = (size_t)ciphertext_len;
    return ALRIOS_VAULT_OK;
}

int alrios_vault_decrypt_and_inject(
    const alrios_vault_store_t *store,
    const uint8_t *master_key,
    size_t key_len,
    const char *const *authorized_keys,
    size_t num_authorized,
    char *out_env_buf,
    size_t env_buf_len
) {
    EVP_CIPHER_CTX *ctx = NULL;
    int len = 0;
    int plaintext_len = 0;
    uint8_t *plaintext = NULL;
    int ret = ALRIOS_VAULT_ERR_CRYPTO;
    size_t offset = 0U;
    size_t i;

    if (!store || !store->encrypted_blob || !master_key || key_len != 32U ||
        !out_env_buf || env_buf_len == 0U) {
        return ALRIOS_VAULT_ERR_ARGUMENT;
    }
    out_env_buf[0] = '\0';

    plaintext = malloc(store->blob_len > 0U ? store->blob_len : 1U);
    if (!plaintext) {
        return ALRIOS_VAULT_ERR_IO;
    }

    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        free(plaintext);
        return ALRIOS_VAULT_ERR_CRYPTO;
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, NULL) != 1 ||
        EVP_DecryptInit_ex(ctx, NULL, NULL, master_key, store->iv) != 1 ||
        EVP_DecryptUpdate(ctx, plaintext, &len, store->encrypted_blob,
                          (int)store->blob_len) != 1) {
        goto cleanup;
    }
    plaintext_len = len;

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16,
                          (void *)store->tag) != 1 ||
        EVP_DecryptFinal_ex(ctx, plaintext + plaintext_len, &len) != 1) {
        goto cleanup;
    }
    plaintext_len += len;

    /* Parse key=value pairs or JSON and filter by authorized_keys */
    {
        char *copy = malloc((size_t)plaintext_len + 1U);
        if (!copy) {
            goto cleanup;
        }
        memcpy(copy, plaintext, (size_t)plaintext_len);
        copy[plaintext_len] = '\0';

        /* Simple line parser: KEY=VALUE format */
        char *line = strtok(copy, "\n,\r{}");
        while (line) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = line;
                char *v = eq + 1;
                /* trim quotes or spaces if needed */
                int allowed = (num_authorized == 0U);
                for (i = 0U; i < num_authorized; ++i) {
                    if (authorized_keys && strcmp(k, authorized_keys[i]) == 0) {
                        allowed = 1;
                        break;
                    }
                }
                if (allowed) {
                    size_t needed = strlen(k) + strlen(v) + 2U;
                    if (offset + needed < env_buf_len) {
                        int written = snprintf(out_env_buf + offset,
                                               env_buf_len - offset,
                                               "%s=%s\n", k, v);
                        if (written > 0) {
                            offset += (size_t)written;
                        }
                    }
                }
            }
            line = strtok(NULL, "\n,\r{}");
        }
        memset(copy, 0, (size_t)plaintext_len + 1U);
        free(copy);
    }

    ret = ALRIOS_VAULT_OK;

cleanup:
    if (plaintext) {
        memset(plaintext, 0, store->blob_len);
        free(plaintext);
    }
    EVP_CIPHER_CTX_free(ctx);
    return ret;
}

void alrios_vault_store_free(alrios_vault_store_t *store) {
    if (store) {
        if (store->encrypted_blob) {
            memset(store->encrypted_blob, 0, store->blob_len);
            free(store->encrypted_blob);
            store->encrypted_blob = NULL;
        }
        memset(store, 0, sizeof(*store));
    }
}
