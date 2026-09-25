/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#include "alrios/crypto_verify.h"

#include <assert.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    uint8_t key[AES256_KEY_LEN];
    uint8_t iv[AES256_GCM_IV_LEN];
    uint8_t aad[] = "ALRIOS-ARAPP-V1";
    uint8_t plaintext[] = "SOVEREIGN_PAYLOAD";
    uint8_t ciphertext[sizeof(plaintext)];
    uint8_t decrypted[sizeof(plaintext)];
    uint8_t tag[AES256_GCM_TAG_LEN];

    memset(key, 0xA5, sizeof(key));
    memset(iv, 0x5A, sizeof(iv));
    memset(ciphertext, 0, sizeof(ciphertext));
    memset(decrypted, 0, sizeof(decrypted));

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    assert(ctx != NULL);
    assert(EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) == 1);
    assert(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, sizeof(iv), NULL) == 1);
    assert(EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) == 1);

    int output_len = 0;
    int final_len = 0;
    assert(EVP_EncryptUpdate(ctx, NULL, &output_len, aad, sizeof(aad) - 1U) == 1);
    assert(EVP_EncryptUpdate(ctx, ciphertext, &output_len,
                             plaintext, sizeof(plaintext)) == 1);
    assert(EVP_EncryptFinal_ex(ctx, ciphertext + output_len, &final_len) == 1);
    assert(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, sizeof(tag), tag) == 1);
    EVP_CIPHER_CTX_free(ctx);

    assert(alrios_aes256_gcm_decrypt(ciphertext, sizeof(ciphertext),
                                     aad, sizeof(aad) - 1U, tag, key, iv,
                                     decrypted) == ALRIOS_CRYPTO_OK);
    assert(memcmp(decrypted, plaintext, sizeof(plaintext)) == 0);

    tag[0] ^= 0x01U;
    memset(decrypted, 0xCC, sizeof(decrypted));
    assert(alrios_aes256_gcm_decrypt(ciphertext, sizeof(ciphertext),
                                     aad, sizeof(aad) - 1U, tag, key, iv,
                                     decrypted) == ALRIOS_CRYPTO_ERR_GCM_AUTH_FAIL);
    for (size_t i = 0; i < sizeof(decrypted); i++) {
        assert(decrypted[i] == 0U);
    }

    printf("TASK-016 (AES-256-GCM Authenticated Decrypt): PASS\n");
    return 0;
}
