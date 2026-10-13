/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/supervisor/policy.h"
#include "alrios/package/arapp_v2.h"
#include "alrios/crypto_verify.h"

#include <string.h>
#include <stdio.h>

int alrios_supervisor_evaluate_policy(
    const alrios_node_policy_t *node_policy,
    const alrios_app_requirements_t *app_req,
    char *missing_caps_buf,
    size_t missing_caps_len
) {
    uint32_t active_caps = 0U;

    if (!node_policy || !app_req) {
        return ALRIOS_POLICY_ERR_ARGUMENT;
    }
    if (missing_caps_buf && missing_caps_len > 0U) {
        missing_caps_buf[0] = '\0';
    }

    /* Check cryptographic capabilities */
    if (alrios_crypto_has_capability(ALRIOS_CRYPTO_CAP_ED25519)) {
        active_caps |= ALRIOS_CRYPTO_CAP_ED25519;
    }
    if (alrios_crypto_has_capability(ALRIOS_CRYPTO_CAP_AES_256_GCM)) {
        active_caps |= ALRIOS_CRYPTO_CAP_AES_256_GCM;
    }
    if (alrios_crypto_has_capability(ALRIOS_CRYPTO_CAP_ML_DSA_65)) {
        active_caps |= ALRIOS_CRYPTO_CAP_ML_DSA_65;
    }
    if (alrios_crypto_has_capability(ALRIOS_CRYPTO_CAP_ML_KEM_768)) {
        active_caps |= ALRIOS_CRYPTO_CAP_ML_KEM_768;
    }

    /* Verify node capabilities satisfy app requirements */
    if ((app_req->required_capabilities & ~active_caps) != 0U) {
        uint32_t missing = app_req->required_capabilities & ~active_caps;
        if (missing_caps_buf && missing_caps_len > 0U) {
            snprintf(missing_caps_buf, missing_caps_len,
                     "missing hardware/crypto capabilities mask: 0x%08x", missing);
        }
        return ALRIOS_POLICY_ERR_CAPABILITY;
    }

    /* Profile Enforcement: Sovereign tier requirements */
    if (node_policy->profile == PROFILE_SOVEREIGN_MAX) {
        /* Sovereign node cannot run non-Sovereign/DevMode or non-PQC apps */
        if ((app_req->container_flags & ALRIOS_ARAPP_V2_FLAG_RING_DEVMODE) != 0U) {
            return ALRIOS_POLICY_ERR_REJECTED;
        }
        if (!app_req->require_pqc && node_policy->require_pqc) {
            return ALRIOS_POLICY_ERR_DOWNGRADE;
        }
    } else if (node_policy->profile == PROFILE_ENTERPRISE_BAL) {
        /* Enterprise tier cannot run Sovereign-only restricted rings if policy prohibits */
        if ((app_req->container_flags & ALRIOS_ARAPP_V2_FLAG_RING_SOVEREIGN) != 0U &&
            node_policy->seccomp_strict == 0) {
            return ALRIOS_POLICY_ERR_REJECTED;
        }
    }

    return ALRIOS_POLICY_OK;
}
