/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/pki/certificate.h"

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

static void make_certificate(alrios_certificate_t *certificate, uint8_t key_seed) {
    size_t index;
    static const uint8_t subject[] = "com.alrigroup.root";

    memset(certificate, 0, sizeof(*certificate));
    certificate->version = ALRIOS_CERTIFICATE_VERSION_1;
    certificate->role = ALRIOS_CERTIFICATE_ROLE_ROOT;
    certificate->public_key_algorithm = ALRIOS_PUBLIC_KEY_ALGORITHM_ED25519;
    certificate->signature_algorithm = ALRIOS_SIGNATURE_ALGORITHM_HYBRID_ED25519_ML_DSA_65;
    certificate->serial[ALRIOS_CERTIFICATE_SERIAL_SIZE - 1U] = key_seed;
    certificate->not_before = UINT64_C(0x0102030405060708);
    certificate->not_after = UINT64_C(0x1112131415161718);
    certificate->subject_len = sizeof(subject) - 1U;
    memcpy(certificate->subject, subject, sizeof(subject) - 1U);
    certificate->public_key_len = ALRIOS_CERTIFICATE_ED25519_KEY_SIZE;
    certificate->signature_len = ALRIOS_CERTIFICATE_HYBRID_SIG_SIZE;
    for (index = 0U; index < certificate->public_key_len; ++index) {
        certificate->public_key[index] = (uint8_t)(key_seed + index);
    }
    for (index = 0U; index < certificate->signature_len; ++index) {
        certificate->signature[index] = (uint8_t)(0xa0U + index);
    }
}

static void test_canonical_encoding(void) {
    alrios_certificate_t certificate;
    alrios_certificate_t decoded;
    uint8_t wire[ALRIOS_CERTIFICATE_WIRE_MAX];
    uint8_t roundtrip[ALRIOS_CERTIFICATE_WIRE_MAX];
    size_t wire_len = 0U;
    size_t roundtrip_len = 0U;

    make_certificate(&certificate, 1U);
    CHECK(alrios_certificate_encode(&certificate, wire, sizeof(wire), &wire_len) ==
          ALRIOS_CERTIFICATE_OK);
    CHECK(wire_len == ALRIOS_CERTIFICATE_HEADER_SIZE_V1 + certificate.subject_len +
                      certificate.public_key_len + certificate.signature_len);
    CHECK(memcmp(wire, "ALRICERT", 8U) == 0);
    CHECK(wire[8U] == 0U && wire[9U] == 1U);
    CHECK(wire[10U] == ALRIOS_CERTIFICATE_ROLE_ROOT);
    CHECK(wire[11U] == ALRIOS_PUBLIC_KEY_ALGORITHM_ED25519);
    CHECK(wire[12U] == ALRIOS_SIGNATURE_ALGORITHM_HYBRID_ED25519_ML_DSA_65);
    CHECK(wire[13U] == 0U && wire[14U] == 0U && wire[15U] == 0U);
    CHECK(wire[16U] == 0U && wire[17U] == 0U);
    CHECK(wire[20U] == 0U && wire[21U] == certificate.subject_len);
    CHECK(wire[22U] == 0U && wire[23U] == ALRIOS_CERTIFICATE_ED25519_KEY_SIZE);
    CHECK(wire[24U] == (uint8_t)(ALRIOS_CERTIFICATE_HYBRID_SIG_SIZE >> 8U));
    CHECK(wire[25U] == (uint8_t)ALRIOS_CERTIFICATE_HYBRID_SIG_SIZE);
    CHECK(wire[76U] == 0x01U && wire[83U] == 0x08U);
    CHECK(wire[84U] == 0x11U && wire[91U] == 0x18U);

    CHECK(alrios_certificate_decode(wire, wire_len, &decoded) == ALRIOS_CERTIFICATE_OK);
    CHECK(decoded.version == certificate.version);
    CHECK(decoded.subject_len == certificate.subject_len);
    CHECK(memcmp(decoded.subject, certificate.subject, certificate.subject_len) == 0);
    CHECK(memcmp(decoded.public_key, certificate.public_key, certificate.public_key_len) == 0);
    CHECK(memcmp(decoded.signature, certificate.signature, certificate.signature_len) == 0);
    CHECK(alrios_certificate_encode(&decoded, roundtrip, sizeof(roundtrip), &roundtrip_len) ==
          ALRIOS_CERTIFICATE_OK);
    CHECK(roundtrip_len == wire_len);
    CHECK(memcmp(roundtrip, wire, wire_len) == 0);
}

static void test_strict_lengths_and_fields(void) {
    alrios_certificate_t certificate;
    alrios_certificate_t decoded;
    uint8_t wire[ALRIOS_CERTIFICATE_WIRE_MAX + 1U];
    uint8_t mutated[ALRIOS_CERTIFICATE_WIRE_MAX + 1U];
    size_t wire_len = 0U;
    size_t index;

    make_certificate(&certificate, 2U);
    CHECK(alrios_certificate_encode(&certificate, wire, sizeof(wire), &wire_len) ==
          ALRIOS_CERTIFICATE_OK);
    for (index = 0U; index < wire_len; ++index) {
        CHECK(alrios_certificate_decode(wire, index, &decoded) != ALRIOS_CERTIFICATE_OK);
    }

    memcpy(mutated, wire, wire_len);
    mutated[wire_len] = 0U;
    CHECK(alrios_certificate_decode(mutated, wire_len + 1U, &decoded) ==
          ALRIOS_CERTIFICATE_ERR_LENGTH);

    memcpy(mutated, wire, wire_len);
    mutated[13U] = 1U;
    CHECK(alrios_certificate_decode(mutated, wire_len, &decoded) ==
          ALRIOS_CERTIFICATE_ERR_NONCANONICAL);

    memcpy(mutated, wire, wire_len);
    mutated[20U] = 0U;
    mutated[21U] = 0U;
    CHECK(alrios_certificate_decode(mutated, wire_len, &decoded) ==
          ALRIOS_CERTIFICATE_ERR_LENGTH);

    certificate.public_key_len--;
    CHECK(alrios_certificate_encode(&certificate, wire, sizeof(wire), &wire_len) ==
          ALRIOS_CERTIFICATE_ERR_LENGTH);
    CHECK(wire_len == 0U);
}

static void test_tbs_and_overlap(void) {
    alrios_certificate_t certificate;
    uint8_t tbs[ALRIOS_CERTIFICATE_TBS_MAX];
    size_t tbs_len = 0U;
    size_t wire_len = 99U;

    make_certificate(&certificate, 6U);
    CHECK(alrios_certificate_encode_tbs(&certificate, tbs, sizeof(tbs), &tbs_len) ==
          ALRIOS_CERTIFICATE_OK);
    CHECK(tbs_len == ALRIOS_CERTIFICATE_TBS_DOMAIN_SIZE +
                     ALRIOS_CERTIFICATE_HEADER_SIZE_V1 + certificate.subject_len +
                     certificate.public_key_len);
    CHECK(memcmp(tbs, "ALRIOS-CERT-TBS-V1", ALRIOS_CERTIFICATE_TBS_DOMAIN_SIZE) == 0);
    CHECK(alrios_certificate_encode(&certificate, (uint8_t *)&certificate,
                                    sizeof(certificate), &wire_len) ==
          ALRIOS_CERTIFICATE_ERR_OVERLAP);
    CHECK(wire_len == 0U);
}

static void test_unknown_algorithms_rejected(void) {
    alrios_certificate_t certificate;
    alrios_certificate_t decoded;
    uint8_t wire[ALRIOS_CERTIFICATE_WIRE_MAX];
    size_t wire_len = 0U;

    make_certificate(&certificate, 3U);
    certificate.public_key_algorithm = 0xffU;
    CHECK(alrios_certificate_encode(&certificate, wire, sizeof(wire), &wire_len) ==
          ALRIOS_CERTIFICATE_ERR_ALGORITHM);

    make_certificate(&certificate, 3U);
    certificate.signature_algorithm = 0xffU;
    CHECK(alrios_certificate_encode(&certificate, wire, sizeof(wire), &wire_len) ==
          ALRIOS_CERTIFICATE_ERR_ALGORITHM);

    make_certificate(&certificate, 3U);
    CHECK(alrios_certificate_encode(&certificate, wire, sizeof(wire), &wire_len) ==
          ALRIOS_CERTIFICATE_OK);
    wire[11U] = 0xffU;
    CHECK(alrios_certificate_decode(wire, wire_len, &decoded) ==
          ALRIOS_CERTIFICATE_ERR_ALGORITHM);
    CHECK(decoded.version == 0U);
    wire[11U] = ALRIOS_PUBLIC_KEY_ALGORITHM_ED25519;
    wire[12U] = 0xffU;
    CHECK(alrios_certificate_decode(wire, wire_len, &decoded) ==
          ALRIOS_CERTIFICATE_ERR_ALGORITHM);
}

static void test_trust_store(void) {
    alrios_trust_store_t store;
    alrios_certificate_t root;
    alrios_certificate_t second_root;
    const alrios_certificate_t *found = NULL;
    uint8_t key_id[ALRIOS_CERTIFICATE_KEY_ID_SIZE];

    make_certificate(&root, 4U);
    make_certificate(&second_root, 5U);
    alrios_trust_store_init(&store);
    store.anchor_count = ALRIOS_TRUST_STORE_MAX_ANCHORS + 1U;
    CHECK(alrios_trust_store_lookup(&store, root.public_key, &found) ==
          ALRIOS_CERTIFICATE_ERR_STATE);
    alrios_trust_store_init(&store);
    CHECK(alrios_trust_store_lookup(&store, root.public_key, &found) ==
          ALRIOS_CERTIFICATE_ERR_SEALED);
    CHECK(found == NULL);
    CHECK(alrios_trust_store_add_anchor(&store, &root) == ALRIOS_CERTIFICATE_OK);
    CHECK(alrios_trust_store_add_anchor(&store, &root) == ALRIOS_CERTIFICATE_ERR_DUPLICATE);
    CHECK(alrios_trust_store_add_anchor(&store, &second_root) == ALRIOS_CERTIFICATE_OK);
    CHECK(alrios_trust_store_seal(&store) == ALRIOS_CERTIFICATE_OK);
    CHECK(alrios_trust_store_add_anchor(&store, &root) == ALRIOS_CERTIFICATE_ERR_SEALED);
    CHECK(alrios_certificate_public_key_id(&root, key_id) == ALRIOS_CERTIFICATE_OK);
    CHECK(alrios_trust_store_lookup(&store, key_id, &found) == ALRIOS_CERTIFICATE_OK);
    CHECK(found != NULL);
    CHECK(found->serial[ALRIOS_CERTIFICATE_SERIAL_SIZE - 1U] == 4U);
    key_id[0] ^= 0xffU;
    CHECK(alrios_trust_store_lookup(&store, key_id, &found) ==
          ALRIOS_CERTIFICATE_ERR_NOT_FOUND);
    CHECK(found == NULL);
    alrios_trust_store_clear(&store);
    CHECK(store.anchor_count == 0U && store.sealed == 0U);
}

int main(void) {
    test_canonical_encoding();
    test_strict_lengths_and_fields();
    test_tbs_and_overlap();
    test_unknown_algorithms_rejected();
    test_trust_store();
    (void)printf("MP-001 (Certificate Wire Format): PASS\n");
    return EXIT_SUCCESS;
}
