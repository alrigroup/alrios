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

#include "alrios/memory/sensitive.h"
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SENSITIVE_REGIONS 64U

typedef struct sensitive_region {
    const void *ptr;
    size_t size;
    int active;
} sensitive_region_t;

static sensitive_region_t g_regions[MAX_SENSITIVE_REGIONS];
static size_t g_region_count = 0;

int alrios_sensitive_register(const void *ptr, size_t size) {
    if (!ptr || size == 0U) {
        return ALRIOS_SENSITIVE_ERR_INVALID;
    }
    if (g_region_count >= MAX_SENSITIVE_REGIONS) {
        return ALRIOS_SENSITIVE_ERR_CAPACITY;
    }
    g_regions[g_region_count].ptr = ptr;
    g_regions[g_region_count].size = size;
    g_regions[g_region_count].active = 1;
    g_region_count++;
    return ALRIOS_SENSITIVE_OK;
}

int alrios_sensitive_unregister(const void *ptr) {
    if (!ptr) {
        return ALRIOS_SENSITIVE_ERR_INVALID;
    }
    for (size_t i = 0; i < g_region_count; i++) {
        if (g_regions[i].active && g_regions[i].ptr == ptr) {
            g_regions[i].active = 0;
            return ALRIOS_SENSITIVE_OK;
        }
    }
    return ALRIOS_SENSITIVE_ERR_INVALID;
}

int alrios_sensitive_sanitize_dump(const void *raw_dump, size_t dump_size,
                                   const uint8_t *key, const uint8_t *iv,
                                   uint8_t *out_encrypted_dump, size_t *out_encrypted_size,
                                   uint8_t out_tag[16]) {
    if (!raw_dump || !key || !iv || !out_encrypted_dump || !out_encrypted_size || !out_tag) {
        return ALRIOS_SENSITIVE_ERR_INVALID;
    }

    // 1. Copy raw dump to a working buffer so we can sanitize registered memory regions
    uint8_t *sanitized = (uint8_t *)malloc(dump_size > 0 ? dump_size : 1);
    if (!sanitized) {
        return ALRIOS_SENSITIVE_ERR_ENCRYPT;
    }
    memcpy(sanitized, raw_dump, dump_size);

    // 2. Overwrite any registered sensitive memory regions present in dump with zeros (sanitization)
    for (size_t i = 0; i < dump_size; i++) {
        const uint8_t *addr = ((const uint8_t *)raw_dump) + i;
        for (size_t r = 0; r < g_region_count; r++) {
            if (g_regions[r].active) {
                const uint8_t *reg_start = (const uint8_t *)g_regions[r].ptr;
                const uint8_t *reg_end = reg_start + g_regions[r].size;
                if (addr >= reg_start && addr < reg_end) {
                    sanitized[i] = 0x00; // Zeroed out!
                }
            }
        }
    }

    // 3. Encrypt sanitized dump using AES-256-GCM
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        free(sanitized);
        return ALRIOS_SENSITIVE_ERR_ENCRYPT;
    }

    int len = 0;
    int ciphertext_len = 0;

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, key, iv) != 1 ||
        EVP_EncryptUpdate(ctx, out_encrypted_dump, &len, sanitized, (int)dump_size) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        free(sanitized);
        return ALRIOS_SENSITIVE_ERR_ENCRYPT;
    }
    ciphertext_len = len;

    if (EVP_EncryptFinal_ex(ctx, out_encrypted_dump + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        free(sanitized);
        return ALRIOS_SENSITIVE_ERR_ENCRYPT;
    }
    ciphertext_len += len;

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, out_tag) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        free(sanitized);
        return ALRIOS_SENSITIVE_ERR_ENCRYPT;
    }

    EVP_CIPHER_CTX_free(ctx);
    free(sanitized);
    *out_encrypted_size = (size_t)ciphertext_len;
    return ALRIOS_SENSITIVE_OK;
}

int alrios_sensitive_decrypt_dump(const uint8_t *encrypted_dump, size_t encrypted_size,
                                  const uint8_t *key, const uint8_t *iv,
                                  const uint8_t tag[16],
                                  uint8_t *out_decrypted_dump, size_t *out_decrypted_size) {
    if (!encrypted_dump || !key || !iv || !tag || !out_decrypted_dump || !out_decrypted_size) {
        return ALRIOS_SENSITIVE_ERR_INVALID;
    }

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return ALRIOS_SENSITIVE_ERR_AUTH;
    }

    int len = 0;
    int plaintext_len = 0;

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, key, iv) != 1 ||
        EVP_DecryptUpdate(ctx, out_decrypted_dump, &len, encrypted_dump, (int)encrypted_size) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return ALRIOS_SENSITIVE_ERR_AUTH;
    }
    plaintext_len = len;

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, (void *)tag) != 1 ||
        EVP_DecryptFinal_ex(ctx, out_decrypted_dump + len, &len) <= 0) {
        EVP_CIPHER_CTX_free(ctx);
        return ALRIOS_SENSITIVE_ERR_AUTH; // Unauthorized / corrupted tag rejected!
    }
    plaintext_len += len;

    EVP_CIPHER_CTX_free(ctx);
    *out_decrypted_size = (size_t)plaintext_len;
    return ALRIOS_SENSITIVE_OK;
}
