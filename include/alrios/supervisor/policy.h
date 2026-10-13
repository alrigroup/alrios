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
#include <stdint.h>
#include <stddef.h>

#define ALRIOS_POLICY_OK                 0
#define ALRIOS_POLICY_ERR_ARGUMENT      -3501
#define ALRIOS_POLICY_ERR_REJECTED      -3502
#define ALRIOS_POLICY_ERR_DOWNGRADE     -3503
#define ALRIOS_POLICY_ERR_CAPABILITY    -3504

#ifdef __cplusplus
extern "C" {
#endif

typedef struct alrios_app_requirements {
    uint32_t container_flags;
    uint32_t required_capabilities;
    int require_pqc;
} alrios_app_requirements_t;

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
