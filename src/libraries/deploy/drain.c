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

#include "alrios/deploy_drain.h"
#include <errno.h>
#include <time.h>

#define ALRIOS_DRAIN_DEFAULT_TIMEOUT_MS 5000U
#define ALRIOS_DRAIN_DEFAULT_POLL_MS 10U

static uint64_t monotonic_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

int alrios_drain_init(alrios_drain_tracker_t *tracker) {
    if (!tracker) return -1;
    tracker->active_connections = 0;
    tracker->is_shutting_down = 0;
    return 0;
}

int alrios_drain_connection_open(alrios_drain_tracker_t *tracker) {
    if (!tracker || tracker->is_shutting_down) return -1;
    if (tracker->active_connections == INT32_MAX) return -1;
    tracker->active_connections++;
    return 0;
}

int alrios_drain_connection_close(alrios_drain_tracker_t *tracker) {
    if (!tracker || tracker->active_connections <= 0) return -1;
    tracker->active_connections--;
    return 0;
}

int alrios_drain_wait(alrios_drain_tracker_t *tracker, const alrios_drain_config_t *config) {
    if (!tracker) return -1;
    uint32_t timeout = config && config->drain_timeout_ms ? config->drain_timeout_ms : ALRIOS_DRAIN_DEFAULT_TIMEOUT_MS;
    uint32_t poll_ms = config && config->poll_interval_ms ? config->poll_interval_ms : ALRIOS_DRAIN_DEFAULT_POLL_MS;
    tracker->is_shutting_down = 1;
    uint64_t start = monotonic_ms();

    while (tracker->active_connections > 0) {
        uint64_t now = monotonic_ms();
        if (now >= start && now - start >= timeout) {
            return 1; /* configurable timeout; caller decides escalation, no blind SIGKILL */
        }
        struct timespec delay = {
            .tv_sec = (time_t)(poll_ms / 1000U),
            .tv_nsec = (long)(poll_ms % 1000U) * 1000000L
        };
        while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {}
    }
    return 0;
}
