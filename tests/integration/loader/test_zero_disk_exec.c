/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/loader/sovereign_loader.h"
#include "alrios/package/arapp_v2.h"

#include <openssl/evp.h>
#include <openssl/sha.h>
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

/* External signature functions */
int alrios_arapp_v2_sign_ed25519(uint8_t *package, size_t package_len,
                                  EVP_PKEY *ed25519_key);

static void write_u16_le(uint8_t *o, uint16_t v) { o[0]=(uint8_t)v; o[1]=(uint8_t)(v>>8U); }
static void write_u32_le(uint8_t *o, uint32_t v) {
    o[0]=(uint8_t)v; o[1]=(uint8_t)(v>>8U);
    o[2]=(uint8_t)(v>>16U); o[3]=(uint8_t)(v>>24U);
}
static void write_u64_le(uint8_t *o, uint64_t v) {
    write_u32_le(o,(uint32_t)v);
    write_u32_le(o+4U,(uint32_t)(v>>32U));
}

static uint8_t *encrypt_aes_gcm_256(
    const uint8_t *plaintext, size_t plaintext_len,
    const uint8_t *key, const uint8_t *iv,
    uint8_t out_tag[16],
    uint8_t **out_ciphertext, size_t *out_len
) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0;
    int ciphertext_len = 0;
    uint8_t *ciphertext = malloc(plaintext_len > 0U ? plaintext_len : 1U);

    CHECK(ctx != NULL && ciphertext != NULL);
    CHECK(EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) == 1);
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, NULL) == 1);
    CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) == 1);
    CHECK(EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, (int)plaintext_len) == 1);
    ciphertext_len = len;
    CHECK(EVP_EncryptFinal_ex(ctx, ciphertext + ciphertext_len, &len) == 1);
    ciphertext_len += len;
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, out_tag) == 1);

    EVP_CIPHER_CTX_free(ctx);
    *out_ciphertext = ciphertext;
    *out_len = (size_t)ciphertext_len;
    return ciphertext;
}

static void test_verify_before_decrypt(void) {
    EVP_PKEY *ed_key = NULL;
    EVP_PKEY_CTX *kctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
    uint8_t aes_key[32];
    uint8_t iv[12];
    uint8_t tag[16];
    uint8_t cleartext_sha[64];
    static const char manifest[] = "{\"app_id\":\"com.test\"}";
    static const char raw_elf[] = "\x7f\x45\x4c\x46\x01\x01\x01\x00";
    uint8_t *ciphertext = NULL;
    size_t ciphertext_len = 0U;
    size_t mlen = strlen(manifest);
    size_t package_len;
    uint8_t *package;
    alrios_sovereign_keys_t keys;
    int child_status = 0;
    int status;

    CHECK(kctx != NULL);
    CHECK(EVP_PKEY_keygen_init(kctx) == 1);
    CHECK(EVP_PKEY_keygen(kctx, &ed_key) == 1);
    EVP_PKEY_CTX_free(kctx);

    memset(aes_key, 0x55, sizeof(aes_key));
    memset(iv, 0xAA, sizeof(iv));
    SHA512((const unsigned char *)raw_elf, sizeof(raw_elf), cleartext_sha);

    encrypt_aes_gcm_256((const uint8_t *)raw_elf, sizeof(raw_elf),
                        aes_key, iv, tag, &ciphertext, &ciphertext_len);

    package_len = ALRIOS_ARAPP_V2_HEADER_SIZE + mlen + ciphertext_len;
    package = calloc(package_len, 1U);
    CHECK(package != NULL);

    memcpy(package, "ALRIGROUP@ARAPP", 15U);
    write_u16_le(package + 16U, ALRIOS_ARAPP_V2_VERSION);
    write_u16_le(package + 18U, ALRIOS_ARAPP_V2_ARCH_X86_64);
    write_u32_le(package + 20U, ALRIOS_ARAPP_V2_FLAG_PROFILE_ENTERPRISE |
                                 ALRIOS_ARAPP_V2_FLAG_RING_DEVMODE);
    write_u64_le(package + 24U, 1000U);
    write_u64_le(package + 32U, 2000U);
    write_u32_le(package + 40U, ALRIOS_ARAPP_V2_HEADER_SIZE);
    write_u32_le(package + 44U, (uint32_t)mlen);
    write_u64_le(package + 48U, ciphertext_len);
    memcpy(package + 56U, iv, 12U);
    memcpy(package + 68U, tag, 16U);
    memcpy(package + 84U, cleartext_sha, 64U);
    memcpy(package + ALRIOS_ARAPP_V2_HEADER_SIZE, manifest, mlen);
    memcpy(package + ALRIOS_ARAPP_V2_HEADER_SIZE + mlen, ciphertext, ciphertext_len);

    /* Sign package */
    CHECK(alrios_arapp_v2_sign_ed25519(package, package_len, ed_key) == 0);

    keys.ed25519_pubkey = ed_key;
    keys.ml_dsa_pubkey = NULL;
    keys.aes_key = aes_key;
    keys.aes_key_len = sizeof(aes_key);

    /* Tamper with signature/manifest after signing — verification must fail before decryption */
    package[ALRIOS_ARAPP_V2_HEADER_SIZE] ^= 0xFFU;

    status = alrios_sovereign_execute_package(
        package, package_len, 1500U, 0, &keys, NULL, NULL, &child_status
    );
    CHECK(status == ALRIOS_SOVEREIGN_ERR_VERIFY);

    EVP_PKEY_free(ed_key);
    free(ciphertext);
    free(package);
}

int main(void) {
    test_verify_before_decrypt();
    (void)puts("test_zero_disk_exec: PASS");
    return EXIT_SUCCESS;
}
