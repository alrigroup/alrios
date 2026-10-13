/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/package/arapp_v2.h"
#include "alrios/crypto_verify.h"

#include <openssl/evp.h>
#include <openssl/pem.h>
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

/* External functions from arapp_v2_signatures.c */
int alrios_arapp_v2_sign_ed25519(uint8_t *package, size_t package_len,
                                  EVP_PKEY *ed25519_key);
int alrios_arapp_v2_verify_ed25519(const uint8_t *package, size_t package_len,
                                    EVP_PKEY *ed25519_pubkey);
int alrios_arapp_v2_sign_ml_dsa_65(uint8_t *package, size_t package_len,
                                    EVP_PKEY *ml_dsa_key);
int alrios_arapp_v2_verify_ml_dsa_65(const uint8_t *package, size_t package_len,
                                      EVP_PKEY *ml_dsa_pubkey);
int alrios_arapp_v2_verify_hybrid(const uint8_t *package, size_t package_len,
                                   EVP_PKEY *ed25519_pubkey,
                                   EVP_PKEY *ml_dsa_pubkey,
                                   int require_pqc);

#define SIG_OK        0
#define SIG_ERR_ARG  -3101
#define SIG_TAMPERED -3106

static void write_u16_le(uint8_t *o, uint16_t v) {
    o[0]=(uint8_t)v; o[1]=(uint8_t)(v>>8U);
}
static void write_u32_le(uint8_t *o, uint32_t v) {
    o[0]=(uint8_t)v; o[1]=(uint8_t)(v>>8U);
    o[2]=(uint8_t)(v>>16U); o[3]=(uint8_t)(v>>24U);
}
static void write_u64_le(uint8_t *o, uint64_t v) {
    write_u32_le(o,(uint32_t)v);
    write_u32_le(o+4U,(uint32_t)(v>>32U));
}

static uint8_t *make_test_package(const char *manifest, size_t *out_len) {
    size_t mlen = strlen(manifest);
    size_t total = ALRIOS_ARAPP_V2_HEADER_SIZE + mlen;
    uint8_t *p = calloc(total, 1U);
    CHECK(p != NULL);
    memcpy(p, "ALRIGROUP@ARAPP", 15U);
    write_u16_le(p+16U, ALRIOS_ARAPP_V2_VERSION);
    write_u16_le(p+18U, ALRIOS_ARAPP_V2_ARCH_X86_64);
    write_u32_le(p+20U, ALRIOS_ARAPP_V2_FLAG_PROFILE_ENTERPRISE |
                         ALRIOS_ARAPP_V2_FLAG_RING_DEVMODE |
                         ALRIOS_ARAPP_V2_FLAG_PQC_HYBRID_SIG);
    write_u64_le(p+24U, 1000U);
    write_u64_le(p+32U, 2000U);
    write_u32_le(p+40U, ALRIOS_ARAPP_V2_HEADER_SIZE);
    write_u32_le(p+44U, (uint32_t)mlen);
    write_u64_le(p+48U, 0U);
    memcpy(p + ALRIOS_ARAPP_V2_HEADER_SIZE, manifest, mlen);
    *out_len = total;
    return p;
}

static void test_ed25519_sign_verify(void) {
    EVP_PKEY *key = NULL;
    EVP_PKEY_CTX *kctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
    size_t pkg_len;
    uint8_t *pkg;
    int status;

    CHECK(kctx != NULL);
    CHECK(EVP_PKEY_keygen_init(kctx) == 1);
    CHECK(EVP_PKEY_keygen(kctx, &key) == 1);
    EVP_PKEY_CTX_free(kctx);

    pkg = make_test_package("{\"a\":1}", &pkg_len);

    /* Sign */
    status = alrios_arapp_v2_sign_ed25519(pkg, pkg_len, key);
    CHECK(status == SIG_OK);

    /* Verify */
    status = alrios_arapp_v2_verify_ed25519(pkg, pkg_len, key);
    CHECK(status == SIG_OK);

    /* Tamper and verify */
    pkg[ALRIOS_ARAPP_V2_HEADER_SIZE] ^= 0xFFU;
    status = alrios_arapp_v2_verify_ed25519(pkg, pkg_len, key);
    CHECK(status == SIG_TAMPERED);

    EVP_PKEY_free(key);
    free(pkg);
}

static void test_null_arguments(void) {
    CHECK(alrios_arapp_v2_sign_ed25519(NULL, 100U, NULL) == SIG_ERR_ARG);
    CHECK(alrios_arapp_v2_verify_ed25519(NULL, 100U, NULL) == SIG_ERR_ARG);
}

static void test_hybrid_policy(void) {
    EVP_PKEY *ed_key = NULL;
    EVP_PKEY_CTX *kctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
    size_t pkg_len;
    uint8_t *pkg;
    int status;

    CHECK(kctx != NULL);
    CHECK(EVP_PKEY_keygen_init(kctx) == 1);
    CHECK(EVP_PKEY_keygen(kctx, &ed_key) == 1);
    EVP_PKEY_CTX_free(kctx);

    pkg = make_test_package("{\"a\":1}", &pkg_len);
    CHECK(alrios_arapp_v2_sign_ed25519(pkg, pkg_len, ed_key) == SIG_OK);

    /* Hybrid verify without PQC key, require_pqc=0 → OK */
    status = alrios_arapp_v2_verify_hybrid(pkg, pkg_len, ed_key, NULL, 0);
    CHECK(status == SIG_OK);

    /* Hybrid verify without PQC key, require_pqc=1 → POLICY error */
    status = alrios_arapp_v2_verify_hybrid(pkg, pkg_len, ed_key, NULL, 1);
    CHECK(status == -3105);

    EVP_PKEY_free(ed_key);
    free(pkg);
}

#if defined(ALRIOS_HAVE_ML_DSA_65)
static void test_ml_dsa_sign_verify(void) {
    EVP_PKEY *key = NULL;
    EVP_PKEY_CTX *kctx = EVP_PKEY_CTX_new_from_name(NULL, "ML-DSA-65", NULL);
    size_t pkg_len;
    uint8_t *pkg;
    int status;

    if (!kctx) {
        (void)puts("ML-DSA-65 not available, skipping PQC test");
        return;
    }
    CHECK(EVP_PKEY_keygen_init(kctx) == 1);
    CHECK(EVP_PKEY_keygen(kctx, &key) == 1);
    EVP_PKEY_CTX_free(kctx);

    pkg = make_test_package("{\"a\":1}", &pkg_len);
    status = alrios_arapp_v2_sign_ml_dsa_65(pkg, pkg_len, key);
    CHECK(status == SIG_OK);
    status = alrios_arapp_v2_verify_ml_dsa_65(pkg, pkg_len, key);
    CHECK(status == SIG_OK);

    /* Tamper */
    pkg[ALRIOS_ARAPP_V2_HEADER_SIZE] ^= 0xFFU;
    status = alrios_arapp_v2_verify_ml_dsa_65(pkg, pkg_len, key);
    CHECK(status == SIG_TAMPERED);

    EVP_PKEY_free(key);
    free(pkg);
}
#endif

int main(void) {
    test_null_arguments();
    test_ed25519_sign_verify();
    test_hybrid_policy();
#if defined(ALRIOS_HAVE_ML_DSA_65)
    test_ml_dsa_sign_verify();
#endif
    (void)puts("test_signed_arapp: PASS");
    return EXIT_SUCCESS;
}
