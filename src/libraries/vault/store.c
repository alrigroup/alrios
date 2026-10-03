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
#include <openssl/evp.h>

int alrios_vault_store_encrypt(const alrios_vault_t *vault, const uint8_t key[32], const uint8_t iv[12], uint8_t *out_ciphertext, size_t *out_ciphertext_len, uint8_t out_tag[16]) {
    if (!vault || !key || !iv || !out_ciphertext || !out_ciphertext_len || !out_tag) {
        return -1;
    }
    if (vault->count > ALRIOS_VAULT_MAX_ENTRIES) {
        return -5;
    }
    if (vault->count > 0 && vault->count > SIZE_MAX / sizeof(alrios_vault_entry_t)) {
        return -6;
    }
    size_t plaintext_len = sizeof(size_t) + vault->count * sizeof(alrios_vault_entry_t);
    uint8_t *plaintext = (uint8_t *)malloc(plaintext_len);
    if (!plaintext) {
        return -2;
    }
    memcpy(plaintext, &vault->count, sizeof(size_t));
    memcpy(plaintext + sizeof(size_t), vault->entries, vault->count * sizeof(alrios_vault_entry_t));

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        alrios_explicit_zeroize(plaintext, plaintext_len);
        free(plaintext);
        return -3;
    }

    int out_len = 0;
    int fin_len = 0;
    int rc = -4;

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, NULL) != 1 ||
        EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) != 1 ||
        EVP_EncryptUpdate(ctx, out_ciphertext, &out_len, plaintext, (int)plaintext_len) != 1 ||
        EVP_EncryptFinal_ex(ctx, out_ciphertext + out_len, &fin_len) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, out_tag) != 1) {
        goto cleanup;
    }

    *out_ciphertext_len = (size_t)(out_len + fin_len);
    rc = 0;

cleanup:
    EVP_CIPHER_CTX_free(ctx);
    alrios_explicit_zeroize(plaintext, plaintext_len);
    free(plaintext);
    return rc;
}

int alrios_vault_store_decrypt(const uint8_t *ciphertext, size_t ciphertext_len, const uint8_t key[32], const uint8_t iv[12], const uint8_t tag[16], alrios_vault_t *out_vault) {
    if (!ciphertext || !key || !iv || !tag || !out_vault || ciphertext_len == 0) {
        return -1;
    }
    size_t plaintext_len = ciphertext_len;
    uint8_t *plaintext = (uint8_t *)malloc(plaintext_len);
    if (!plaintext) {
        return -2;
    }

    int decrypt_rc = alrios_aes256_gcm_decrypt(
        ciphertext,
        ciphertext_len,
        NULL,
        0,
        tag,
        key,
        iv,
        plaintext
    );

    if (decrypt_rc != ALRIOS_CRYPTO_OK) {
        alrios_explicit_zeroize(plaintext, plaintext_len);
        free(plaintext);
        return -3;
    }

    if (plaintext_len < sizeof(size_t)) {
        alrios_explicit_zeroize(plaintext, plaintext_len);
        free(plaintext);
        return -4;
    }

    size_t count = 0;
    memcpy(&count, plaintext, sizeof(size_t));
    if (count > ALRIOS_VAULT_MAX_ENTRIES || plaintext_len < sizeof(size_t) + count * sizeof(alrios_vault_entry_t)) {
        alrios_explicit_zeroize(plaintext, plaintext_len);
        free(plaintext);
        return -5;
    }

    alrios_vault_init(out_vault);
    out_vault->count = count;
    memcpy(out_vault->entries, plaintext + sizeof(size_t), count * sizeof(alrios_vault_entry_t));

    alrios_explicit_zeroize(plaintext, plaintext_len);
    free(plaintext);
    return 0;
}
