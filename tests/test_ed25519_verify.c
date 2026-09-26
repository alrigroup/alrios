/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#include "alrios/crypto_verify.h"

#include <assert.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    EVP_PKEY_CTX *keygen = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
    assert(keygen != NULL);
    assert(EVP_PKEY_keygen_init(keygen) == 1);

    EVP_PKEY *key = NULL;
    assert(EVP_PKEY_keygen(keygen, &key) == 1);
    EVP_PKEY_CTX_free(keygen);

    uint8_t public_key[ED25519_PUBLIC_KEY_LEN];
    size_t public_key_len = sizeof(public_key);
    assert(EVP_PKEY_get_raw_public_key(key, public_key, &public_key_len) == 1);
    (void)public_key_len;
    assert(public_key_len == ED25519_PUBLIC_KEY_LEN);

    uint8_t digest[SHA512_DIGEST_LEN];
    for (size_t i = 0; i < sizeof(digest); i++) {
        digest[i] = (uint8_t)i;
    }

    uint8_t signature[ED25519_SIGNATURE_LEN];
    size_t signature_len = sizeof(signature);
    EVP_MD_CTX *sign_ctx = EVP_MD_CTX_new();
    assert(sign_ctx != NULL);
    assert(EVP_DigestSignInit(sign_ctx, NULL, NULL, NULL, key) == 1);
    assert(EVP_DigestSign(sign_ctx, signature, &signature_len,
                          digest, sizeof(digest)) == 1);
    (void)signature_len;
    assert(signature_len == ED25519_SIGNATURE_LEN);
    EVP_MD_CTX_free(sign_ctx);
    EVP_PKEY_free(key);

    assert(alrios_ed25519_verify(public_key, digest, signature) == ALRIOS_CRYPTO_OK);

    digest[0] ^= 0x01U;
    assert(alrios_ed25519_verify(public_key, digest, signature) ==
           ALRIOS_CRYPTO_ERR_BAD_SIG_CLASSIC);

    printf("TASK-014 (Ed25519 Verify): PASS\n");
    return 0;
}
