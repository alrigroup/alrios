/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_RESOURCES_PSI_H
#define ALRIOS_RESOURCES_PSI_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum alrios_pressure_level {
    ALRIOS_PRESSURE_NORMAL = 0,
    ALRIOS_PRESSURE_WARN   = 80,
    ALRIOS_PRESSURE_SHED   = 90
} alrios_pressure_level_t;

typedef enum alrios_daemon_tier {
    ALRIOS_TIER_CORE   = 0, /* Protected: arkernel, ardb, arauth - never sacrificed */
    ALRIOS_TIER_WORKER = 1, /* Dynamic worker: arws, arwe */
    ALRIOS_TIER_GUEST  = 2  /* DevMode guest applications - sacrificial */
} alrios_daemon_tier_t;

typedef struct alrios_target_process {
    int pid;
    alrios_daemon_tier_t tier;
    int is_sacrificed;
} alrios_target_process_t;

alrios_pressure_level_t alrios_psi_evaluate_pressure(double memory_usage_percent);

int alrios_psi_handle_notification(alrios_pressure_level_t level,
                                   alrios_target_process_t *targets,
                                   size_t target_count,
                                   size_t *out_shed_count);

#ifdef __cplusplus
}
#endif

#endif
