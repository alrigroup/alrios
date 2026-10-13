/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_SANDBOX_DEVMODE_H
#define ALRIOS_SANDBOX_DEVMODE_H

#include <stdint.h>

#define ALRIOS_DEVMODE_POLICY_ABI_VERSION 1U

typedef enum alrios_deployment_mode {
    ALRIOS_DEPLOYMENT_DEVELOPMENT = 1,
    ALRIOS_DEPLOYMENT_PRODUCTION = 2
} alrios_deployment_mode_t;

typedef enum alrios_artifact_trust {
    ALRIOS_ARTIFACT_UNSIGNED = 1,
    ALRIOS_ARTIFACT_SIGNATURE_VERIFIED = 2
} alrios_artifact_trust_t;

typedef enum alrios_execution_ring {
    ALRIOS_EXECUTION_RING_DEVMODE = 1,
    ALRIOS_EXECUTION_RING_SOVEREIGN = 2
} alrios_execution_ring_t;

typedef enum alrios_devmode_policy_status {
    ALRIOS_DEVMODE_POLICY_OK = 0,
    ALRIOS_DEVMODE_POLICY_ERR_INVALID = -5100,
    ALRIOS_DEVMODE_POLICY_ERR_DISABLED = -5101,
    ALRIOS_DEVMODE_POLICY_ERR_UNSIGNED_SOVEREIGN = -5102
} alrios_devmode_policy_status_t;

typedef struct alrios_devmode_policy {
    uint32_t abi_version;
    alrios_deployment_mode_t deployment;
    uint8_t devmode_enabled;
} alrios_devmode_policy_t;

typedef struct alrios_devmode_request {
    alrios_artifact_trust_t artifact_trust;
    alrios_execution_ring_t requested_ring;
} alrios_devmode_request_t;

typedef struct alrios_devmode_admission {
    uint32_t abi_version;
    alrios_artifact_trust_t artifact_trust;
    alrios_execution_ring_t execution_ring;
} alrios_devmode_admission_t;

int alrios_devmode_policy_evaluate(const alrios_devmode_policy_t *policy,
                                   const alrios_devmode_request_t *request,
                                   alrios_devmode_admission_t *out_admission);

#endif
