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

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALRIOS_DRAIN_OK                    0
#define ALRIOS_DRAIN_ERR_INVALID         -1
#define ALRIOS_DRAIN_ERR_STATE           -2
#define ALRIOS_DRAIN_ERR_TIMEOUT         -3
#define ALRIOS_DRAIN_ERR_LIMIT           -4
#define ALRIOS_DRAIN_ERR_NOT_FOUND       -5

#define ALRIOS_DRAIN_DEFAULT_TIMEOUT_MS  5000U
#define ALRIOS_DRAIN_DEFAULT_GRACE_MS    1000U
#define ALRIOS_DRAIN_MAX_STREAMS         1024U

typedef enum alrios_drain_state {
    ALRIOS_DRAIN_STATE_IDLE = 0,
    ALRIOS_DRAIN_STATE_ACTIVE,
    ALRIOS_DRAIN_STATE_DRAINING,
    ALRIOS_DRAIN_STATE_COMPLETED,
    ALRIOS_DRAIN_STATE_TIMED_OUT
} alrios_drain_state_t;

typedef enum alrios_stream_state {
    ALRIOS_STREAM_STATE_OPEN = 1,
    ALRIOS_STREAM_STATE_CLOSING,
    ALRIOS_STREAM_STATE_CLOSED
} alrios_stream_state_t;

typedef struct alrios_drain_stream {
    uint64_t stream_id;
    int fd;
    alrios_stream_state_t state;
    uint64_t connected_at_ms;
    void *user_data;
} alrios_drain_stream_t;

typedef struct alrios_drain_config {
    uint32_t drain_timeout_ms;   /* Max time allowed for in-flight streams to complete */
    uint32_t grace_period_ms;    /* Grace period before forced termination if timeout reached */
    uint32_t max_streams;        /* Max tracked active streams */
} alrios_drain_config_t;

typedef struct alrios_drain_stats {
    uint64_t total_accepted;
    uint64_t total_completed;
    uint64_t total_timed_out;
    uint64_t active_streams;
} alrios_drain_stats_t;

typedef struct alrios_drain_tracker {
    alrios_drain_config_t config;
    _Atomic alrios_drain_state_t state;
    atomic_flag lock;
    alrios_drain_stream_t *streams;
    size_t stream_capacity;
    size_t active_count;
    uint64_t drain_start_ms;
    uint64_t total_accepted;
    uint64_t total_completed;
    uint64_t total_timed_out;
    int notify_pipe[2];          /* Internal pipe for unblocking event loops on drain */
} alrios_drain_tracker_t;

/* Initialize the connection drain tracker */
int alrios_drain_tracker_init(alrios_drain_tracker_t *tracker,
                              const alrios_drain_config_t *config);

/* Destroy tracker and free internal stream tracking buffers */
void alrios_drain_tracker_destroy(alrios_drain_tracker_t *tracker);

/* Register a new long-lived connection / stream (e.g. WebSocket, SSE) */
int alrios_drain_stream_register(alrios_drain_tracker_t *tracker,
                                 int fd,
                                 uint64_t stream_id,
                                 void *user_data);

/* Mark a stream as completed / closed */
int alrios_drain_stream_complete(alrios_drain_tracker_t *tracker,
                                 uint64_t stream_id);

/* Check if the tracker is currently draining */
bool alrios_drain_is_draining(const alrios_drain_tracker_t *tracker);

/* Return count of active streams */
size_t alrios_drain_active_count(alrios_drain_tracker_t *tracker);

/* Begin draining: stop accepting new work, wake pollers via notify_pipe, start drain deadline */
int alrios_drain_begin(alrios_drain_tracker_t *tracker);

/* Step or check drain progress; returns 0 if completed, -ETIMEDOUT / ALRIOS_DRAIN_ERR_TIMEOUT if timed out, 1 if still draining */
int alrios_drain_poll(alrios_drain_tracker_t *tracker);

/* Synchronous bounded wait for all registered streams to complete gracefully */
int alrios_drain_wait_complete(alrios_drain_tracker_t *tracker, int timeout_ms);

/* Snapshot drain statistics */
int alrios_drain_get_stats(alrios_drain_tracker_t *tracker,
                           alrios_drain_stats_t *out_stats);

/*
 * Graceful process drain watcher:
 * Replaces hardcoded blind SIGKILL at 100ms.
 * Gracefully waits for target process to finish its in-flight connections
 * up to timeout_ms. If timeout_ms expires and process is still alive,
 * sends SIGTERM, waits grace_period_ms, and only escalates to SIGKILL if
 * the process still refuses to terminate.
 */
int alrios_drain_process_await(pid_t pid,
                               uint32_t timeout_ms,
                               uint32_t grace_period_ms,
                               int *out_exit_status);

#ifdef __cplusplus
}
#endif

#endif /* ALRIOS_DEPLOY_DRAIN_H */
