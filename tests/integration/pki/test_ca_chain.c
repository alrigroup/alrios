/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/pki/issuer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("[TEST] Testing PKI Chain Generation & Trust-Store Verification...\n");

    /* 1. Issue Root CA */
    EVP_PKEY *root_pkey = NULL;
    alrios_certificate_t root_cert;
    uint8_t root_wire[ALRIOS_CERTIFICATE_WIRE_MAX];
    size_t root_wire_len = 0;

    assert(alrios_pki_issue_root("alrios.root.ca", 365, &root_pkey,
                                 &root_cert, root_wire, sizeof(root_wire), &root_wire_len) == 0);
    assert(root_pkey != NULL);
    assert(root_wire_len > 0);

    /* 2. Setup Trust Store with Root */
    alrios_trust_store_t store;
    alrios_trust_store_init(&store);
    assert(alrios_trust_store_add_anchor(&store, &root_cert) == 0);
    assert(alrios_trust_store_seal(&store) == 0);

    /* 3. Issue Intermediate CA */
    EVP_PKEY *inter_pkey = NULL;
    alrios_certificate_t inter_cert;
    uint8_t inter_wire[ALRIOS_CERTIFICATE_WIRE_MAX];
    size_t inter_wire_len = 0;

    assert(alrios_pki_issue_intermediate("alrios.release.intermediate", 90,
                                         &root_cert, root_pkey, &inter_pkey,
                                         &inter_cert, inter_wire, sizeof(inter_wire), &inter_wire_len) == 0);
    assert(inter_pkey != NULL);

    /* 4. Issue Leaf Application Certificate */
    EVP_PKEY *leaf_pkey = NULL;
    alrios_certificate_t leaf_cert;
    uint8_t leaf_wire[ALRIOS_CERTIFICATE_WIRE_MAX];
    size_t leaf_wire_len = 0;

    assert(alrios_pki_issue_leaf("com.alrigroup.core.gateway", 30,
                                 &inter_cert, inter_pkey, &leaf_pkey,
                                 &leaf_cert, leaf_wire, sizeof(leaf_wire), &leaf_wire_len) == 0);
    assert(leaf_pkey != NULL);

    /* 5. Validate Chain (Positive case) */
    uint64_t current_time = leaf_cert.not_before + 100ULL;
    assert(alrios_pki_verify_chain(&store, &leaf_cert, &inter_cert, current_time) == ALRIOS_ISSUER_OK);

    /* 6. Negative Tests */
    /* Expired certificate */
    assert(alrios_pki_verify_chain(&store, &leaf_cert, &inter_cert, leaf_cert.not_after + 100ULL) == ALRIOS_ISSUER_ERR_EXPIRED);

    /* Corrupted Leaf signature */
    alrios_certificate_t bad_leaf = leaf_cert;
    bad_leaf.signature[10] ^= 0xFF;
    assert(alrios_pki_verify_chain(&store, &bad_leaf, &inter_cert, current_time) == ALRIOS_ISSUER_ERR_VERIFY_FAILED);

    /* Corrupted Intermediate signature */
    alrios_certificate_t bad_inter = inter_cert;
    bad_inter.signature[10] ^= 0xFF;
    assert(alrios_pki_verify_chain(&store, &leaf_cert, &bad_inter, current_time) == ALRIOS_ISSUER_ERR_VERIFY_FAILED);

    /* Role violation */
    alrios_certificate_t rogue_leaf = leaf_cert;
    rogue_leaf.role = ALRIOS_CERTIFICATE_ROLE_INTERMEDIATE;
    assert(alrios_pki_verify_chain(&store, &rogue_leaf, &inter_cert, current_time) == ALRIOS_ISSUER_ERR_CHAIN_INVALID);

    EVP_PKEY_free(root_pkey);
    EVP_PKEY_free(inter_pkey);
    EVP_PKEY_free(leaf_pkey);
    alrios_trust_store_clear(&store);

    printf("TASK-MP-002: PASS (Root -> Intermediate -> Leaf verified)\n");
    return 0;
}
