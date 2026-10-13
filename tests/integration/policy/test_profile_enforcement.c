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

static void test_sovereign_tier_enforcement(void) {
    alrios_node_policy_t sovereign_node;
    alrios_app_requirements_t app_req;
    char missing[128];
    int status;

    CHECK(alrios_profile_init(&sovereign_node, PROFILE_SOVEREIGN_MAX) == 0);

    /* Test 1: DevMode app on Sovereign node -> REJECTED */
    memset(&app_req, 0, sizeof(app_req));
    app_req.container_flags = ALRIOS_ARAPP_V2_FLAG_RING_DEVMODE;
    app_req.require_pqc = 1;
    app_req.required_capabilities = ALRIOS_CRYPTO_CAP_ED25519;

    status = alrios_supervisor_evaluate_policy(
        &sovereign_node, &app_req, missing, sizeof(missing)
    );
    CHECK(status == ALRIOS_POLICY_ERR_REJECTED);

    /* Test 2: Sovereign app without PQC requirement on PQC node -> DOWNGRADE */
    memset(&app_req, 0, sizeof(app_req));
    app_req.container_flags = ALRIOS_ARAPP_V2_FLAG_RING_SOVEREIGN;
    app_req.require_pqc = 0;
    app_req.required_capabilities = ALRIOS_CRYPTO_CAP_ED25519;

    status = alrios_supervisor_evaluate_policy(
        &sovereign_node, &app_req, missing, sizeof(missing)
    );
    CHECK(status == ALRIOS_POLICY_ERR_DOWNGRADE);

    /* Test 3: Valid Sovereign app -> OK */
    app_req.require_pqc = 1;
    app_req.required_capabilities = ALRIOS_CRYPTO_CAP_ED25519 | ALRIOS_CRYPTO_CAP_AES_256_GCM;
    status = alrios_supervisor_evaluate_policy(
        &sovereign_node, &app_req, missing, sizeof(missing)
    );
    CHECK(status == ALRIOS_POLICY_OK || status == ALRIOS_POLICY_ERR_CAPABILITY);
}

int main(void) {
    test_sovereign_tier_enforcement();
    (void)puts("test_profile_enforcement: PASS");
    return EXIT_SUCCESS;
}
