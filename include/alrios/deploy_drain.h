/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_DEPLOY_DRAIN_H
#define ALRIOS_DEPLOY_DRAIN_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct alrios_drain_config {
    uint32_t drain_timeout_ms;
    uint32_t poll_interval_ms;
} alrios_drain_config_t;

typedef struct alrios_drain_tracker {
    int active_connections;
    int is_shutting_down;
} alrios_drain_tracker_t;

int alrios_drain_init(alrios_drain_tracker_t *tracker);
int alrios_drain_connection_open(alrios_drain_tracker_t *tracker);
int alrios_drain_connection_close(alrios_drain_tracker_t *tracker);
int alrios_drain_wait(alrios_drain_tracker_t *tracker, const alrios_drain_config_t *config);

#ifdef __cplusplus
}
#endif

#endif
