/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "alrios/resources/psi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

alrios_pressure_level_t alrios_psi_evaluate_pressure(double memory_usage_percent) {
    if (memory_usage_percent >= 90.0) {
        return ALRIOS_PRESSURE_SHED;
    } else if (memory_usage_percent >= 80.0) {
        return ALRIOS_PRESSURE_WARN;
    }
    return ALRIOS_PRESSURE_NORMAL;
}

int alrios_psi_handle_notification(alrios_pressure_level_t level,
                                   alrios_target_process_t *targets,
                                   size_t target_count,
                                   size_t *out_shed_count) {
    if (!targets && target_count > 0) {
        return -1;
    }

    size_t shed = 0;

    if (level == ALRIOS_PRESSURE_WARN) {
        // Emit cooperative warning - workers flush caches, no processes sacrificed
        for (size_t i = 0; i < target_count; i++) {
            targets[i].is_sacrificed = 0;
        }
    } else if (level == ALRIOS_PRESSURE_SHED) {
        // Selective shedding: only sacrificial guests are terminated, core daemons are NEVER sacrificed
        for (size_t i = 0; i < target_count; i++) {
            if (targets[i].tier == ALRIOS_TIER_GUEST) {
                targets[i].is_sacrificed = 1;
                shed++;
            } else {
                // CORE and WORKER daemons remain protected
                targets[i].is_sacrificed = 0;
            }
        }
    }

    if (out_shed_count) {
        *out_shed_count = shed;
    }
    return 0;
}
