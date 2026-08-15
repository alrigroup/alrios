/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "ar_kernel.h"
#include "ar_svc.h"
#include "aros_hal.h"

typedef enum {
    STRATEGY_ONEFORONE,
    STRATEGY_ONEFORALL,
    STRATEGY_TEMPORARY
} supervisor_strategy_t;

static supervisor_strategy_t current_strategy = STRATEGY_ONEFORONE;

int ar_supervisor_set_strategy(int strategy) {
    if (strategy < STRATEGY_ONEFORONE || strategy > STRATEGY_TEMPORARY)
        return -1;
    current_strategy = (supervisor_strategy_t)strategy;
    return 0;
}

int ar_supervisor_restart_one(const char *name) {
    if (ar_svc_status(name) == SVC_CRASHED) {
        return ar_svc_restart(name);
    }
    return 0;
}

int ar_supervisor_restart_all(void) {
    int count = ar_svc_get_count();
    for (int i = 0; i < count; i++) {
        const char *name = ar_svc_get_name(i);
        if (name && ar_svc_status(name) == SVC_CRASHED)
            ar_svc_restart(name);
    }
    return 0;
}
