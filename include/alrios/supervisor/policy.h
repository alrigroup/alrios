/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_SUPERVISOR_POLICY_H
#define ALRIOS_SUPERVISOR_POLICY_H

#include "alrios/profiles.h"
#include "alrios/pki/certificate.h"

#include <stddef.h>
#include <stdint.h>

#define ALRIOS_POLICY_OK                    0
#define ALRIOS_POLICY_ERR_ARGUMENT         -3501
#define ALRIOS_POLICY_ERR_REJECTED         -3502
#define ALRIOS_POLICY_ERR_DOWNGRADE        -3503
#define ALRIOS_POLICY_ERR_CAPABILITY       -3504
#define ALRIOS_POLICY_ERR_SIGNATURE        -3505
#define ALRIOS_POLICY_ERR_CERTIFICATE      -3506
#define ALRIOS_POLICY_ERR_IMMUTABLE        -3507
#define ALRIOS_POLICY_ERR_STATE            -3508
#define ALRIOS_POLICY_ERR_MEMORY           -3509
#define ALRIOS_POLICY_ERR_IO               -3510
#define ALRIOS_POLICY_ERR_FORMAT           -3511
#define ALRIOS_POLICY_ERR_UNSUPPORTED      -3512

#ifdef __cplusplus
extern "C" {
#endif

typedef struct alrios_app_requirements {
    uint32_t container_flags;
    uint32_t required_capabilities;
    int require_pqc;
    int package_signed;
    int certificate_chain_valid;
    int ed25519_signature_present;
    int ed25519_signature_valid;
    int ml_dsa_signature_present;
    int ml_dsa_signature_valid;
    int developer_mode_verified;
} alrios_app_requirements_t;

/*
 * Opaque boot-time policy. Startup seals a canonical profile immediately;
 * callers must pass verification results produced by the package/PKI layers.
 */
typedef struct alrios_supervisor_policy alrios_supervisor_policy_t;

int alrios_supervisor_policy_startup(
    alrios_supervisor_policy_t **out_policy,
    alrios_profile_t profile
);

int alrios_supervisor_policy_startup_with_trust(
    alrios_supervisor_policy_t **out_policy,
    alrios_profile_t profile,
    const char *trust_directory
);

void alrios_supervisor_policy_shutdown(alrios_supervisor_policy_t *policy);

int alrios_supervisor_policy_set_profile(
    alrios_supervisor_policy_t *policy,
    alrios_profile_t profile
);

int alrios_supervisor_policy_get_profile(
    const alrios_supervisor_policy_t *policy,
    alrios_profile_t *out_profile
);

int alrios_supervisor_policy_evaluate_launch(
    const alrios_supervisor_policy_t *policy,
    const alrios_app_requirements_t *app_req,
    char *missing_caps_buf,
    size_t missing_caps_len
);

/*
 * Verifies a package using the supervisor-owned immutable trust configuration,
 * derives launch requirements from those verification results, then enforces
 * the sealed profile. Legacy v1 packages are treated as unsigned.
 */
int alrios_supervisor_policy_evaluate_package(
    const alrios_supervisor_policy_t *policy,
    const char *package_path,
    uint64_t now,
    char *reason_buf,
    size_t reason_len
);

/* Evaluates a canonical node policy; startup code should use the sealed API. */
int alrios_supervisor_evaluate_policy(
    const alrios_node_policy_t *node_policy,
    const alrios_app_requirements_t *app_req,
    char *missing_caps_buf,
    size_t missing_caps_len
);

#ifdef __cplusplus
}
#endif

#endif /* ALRIOS_SUPERVISOR_POLICY_H */
