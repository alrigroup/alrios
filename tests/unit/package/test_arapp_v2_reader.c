/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/package/arapp_v2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                      __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void write_u16_be(uint8_t *output, uint16_t value) {
    output[0] = (uint8_t)(value >> 8U);
    output[1] = (uint8_t)value;
}

static void write_u32_be(uint8_t *output, uint32_t value) {
    output[0] = (uint8_t)(value >> 24U);
    output[1] = (uint8_t)(value >> 16U);
    output[2] = (uint8_t)(value >> 8U);
    output[3] = (uint8_t)value;
}

static void write_u64_be(uint8_t *output, uint64_t value) {
    size_t index;

    for (index = 8U; index > 0U; --index) {
        output[index - 1U] = (uint8_t)value;
        value >>= 8U;
    }
}

static uint8_t *make_valid_package(const char *manifest,
                                   const uint8_t *ciphertext,
                                   size_t ciphertext_len,
                                   size_t *out_len) {
    size_t manifest_len;
    size_t package_len;
    uint8_t *package;

    CHECK(manifest != NULL);
    CHECK(out_len != NULL);
    CHECK(ciphertext_len == 0U || ciphertext != NULL);
    manifest_len = strlen(manifest);
    CHECK(manifest_len <= UINT32_MAX);
    CHECK(ciphertext_len <= UINT32_MAX);
    CHECK(manifest_len <= SIZE_MAX - ALRIOS_ARAPP_V2_HEADER_SIZE);
    package_len = ALRIOS_ARAPP_V2_HEADER_SIZE + manifest_len;
    CHECK(ciphertext_len <= SIZE_MAX - package_len);
    package_len += ciphertext_len;
    package = (uint8_t *)calloc(package_len, 1U);

    CHECK(package != NULL);
    memcpy(package, ALRIOS_ARAPP_V2_MAGIC, ALRIOS_ARAPP_V2_MAGIC_SIZE);
    write_u16_be(package + 16U, ALRIOS_ARAPP_V2_VERSION);
    write_u16_be(package + 18U, ALRIOS_ARAPP_V2_ARCH_X86_64);
    write_u32_be(package + 20U,
                 ALRIOS_ARAPP_V2_FLAG_PROFILE_ENTERPRISE |
                 ALRIOS_ARAPP_V2_FLAG_RING_DEVMODE |
                 ALRIOS_ARAPP_V2_FLAG_ENCRYPTED_GCM);
    write_u64_be(package + 24U, 1000U);
    write_u64_be(package + 32U, 2000U);
    write_u32_be(package + 40U, ALRIOS_ARAPP_V2_HEADER_SIZE);
    write_u32_be(package + 44U, (uint32_t)manifest_len);
    write_u32_be(package + 48U, (uint32_t)ciphertext_len);
    memset(package + 52U, 0x11, ALRIOS_ARAPP_V2_GCM_IV_SIZE);
    memset(package + 64U, 0x22, ALRIOS_ARAPP_V2_GCM_TAG_SIZE);
    memset(package + 80U, 0x33, ALRIOS_ARAPP_V2_SHA512_SIZE);
    memset(package + ALRIOS_ARAPP_V2_PREFIX_SIZE,
           0x44, ALRIOS_ARAPP_V2_ED25519_SIGNATURE_SIZE);
    memset(package + ALRIOS_ARAPP_V2_PREFIX_SIZE +
               ALRIOS_ARAPP_V2_ED25519_SIGNATURE_SIZE,
           0x55, ALRIOS_ARAPP_V2_ML_DSA_65_SIGNATURE_SIZE);
    memcpy(package + ALRIOS_ARAPP_V2_HEADER_SIZE, manifest, manifest_len);
    if (ciphertext_len > 0U) {
        memcpy(package + ALRIOS_ARAPP_V2_HEADER_SIZE + manifest_len,
               ciphertext, ciphertext_len);
    }
    *out_len = package_len;
    return package;
}

static void test_valid_package(void) {
    static const char manifest[] =
        "{\"app_id\":\"com.example.app\",\"version\":\"1.0.0\"}";
    static const uint8_t ciphertext[] = {0xdeU, 0xadU, 0xbeU, 0xefU};
    alrios_arapp_v2_view_t view;
    size_t package_len;
    uint8_t *package = make_valid_package(manifest, ciphertext,
                                          sizeof(ciphertext), &package_len);

    CHECK(alrios_arapp_v2_validate(package, package_len, 1500U, 0U,
                                   &view) == ALRIOS_ARAPP_V2_OK);
    CHECK(view.target_arch == ALRIOS_ARAPP_V2_ARCH_X86_64);
    CHECK(view.flags == (ALRIOS_ARAPP_V2_FLAG_PROFILE_ENTERPRISE |
                         ALRIOS_ARAPP_V2_FLAG_RING_DEVMODE |
                         ALRIOS_ARAPP_V2_FLAG_ENCRYPTED_GCM));
    CHECK(view.issued_at == 1000U);
    CHECK(view.expires_at == 2000U);
    CHECK(view.manifest_len == strlen(manifest));
    CHECK(memcmp(view.manifest, manifest, view.manifest_len) == 0);
    CHECK(view.ciphertext_len == sizeof(ciphertext));
    CHECK(memcmp(view.ciphertext, ciphertext, sizeof(ciphertext)) == 0);
    CHECK(view.prefix == package);
    CHECK(view.prefix_len == ALRIOS_ARAPP_V2_PREFIX_SIZE);
    CHECK(view.header_len == ALRIOS_ARAPP_V2_HEADER_SIZE);
    CHECK(view.gcm_iv == package + 52U);
    CHECK(view.gcm_tag == package + 64U);
    CHECK(view.cleartext_sha512 == package + 80U);
    free(package);
}

static void test_layout_failures(void) {
    static const uint8_t ciphertext[] = {1U};
    alrios_arapp_v2_view_t view;
    size_t package_len;
    uint8_t *package = make_valid_package("{\"a\":1}", ciphertext,
                                          sizeof(ciphertext), &package_len);

    memset(&view, 0xa5, sizeof(view));
    CHECK(alrios_arapp_v2_validate(NULL, package_len, 1500U, 0U, &view) ==
          ALRIOS_ARAPP_V2_ERR_ARGUMENT);
    CHECK(view.prefix == NULL);
    CHECK(alrios_arapp_v2_validate(package, package_len, 1500U, 0U, NULL) ==
          ALRIOS_ARAPP_V2_ERR_ARGUMENT);
    CHECK(alrios_arapp_v2_validate(package, SIZE_MAX, 1500U, 0U, &view) ==
          ALRIOS_ARAPP_V2_ERR_LAYOUT);
    CHECK(alrios_arapp_v2_validate(package, ALRIOS_ARAPP_V2_HEADER_SIZE - 1U,
                                   1500U, 0U, &view) ==
          ALRIOS_ARAPP_V2_ERR_TRUNCATED);

    package[0] ^= 1U;
    CHECK(alrios_arapp_v2_validate(package, package_len, 1500U, 0U, &view) ==
          ALRIOS_ARAPP_V2_ERR_MAGIC);
    package[0] ^= 1U;

    write_u32_be(package + 40U, ALRIOS_ARAPP_V2_HEADER_SIZE - 1U);
    CHECK(alrios_arapp_v2_validate(package, package_len, 1500U, 0U, &view) ==
          ALRIOS_ARAPP_V2_ERR_LAYOUT);
    write_u32_be(package + 40U, ALRIOS_ARAPP_V2_HEADER_SIZE);

    write_u32_be(package + 48U, UINT32_MAX);
    CHECK(alrios_arapp_v2_validate(package, package_len, 1500U, 0U, &view) ==
          ALRIOS_ARAPP_V2_ERR_LAYOUT);
    write_u32_be(package + 48U, (uint32_t)sizeof(ciphertext));

    write_u32_be(package + 44U, UINT32_MAX);
    CHECK(alrios_arapp_v2_validate(package, package_len, 1500U, 0U, &view) ==
          ALRIOS_ARAPP_V2_ERR_LAYOUT);
    write_u32_be(package + 44U, 7U);

    CHECK(alrios_arapp_v2_validate(package, package_len - 1U, 1500U, 0U,
                                   &view) == ALRIOS_ARAPP_V2_ERR_LAYOUT);
    CHECK(alrios_arapp_v2_validate(package, package_len + 1U, 1500U, 0U,
                                   &view) == ALRIOS_ARAPP_V2_ERR_LAYOUT);
    free(package);
}

static void test_header_policy_failures(void) {
    alrios_arapp_v2_view_t view;
    size_t package_len;
    uint8_t *package = make_valid_package("{\"a\":1}", NULL, 0U,
                                          &package_len);

    write_u16_be(package + 16U, ALRIOS_ARAPP_V2_VERSION + 1U);
    CHECK(alrios_arapp_v2_validate(package, package_len, 1500U, 0U, &view) ==
          ALRIOS_ARAPP_V2_ERR_VERSION);
    write_u16_be(package + 16U, ALRIOS_ARAPP_V2_VERSION);

    write_u16_be(package + 18U, 0xffffU);
    CHECK(alrios_arapp_v2_validate(package, package_len, 1500U, 0U, &view) ==
          ALRIOS_ARAPP_V2_ERR_ARCH);
    write_u16_be(package + 18U, ALRIOS_ARAPP_V2_ARCH_X86_64);

    write_u32_be(package + 20U, ALRIOS_ARAPP_V2_FLAGS_MASK + 1U);
    CHECK(alrios_arapp_v2_validate(package, package_len, 1500U, 0U, &view) ==
          ALRIOS_ARAPP_V2_ERR_FLAGS);
    write_u32_be(package + 20U,
                 ALRIOS_ARAPP_V2_FLAG_PROFILE_ENTERPRISE |
                 ALRIOS_ARAPP_V2_FLAG_PROFILE_PERFORMANCE |
                 ALRIOS_ARAPP_V2_FLAG_RING_DEVMODE);
    CHECK(alrios_arapp_v2_validate(package, package_len, 1500U, 0U, &view) ==
          ALRIOS_ARAPP_V2_ERR_FLAGS);
    free(package);
}

static void test_manifest_and_padding_failures(void) {
    alrios_arapp_v2_view_t view;
    size_t package_len;
    uint8_t *package = make_valid_package("{ \"b\":2,\"a\":1}", NULL, 0U,
                                          &package_len);

    CHECK(alrios_arapp_v2_validate(package, package_len, 1500U, 0U, &view) ==
          ALRIOS_ARAPP_V2_ERR_MANIFEST);
    free(package);

    package = make_valid_package("{\"b\":2,\"a\":1}", NULL, 0U,
                                 &package_len);
    CHECK(alrios_arapp_v2_validate(package, package_len, 1500U, 0U, &view) ==
          ALRIOS_ARAPP_V2_ERR_MANIFEST);
    free(package);

    package = make_valid_package("{\"a\":1}", NULL, 0U, &package_len);
    package[ALRIOS_ARAPP_V2_HEADER_SIZE + 2U] = 0U;
    CHECK(alrios_arapp_v2_validate(package, package_len, 1500U, 0U, &view) ==
          ALRIOS_ARAPP_V2_ERR_MANIFEST);
    free(package);

    package = make_valid_package("{\"a\":1}", NULL, 0U, &package_len);
    package[ALRIOS_ARAPP_V2_PREFIX_SIZE +
            ALRIOS_ARAPP_V2_ED25519_SIGNATURE_SIZE +
            ALRIOS_ARAPP_V2_ML_DSA_65_SIGNATURE_SIZE] = 1U;
    CHECK(alrios_arapp_v2_validate(package, package_len, 1500U, 0U, &view) ==
          ALRIOS_ARAPP_V2_ERR_PADDING);
    free(package);
}

static void test_time_failures(void) {
    alrios_arapp_v2_view_t view;
    size_t package_len;
    uint8_t *package = make_valid_package("{\"a\":1}", NULL, 0U,
                                          &package_len);

    write_u64_be(package + 24U, 1801U);
    CHECK(alrios_arapp_v2_validate(package, package_len, 1500U, 300U, &view) ==
          ALRIOS_ARAPP_V2_ERR_TIME);
    write_u64_be(package + 24U, 1800U);
    CHECK(alrios_arapp_v2_validate(package, package_len, 1500U, 300U, &view) ==
          ALRIOS_ARAPP_V2_OK);

    write_u64_be(package + 24U, 1000U);
    write_u64_be(package + 32U, 1499U);
    CHECK(alrios_arapp_v2_validate(package, package_len, 1500U, 300U, &view) ==
          ALRIOS_ARAPP_V2_ERR_TIME);
    write_u64_be(package + 32U, 1500U);
    CHECK(alrios_arapp_v2_validate(package, package_len, 1500U, 0U, &view) ==
          ALRIOS_ARAPP_V2_OK);

    write_u64_be(package + 24U, 1500U);
    write_u64_be(package + 32U, 1500U);
    CHECK(alrios_arapp_v2_validate(package, package_len, 1500U, 0U, &view) ==
          ALRIOS_ARAPP_V2_ERR_TIME);

    write_u64_be(package + 24U, UINT64_MAX - 1U);
    write_u64_be(package + 32U, UINT64_MAX);
    CHECK(alrios_arapp_v2_validate(package, package_len, UINT64_MAX - 2U,
                                   UINT64_MAX, &view) == ALRIOS_ARAPP_V2_OK);
    free(package);
}

static void test_sovereign_policy(void) {
    alrios_arapp_v2_view_t view;
    size_t package_len;
    uint8_t *package = make_valid_package("{\"a\":1}", NULL, 0U,
                                          &package_len);

    write_u32_be(package + 20U,
                 ALRIOS_ARAPP_V2_FLAG_PROFILE_SOVEREIGN |
                 ALRIOS_ARAPP_V2_FLAG_RING_SOVEREIGN);
    CHECK(alrios_arapp_v2_validate(package, package_len, 1500U, 0U, &view) ==
          ALRIOS_ARAPP_V2_ERR_FLAGS);
    write_u32_be(package + 20U,
                 ALRIOS_ARAPP_V2_FLAG_PROFILE_SOVEREIGN |
                 ALRIOS_ARAPP_V2_FLAG_RING_SOVEREIGN |
                 ALRIOS_ARAPP_V2_FLAG_PQC_HYBRID_SIG |
                 ALRIOS_ARAPP_V2_FLAG_ENCRYPTED_GCM);
    CHECK(alrios_arapp_v2_validate(package, package_len, 1500U, 0U, &view) ==
          ALRIOS_ARAPP_V2_OK);
    free(package);
}

int main(void) {
    test_valid_package();
    test_layout_failures();
    test_header_policy_failures();
    test_manifest_and_padding_failures();
    test_time_failures();
    test_sovereign_policy();
    (void)puts("test_arapp_v2_reader: PASS");
    return EXIT_SUCCESS;
}
