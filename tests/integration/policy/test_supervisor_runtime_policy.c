/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/supervisor/policy.h"
#include "alrios/crypto_verify.h"
#include "alrios/package/arapp_v2.h"
#include "alrios/pki/issuer.h"
#include "loader.h"

#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif

#define CHECK(cond) do { \
    if (!(cond)) { \
        (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                      __FILE__, __LINE__, #cond); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void write_u16_be(uint8_t *o, uint16_t v) {
    o[0] = (uint8_t)(v >> 8U);
    o[1] = (uint8_t)v;
}

static void write_u32_be(uint8_t *o, uint32_t v) {
    o[0] = (uint8_t)(v >> 24U);
    o[1] = (uint8_t)(v >> 16U);
    o[2] = (uint8_t)(v >> 8U);
    o[3] = (uint8_t)v;
}

static void write_u64_be(uint8_t *o, uint64_t v) {
    size_t index;
    for (index = 8U; index > 0U; --index) {
        o[index - 1U] = (uint8_t)v;
        v >>= 8U;
    }
}

static void write_file(const char *path, const void *data, size_t len) {
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL);
    CHECK(fwrite(data, 1U, len, f) == len);
    CHECK(fclose(f) == 0);
}

static uint8_t *make_package_buffer(uint32_t flags,
                                    uint64_t now,
                                    const char *manifest,
                                    size_t *out_len) {
    size_t mlen = strlen(manifest);
    size_t total = ALRIOS_ARAPP_V2_HEADER_SIZE + mlen;
    uint8_t *p = (uint8_t *)calloc(total, 1U);
    CHECK(p != NULL);
    memcpy(p, "ALRIGROUP@ARAPP", 15U);
    write_u16_be(p + 16U, ALRIOS_ARAPP_V2_VERSION);
    write_u16_be(p + 18U, ALRIOS_ARAPP_V2_ARCH_X86_64);
    write_u32_be(p + 20U, flags);
    write_u64_be(p + 24U, now > 5U ? now - 5U : 1U);
    write_u64_be(p + 32U, now + 1000U);
    write_u32_be(p + 40U, ALRIOS_ARAPP_V2_HEADER_SIZE);
    write_u32_be(p + 44U, (uint32_t)mlen);
    write_u32_be(p + 48U, 0U);
    memcpy(p + ALRIOS_ARAPP_V2_HEADER_SIZE, manifest, mlen);
    *out_len = total;
    return p;
}

int main(void) {
    char test_dir[] = "/tmp/alrios_supervisor_runtime_test_XXXXXX";
    char trust_dir[1024];
    char root_path[1024];
    char inter_path[1024];
    char leaf_path[1024];
    char ml_dsa_pub_path[1024];
    char unsigned_pkg_path[1024];
    char classical_pkg_path[1024];
    char hybrid_pkg_path[1024];
    char devmode_pkg_path[1024];

    EVP_PKEY *root_pkey = NULL;
    EVP_PKEY *inter_pkey = NULL;
    EVP_PKEY *leaf_pkey = NULL;
    EVP_PKEY *ml_dsa_key = NULL;
    alrios_certificate_t root_cert;
    alrios_certificate_t inter_cert;
    alrios_certificate_t leaf_cert;
    uint8_t root_wire[ALRIOS_CERTIFICATE_WIRE_MAX];
    uint8_t inter_wire[ALRIOS_CERTIFICATE_WIRE_MAX];
    uint8_t leaf_wire[ALRIOS_CERTIFICATE_WIRE_MAX];
    size_t root_wire_len = 0U;
    size_t inter_wire_len = 0U;
    size_t leaf_wire_len = 0U;

    uint8_t raw_ml_dsa_pub[ALRIOS_CERTIFICATE_ML_DSA_65_KEY_SIZE];
    size_t raw_ml_dsa_pub_len = sizeof(raw_ml_dsa_pub);

    uint64_t now = 1000U;
    const char *manifest = "{\"entry\":\"main.bin\",\"name\":\"app\",\"version\":\"1.0.0\"}";

    CHECK(alrios_crypto_has_capability(ALRIOS_CRYPTO_CAP_ML_DSA_65));
    CHECK(mkdtemp(test_dir) != NULL);
    (void)snprintf(trust_dir, sizeof(trust_dir), "%s/trust", test_dir);
    CHECK(mkdir(trust_dir, 0700) == 0);

    /* Build PKI material */
    CHECK(alrios_pki_issue_root("alrios.root.test", 365, &root_pkey,
                                &root_cert, root_wire, sizeof(root_wire),
                                &root_wire_len) == 0);
    CHECK(alrios_pki_issue_intermediate("alrios.inter.test", 90,
                                        &root_cert, root_pkey, &inter_pkey,
                                        &inter_cert, inter_wire, sizeof(inter_wire),
                                        &inter_wire_len) == 0);
    CHECK(alrios_pki_issue_leaf("app.test", 30,
                                &inter_cert, inter_pkey, &leaf_pkey,
                                &leaf_cert, leaf_wire, sizeof(leaf_wire),
                                &leaf_wire_len) == 0);
    now = leaf_cert.not_before + 10ULL;

    {
        EVP_PKEY_CTX *kctx = EVP_PKEY_CTX_new_from_name(NULL, "ML-DSA-65", NULL);
        CHECK(kctx != NULL);
        CHECK(EVP_PKEY_keygen_init(kctx) == 1);
        CHECK(EVP_PKEY_keygen(kctx, &ml_dsa_key) == 1);
        EVP_PKEY_CTX_free(kctx);
    }
    CHECK(EVP_PKEY_get_raw_public_key(ml_dsa_key, raw_ml_dsa_pub,
                                      &raw_ml_dsa_pub_len) == 1);
    CHECK(raw_ml_dsa_pub_len == ALRIOS_CERTIFICATE_ML_DSA_65_KEY_SIZE);

    (void)snprintf(root_path, sizeof(root_path), "%s/root.cert", trust_dir);
    (void)snprintf(inter_path, sizeof(inter_path), "%s/intermediate.cert", trust_dir);
    (void)snprintf(leaf_path, sizeof(leaf_path), "%s/leaf.cert", trust_dir);
    (void)snprintf(ml_dsa_pub_path, sizeof(ml_dsa_pub_path), "%s/ml-dsa-65.pub", trust_dir);

    write_file(root_path, root_wire, root_wire_len);
    write_file(inter_path, inter_wire, inter_wire_len);
    write_file(leaf_path, leaf_wire, leaf_wire_len);
    write_file(ml_dsa_pub_path, raw_ml_dsa_pub, raw_ml_dsa_pub_len);

    /* Construct test packages */
    (void)snprintf(unsigned_pkg_path, sizeof(unsigned_pkg_path), "%s/unsigned.arapp", test_dir);
    (void)snprintf(classical_pkg_path, sizeof(classical_pkg_path), "%s/classical.arapp", test_dir);
    (void)snprintf(hybrid_pkg_path, sizeof(hybrid_pkg_path), "%s/hybrid.arapp", test_dir);
    (void)snprintf(devmode_pkg_path, sizeof(devmode_pkg_path), "%s/devmode.arapp", test_dir);

    {
        size_t pkg_len = 0U;
        uint8_t *pkg = make_package_buffer(
            ALRIOS_ARAPP_V2_FLAG_PROFILE_SOVEREIGN |
            ALRIOS_ARAPP_V2_FLAG_RING_SOVEREIGN |
            ALRIOS_ARAPP_V2_FLAG_PQC_HYBRID_SIG,
            now, manifest, &pkg_len);
        write_file(unsigned_pkg_path, pkg, pkg_len);
        free(pkg);
    }

    {
        size_t pkg_len = 0U;
        uint8_t *pkg = make_package_buffer(
            ALRIOS_ARAPP_V2_FLAG_PROFILE_ENTERPRISE |
            ALRIOS_ARAPP_V2_FLAG_RING_DEVMODE,
            now, manifest, &pkg_len);
        CHECK(alrios_arapp_v2_sign_ed25519(pkg, pkg_len, leaf_pkey) ==
              ALRIOS_ARAPP_V2_SIG_OK);
        write_file(classical_pkg_path, pkg, pkg_len);
        free(pkg);
    }

    {
        size_t pkg_len = 0U;
        uint8_t *pkg = make_package_buffer(
            ALRIOS_ARAPP_V2_FLAG_PROFILE_SOVEREIGN |
            ALRIOS_ARAPP_V2_FLAG_RING_SOVEREIGN |
            ALRIOS_ARAPP_V2_FLAG_PQC_HYBRID_SIG,
            now, manifest, &pkg_len);
        CHECK(alrios_arapp_v2_sign_ed25519(pkg, pkg_len, leaf_pkey) ==
              ALRIOS_ARAPP_V2_SIG_OK);
        CHECK(alrios_arapp_v2_sign_ml_dsa_65(pkg, pkg_len, ml_dsa_key) ==
              ALRIOS_ARAPP_V2_SIG_OK);
        write_file(hybrid_pkg_path, pkg, pkg_len);
        free(pkg);
    }

    {
        size_t pkg_len = 0U;
        uint8_t *pkg = make_package_buffer(
            ALRIOS_ARAPP_V2_FLAG_PROFILE_ENTERPRISE |
            ALRIOS_ARAPP_V2_FLAG_RING_DEVMODE,
            now, manifest, &pkg_len);
        write_file(devmode_pkg_path, pkg, pkg_len);
        free(pkg);
    }

    /* 1. Test SOVEREIGN-MAX profile runtime enforcement */
    CHECK(loader_policy_initialize(PROFILE_SOVEREIGN_MAX, trust_dir) ==
          ALRIOS_POLICY_OK);
    {
        alrios_profile_t profile = PROFILE_EDGE_PERF;
        char reason[128] = {0};

        CHECK(loader_policy_get_profile(&profile) == ALRIOS_POLICY_OK);
        CHECK(profile == PROFILE_SOVEREIGN_MAX);
        CHECK(loader_policy_set_profile(PROFILE_ENTERPRISE_BAL) ==
              ALRIOS_POLICY_ERR_DOWNGRADE);
        CHECK(loader_policy_set_profile(PROFILE_SOVEREIGN_MAX) ==
              ALRIOS_POLICY_ERR_IMMUTABLE);

        /* Sovereign rejects unsigned */
        CHECK(loader_policy_evaluate_package(unsigned_pkg_path, reason, sizeof(reason)) ==
              ALRIOS_POLICY_ERR_SIGNATURE);
        /* Sovereign rejects classical-only signed packages */
        CHECK(loader_policy_evaluate_package(classical_pkg_path, reason, sizeof(reason)) ==
              ALRIOS_POLICY_ERR_SIGNATURE);
        /* Sovereign accepts valid hybrid signed packages */
        CHECK(loader_policy_evaluate_package(hybrid_pkg_path, reason, sizeof(reason)) ==
              ALRIOS_POLICY_OK);
    }
    loader_policy_shutdown();

    /* 2. Test ENTERPRISE-BALANCED profile runtime enforcement */
    CHECK(loader_policy_initialize(PROFILE_ENTERPRISE_BAL, trust_dir) ==
          ALRIOS_POLICY_OK);
    {
        char reason[128] = {0};

        /* Standard accepts classical-only signed package */
        CHECK(loader_policy_evaluate_package(classical_pkg_path, reason, sizeof(reason)) ==
              ALRIOS_POLICY_OK);
        /* Standard accepts verified developer mode unsigned package */
        CHECK(loader_policy_evaluate_package(devmode_pkg_path, reason, sizeof(reason)) ==
              ALRIOS_POLICY_OK);
        /* Standard accepts valid hybrid signed package */
        CHECK(loader_policy_evaluate_package(hybrid_pkg_path, reason, sizeof(reason)) ==
              ALRIOS_POLICY_OK);
    }
    loader_policy_shutdown();

    /* Cleanup temporary files */
    unlink(devmode_pkg_path);
    unlink(hybrid_pkg_path);
    unlink(classical_pkg_path);
    unlink(unsigned_pkg_path);
    unlink(ml_dsa_pub_path);
    unlink(leaf_path);
    unlink(inter_path);
    unlink(root_path);
    rmdir(trust_dir);
    rmdir(test_dir);

    EVP_PKEY_free(ml_dsa_key);
    EVP_PKEY_free(leaf_pkey);
    EVP_PKEY_free(inter_pkey);
    EVP_PKEY_free(root_pkey);

    (void)puts("test_supervisor_runtime_policy: PASS");
    return EXIT_SUCCESS;
}
