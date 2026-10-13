/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/deploy/drain.h"

#include <stdlib.h>
#include <string.h>

int alrios_drain_tracker_init(alrios_drain_tracker_t *tracker,
                              const alrios_drain_config_t *config) {
    if (!tracker) return ALRIOS_DRAIN_ERR_INVALID;
    memset(tracker, 0, sizeof(*tracker));
    if (config) tracker->config = *config;
    atomic_init(&tracker->state, ALRIOS_DRAIN_STATE_IDLE);
    atomic_flag_clear(&tracker->lock);
    tracker->notify_pipe[0] = -1;
    tracker->notify_pipe[1] = -1;
    return ALRIOS_DRAIN_OK;
}

void alrios_drain_tracker_destroy(alrios_drain_tracker_t *tracker) {
    if (!tracker) return;
    free(tracker->streams);
    memset(tracker, 0, sizeof(*tracker));
}

int alrios_drain_stream_register(alrios_drain_tracker_t *tracker,
                                 int fd,
                                 uint64_t stream_id,
                                 void *user_data) {
    (void)tracker;
    (void)fd;
    (void)stream_id;
    (void)user_data;
    return ALRIOS_DRAIN_ERR_STATE;
}

int alrios_drain_stream_complete(alrios_drain_tracker_t *tracker, uint64_t stream_id) {
    (void)tracker;
    (void)stream_id;
    return ALRIOS_DRAIN_ERR_STATE;
}

bool alrios_drain_is_draining(const alrios_drain_tracker_t *tracker) {
    return tracker && atomic_load(&tracker->state) == ALRIOS_DRAIN_STATE_DRAINING;
}

size_t alrios_drain_active_count(alrios_drain_tracker_t *tracker) {
    return tracker ? tracker->active_count : 0U;
}

int alrios_drain_begin(alrios_drain_tracker_t *tracker) {
    (void)tracker;
    return ALRIOS_DRAIN_ERR_STATE;
}

int alrios_drain_poll(alrios_drain_tracker_t *tracker) {
    (void)tracker;
    return ALRIOS_DRAIN_ERR_STATE;
}

int alrios_drain_wait_complete(alrios_drain_tracker_t *tracker, int timeout_ms) {
    (void)tracker;
    (void)timeout_ms;
    return ALRIOS_DRAIN_ERR_STATE;
}

int alrios_drain_get_stats(alrios_drain_tracker_t *tracker,
                           alrios_drain_stats_t *out_stats) {
    if (!tracker || !out_stats) return ALRIOS_DRAIN_ERR_INVALID;
    memset(out_stats, 0, sizeof(*out_stats));
    return ALRIOS_DRAIN_OK;
}

int alrios_drain_process_await(pid_t pid,
                               uint32_t timeout_ms,
                               uint32_t grace_period_ms,
                               int *out_exit_status) {
    (void)pid;
    (void)timeout_ms;
    (void)grace_period_ms;
    if (out_exit_status) *out_exit_status = 0;
    return ALRIOS_DRAIN_ERR_STATE;
}
