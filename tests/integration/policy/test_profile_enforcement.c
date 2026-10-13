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

static alrios_app_requirements_t signed_app(void) {
    alrios_app_requirements_t app_req;

    (void)memset(&app_req, 0, sizeof(app_req));
    app_req.required_capabilities = ALRIOS_CRYPTO_CAP_ED25519;
    app_req.package_signed = 1;
    app_req.certificate_chain_valid = 1;
    app_req.ed25519_signature_present = 1;
    app_req.ed25519_signature_valid = 1;
    return app_req;
}

static alrios_app_requirements_t hybrid_app(void) {
    alrios_app_requirements_t app_req = signed_app();

    app_req.container_flags = ALRIOS_ARAPP_V2_FLAG_RING_SOVEREIGN |
                              ALRIOS_ARAPP_V2_FLAG_PQC_HYBRID_SIG;
    app_req.required_capabilities |= ALRIOS_CRYPTO_CAP_ML_DSA_65;
    app_req.require_pqc = 1;
    app_req.ml_dsa_signature_present = 1;
    app_req.ml_dsa_signature_valid = 1;
    return app_req;
}

static void test_sovereign_rejects_unsigned(void) {
    alrios_supervisor_policy_t *policy = NULL;
    alrios_app_requirements_t app_req;

    CHECK(alrios_supervisor_policy_startup(
              &policy, PROFILE_SOVEREIGN_MAX) == ALRIOS_POLICY_OK);
    (void)memset(&app_req, 0, sizeof(app_req));
    CHECK(alrios_supervisor_policy_evaluate_launch(
              policy, &app_req, NULL, 0U) == ALRIOS_POLICY_ERR_SIGNATURE);
    alrios_supervisor_policy_shutdown(policy);
}

static void test_sovereign_rejects_ed25519_only(void) {
    alrios_supervisor_policy_t *policy = NULL;
    alrios_app_requirements_t app_req = signed_app();

    app_req.container_flags = ALRIOS_ARAPP_V2_FLAG_RING_SOVEREIGN;
    CHECK(alrios_supervisor_policy_startup(
              &policy, PROFILE_SOVEREIGN_MAX) == ALRIOS_POLICY_OK);
    CHECK(alrios_supervisor_policy_evaluate_launch(
              policy, &app_req, NULL, 0U) == ALRIOS_POLICY_ERR_SIGNATURE);
    alrios_supervisor_policy_shutdown(policy);
}

static void test_sovereign_accepts_valid_hybrid(void) {
    alrios_supervisor_policy_t *policy = NULL;
    alrios_app_requirements_t app_req = hybrid_app();

    CHECK(alrios_supervisor_policy_startup(
              &policy, PROFILE_SOVEREIGN_MAX) == ALRIOS_POLICY_OK);
    CHECK(alrios_supervisor_policy_evaluate_launch(
              policy, &app_req, NULL, 0U) == ALRIOS_POLICY_OK);

    app_req.ml_dsa_signature_valid = 0;
    CHECK(alrios_supervisor_policy_evaluate_launch(
              policy, &app_req, NULL, 0U) == ALRIOS_POLICY_ERR_SIGNATURE);
    app_req.ml_dsa_signature_valid = 1;
    app_req.certificate_chain_valid = 0;
    CHECK(alrios_supervisor_policy_evaluate_launch(
              policy, &app_req, NULL, 0U) == ALRIOS_POLICY_ERR_CERTIFICATE);
    alrios_supervisor_policy_shutdown(policy);
}

static void test_standard_profile_enforcement(void) {
    alrios_supervisor_policy_t *policy = NULL;
    alrios_app_requirements_t app_req;

    CHECK(alrios_supervisor_policy_startup(
              &policy, PROFILE_ENTERPRISE_BAL) == ALRIOS_POLICY_OK);

    app_req = signed_app();
    app_req.container_flags = ALRIOS_ARAPP_V2_FLAG_PROFILE_ENTERPRISE;
    CHECK(alrios_supervisor_policy_evaluate_launch(
              policy, &app_req, NULL, 0U) == ALRIOS_POLICY_OK);

    (void)memset(&app_req, 0, sizeof(app_req));
    CHECK(alrios_supervisor_policy_evaluate_launch(
              policy, &app_req, NULL, 0U) == ALRIOS_POLICY_ERR_SIGNATURE);

    app_req.container_flags = ALRIOS_ARAPP_V2_FLAG_RING_DEVMODE;
    CHECK(alrios_supervisor_policy_evaluate_launch(
              policy, &app_req, NULL, 0U) == ALRIOS_POLICY_ERR_REJECTED);
    app_req.developer_mode_verified = 1;
    CHECK(alrios_supervisor_policy_evaluate_launch(
              policy, &app_req, NULL, 0U) == ALRIOS_POLICY_OK);

    app_req = signed_app();
    app_req.ed25519_signature_valid = 0;
    CHECK(alrios_supervisor_policy_evaluate_launch(
              policy, &app_req, NULL, 0U) == ALRIOS_POLICY_ERR_SIGNATURE);
    alrios_supervisor_policy_shutdown(policy);
}

static void test_profile_is_immutable_after_startup(void) {
    alrios_supervisor_policy_t *policy = NULL;
    alrios_profile_t profile = PROFILE_EDGE_PERF;
    alrios_app_requirements_t app_req = hybrid_app();

    CHECK(alrios_supervisor_policy_startup(
              &policy, PROFILE_SOVEREIGN_MAX) == ALRIOS_POLICY_OK);
    CHECK(alrios_supervisor_policy_set_profile(
              policy, PROFILE_ENTERPRISE_BAL) == ALRIOS_POLICY_ERR_DOWNGRADE);
    CHECK(alrios_supervisor_policy_set_profile(
              policy, PROFILE_SOVEREIGN_MAX) == ALRIOS_POLICY_ERR_IMMUTABLE);
    CHECK(alrios_supervisor_policy_get_profile(policy, &profile) ==
          ALRIOS_POLICY_OK);
    CHECK(profile == PROFILE_SOVEREIGN_MAX);
    CHECK(alrios_supervisor_policy_evaluate_launch(
              policy, &app_req, NULL, 0U) == ALRIOS_POLICY_OK);
    alrios_supervisor_policy_shutdown(policy);
}

int main(void) {
    if (!alrios_crypto_has_capability(ALRIOS_CRYPTO_CAP_ML_DSA_65)) {
        (void)fprintf(stderr,
                      "test_profile_enforcement: ML-DSA-65 unavailable\n");
        return EXIT_FAILURE;
    }

    test_sovereign_rejects_unsigned();
    test_sovereign_rejects_ed25519_only();
    test_sovereign_accepts_valid_hybrid();
    test_standard_profile_enforcement();
    test_profile_is_immutable_after_startup();
    (void)puts("test_profile_enforcement: PASS");
    return EXIT_SUCCESS;
}
