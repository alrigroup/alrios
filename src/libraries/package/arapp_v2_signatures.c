/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/package/arapp_v2.h"
#include "alrios/pki/certificate.h"
#include "alrios/pki/issuer.h"
#include "alrios/crypto_verify.h"
#include <openssl/evp.h>
#include <stdlib.h>
#include <string.h>

int arapp_v2_sign(arapp_v2_package_t *pkg,
                  EVP_PKEY *ed25519_privkey,
                  const uint8_t *ml_dsa_65_privkey,
                  arapp_v2_signatures_t *out_sigs) {
    uint8_t digest[ARAPP_V2_SHA512_LEN];
    EVP_MD_CTX *md_ctx = NULL;
    size_t sig_len = ARAPP_V2_ED25519_SIG_LEN;
    int rc;

    if (!pkg || !ed25519_privkey || !out_sigs) {
        return ARAPP_V2_ERR_INVALID_ARGUMENT;
    }

    int requires_pqc = (pkg->container_flags & (ARAPP_V2_FLAG_PROFILE_SOVEREIGN | ARAPP_V2_FLAG_PQC_HYBRID_SIG)) != 0;
    if (requires_pqc && !ml_dsa_65_privkey) {
        return ARAPP_V2_ERR_SIGNATURE_INVALID;
    }

    rc = arapp_v2_compute_digest(pkg, digest);
    if (rc != ARAPP_V2_OK) {
        return rc;
    }

    memset(out_sigs, 0, sizeof(*out_sigs));

    md_ctx = EVP_MD_CTX_new();
    if (!md_ctx) {
        return ARAPP_V2_ERR_MEMORY;
    }

    if (EVP_DigestSignInit(md_ctx, NULL, NULL, NULL, ed25519_privkey) != 1) {
        EVP_MD_CTX_free(md_ctx);
        return ARAPP_V2_ERR_SIGNATURE_INVALID;
    }

    if (EVP_DigestSign(md_ctx, out_sigs->sig_ed25519, &sig_len, digest, sizeof(digest)) != 1 ||
        sig_len != ARAPP_V2_ED25519_SIG_LEN) {
        EVP_MD_CTX_free(md_ctx);
        return ARAPP_V2_ERR_SIGNATURE_INVALID;
    }
    EVP_MD_CTX_free(md_ctx);

    if (requires_pqc && ml_dsa_65_privkey) {
        if (!alrios_ml_dsa_65_provider_available()) {
            return ARAPP_V2_ERR_SIGNATURE_INVALID;
        }
        if (alrios_ml_dsa_65_sign(ml_dsa_65_privkey, digest, out_sigs->sig_ml_dsa_65) != ALRIOS_CRYPTO_OK) {
            return ARAPP_V2_ERR_SIGNATURE_INVALID;
        }
    }

    return ARAPP_V2_OK;
}

int arapp_v2_verify_package_with_chain(const uint8_t *buffer, size_t size,
                                       const alrios_certificate_t *leaf_cert,
                                       const alrios_certificate_t *inter_cert,
                                       const alrios_trust_store_t *store,
                                       const uint8_t *ml_dsa_65_pubkey,
                                       uint64_t current_time,
                                       arapp_v2_package_t *out_pkg) {
    int rc;

    if (!buffer || !leaf_cert || !inter_cert || !store || !out_pkg) {
        return ARAPP_V2_ERR_INVALID_ARGUMENT;
    }

    rc = arapp_v2_read_and_validate(buffer, size, current_time, out_pkg);
    if (rc != ARAPP_V2_OK) {
        return rc;
    }

    rc = alrios_pki_verify_chain(store, leaf_cert, inter_cert, current_time);
    if (rc != ALRIOS_ISSUER_OK) {
        return ARAPP_V2_ERR_SIGNATURE_INVALID;
    }

    if (leaf_cert->public_key_algorithm != ALRIOS_PUBLIC_KEY_ALGORITHM_ED25519 ||
        leaf_cert->public_key_len < ED25519_PUBLIC_KEY_LEN) {
        return ARAPP_V2_ERR_SIGNATURE_INVALID;
    }

    const uint8_t *ed_pub = leaf_cert->public_key;

    int sovereign = (out_pkg->container_flags & (ARAPP_V2_FLAG_PROFILE_SOVEREIGN | ARAPP_V2_FLAG_PQC_HYBRID_SIG)) != 0;
    if (sovereign && !ml_dsa_65_pubkey) {
        return ARAPP_V2_ERR_SIGNATURE_INVALID;
    }

    rc = arapp_v2_verify_signatures(out_pkg, ed_pub, ml_dsa_65_pubkey);
    if (rc != ARAPP_V2_OK) {
        return rc;
    }

    return ARAPP_V2_OK;
}
