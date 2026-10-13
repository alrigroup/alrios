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

#include "alrios/deploy/drain.h"
#include "aros_hal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static uint64_t drain_monotonic_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0U;
    }
    return ((uint64_t)ts.tv_sec * 1000ULL) + ((uint64_t)ts.tv_nsec / 1000000ULL);
}

static void drain_sleep_ms(uint32_t ms) {
    struct timespec req;
    req.tv_sec = (time_t)(ms / 1000U);
    req.tv_nsec = (long)((ms % 1000U) * 1000000ULL);
    while (nanosleep(&req, &req) != 0 && errno == EINTR) {
    }
}

int alrios_drain_tracker_init(alrios_drain_tracker_t *tracker,
                              const alrios_drain_config_t *config) {
    if (tracker == NULL) {
        return ALRIOS_DRAIN_ERR_INVALID;
    }

    memset(tracker, 0, sizeof(*tracker));

    if (config != NULL) {
        tracker->config = *config;
    } else {
        tracker->config.drain_timeout_ms = ALRIOS_DRAIN_DEFAULT_TIMEOUT_MS;
        tracker->config.grace_period_ms = ALRIOS_DRAIN_DEFAULT_GRACE_MS;
        tracker->config.max_streams = ALRIOS_DRAIN_MAX_STREAMS;
    }

    if (tracker->config.drain_timeout_ms == 0U) {
        tracker->config.drain_timeout_ms = ALRIOS_DRAIN_DEFAULT_TIMEOUT_MS;
    }
    if (tracker->config.max_streams == 0U) {
        tracker->config.max_streams = ALRIOS_DRAIN_MAX_STREAMS;
    }

    tracker->stream_capacity = tracker->config.max_streams;
    tracker->streams = (alrios_drain_stream_t *)calloc(tracker->stream_capacity,
                                                       sizeof(alrios_drain_stream_t));
    if (tracker->streams == NULL) {
        return ALRIOS_DRAIN_ERR_INVALID;
    }

    tracker->notify_pipe[0] = -1;
    tracker->notify_pipe[1] = -1;

#if defined(HAVE_PIPE2) && defined(O_CLOEXEC)
    if (pipe2(tracker->notify_pipe, O_CLOEXEC | O_NONBLOCK) != 0) {
        tracker->notify_pipe[0] = -1;
        tracker->notify_pipe[1] = -1;
    }
#else
    if (pipe(tracker->notify_pipe) == 0) {
        (void)fcntl(tracker->notify_pipe[0], F_SETFD, FD_CLOEXEC);
        (void)fcntl(tracker->notify_pipe[1], F_SETFD, FD_CLOEXEC);
        int fl0 = fcntl(tracker->notify_pipe[0], F_GETFL, 0);
        if (fl0 >= 0) {
            (void)fcntl(tracker->notify_pipe[0], F_SETFL, fl0 | O_NONBLOCK);
        }
        int fl1 = fcntl(tracker->notify_pipe[1], F_GETFL, 0);
        if (fl1 >= 0) {
            (void)fcntl(tracker->notify_pipe[1], F_SETFL, fl1 | O_NONBLOCK);
        }
    } else {
        tracker->notify_pipe[0] = -1;
        tracker->notify_pipe[1] = -1;
    }
#endif

    atomic_flag_clear(&tracker->lock);
    atomic_store(&tracker->state, ALRIOS_DRAIN_STATE_ACTIVE);
    return ALRIOS_DRAIN_OK;
}

void alrios_drain_tracker_destroy(alrios_drain_tracker_t *tracker) {
    if (tracker == NULL) {
        return;
    }

    while (atomic_flag_test_and_set(&tracker->lock)) {
    }

    if (tracker->notify_pipe[0] >= 0) {
        (void)close(tracker->notify_pipe[0]);
        tracker->notify_pipe[0] = -1;
    }
    if (tracker->notify_pipe[1] >= 0) {
        (void)close(tracker->notify_pipe[1]);
        tracker->notify_pipe[1] = -1;
    }

    free(tracker->streams);
    tracker->streams = NULL;
    tracker->stream_capacity = 0U;
    tracker->active_count = 0U;

    atomic_flag_clear(&tracker->lock);
    atomic_store(&tracker->state, ALRIOS_DRAIN_STATE_IDLE);
}

int alrios_drain_stream_register(alrios_drain_tracker_t *tracker,
                                 int fd,
                                 uint64_t stream_id,
                                 void *user_data) {
    if (tracker == NULL || stream_id == 0U) {
        return ALRIOS_DRAIN_ERR_INVALID;
    }

    if (atomic_load(&tracker->state) != ALRIOS_DRAIN_STATE_ACTIVE) {
        return ALRIOS_DRAIN_ERR_STATE;
    }

    while (atomic_flag_test_and_set(&tracker->lock)) {
    }

    if (atomic_load(&tracker->state) != ALRIOS_DRAIN_STATE_ACTIVE) {
        atomic_flag_clear(&tracker->lock);
        return ALRIOS_DRAIN_ERR_STATE;
    }

    size_t free_idx = tracker->stream_capacity;
    for (size_t i = 0U; i < tracker->stream_capacity; ++i) {
        if (tracker->streams[i].stream_id == stream_id &&
            tracker->streams[i].state != ALRIOS_STREAM_STATE_CLOSED) {
            atomic_flag_clear(&tracker->lock);
            return ALRIOS_DRAIN_ERR_INVALID;
        }
        if (free_idx == tracker->stream_capacity &&
            (tracker->streams[i].stream_id == 0U ||
             tracker->streams[i].state == ALRIOS_STREAM_STATE_CLOSED)) {
            free_idx = i;
        }
    }

    if (free_idx >= tracker->stream_capacity) {
        atomic_flag_clear(&tracker->lock);
        return ALRIOS_DRAIN_ERR_LIMIT;
    }

    tracker->streams[free_idx].stream_id = stream_id;
    tracker->streams[free_idx].fd = fd;
    tracker->streams[free_idx].state = ALRIOS_STREAM_STATE_OPEN;
    tracker->streams[free_idx].connected_at_ms = drain_monotonic_ms();
    tracker->streams[free_idx].user_data = user_data;

    tracker->active_count++;
    tracker->total_accepted++;

    atomic_flag_clear(&tracker->lock);
    return ALRIOS_DRAIN_OK;
}

int alrios_drain_stream_complete(alrios_drain_tracker_t *tracker,
                                 uint64_t stream_id) {
    if (tracker == NULL || stream_id == 0U) {
        return ALRIOS_DRAIN_ERR_INVALID;
    }

    while (atomic_flag_test_and_set(&tracker->lock)) {
    }

    size_t found_idx = tracker->stream_capacity;
    for (size_t i = 0U; i < tracker->stream_capacity; ++i) {
        if (tracker->streams[i].stream_id == stream_id &&
            tracker->streams[i].state != ALRIOS_STREAM_STATE_CLOSED) {
            found_idx = i;
            break;
        }
    }

    if (found_idx >= tracker->stream_capacity) {
        atomic_flag_clear(&tracker->lock);
        return ALRIOS_DRAIN_ERR_NOT_FOUND;
    }

    tracker->streams[found_idx].state = ALRIOS_STREAM_STATE_CLOSED;
    tracker->streams[found_idx].stream_id = 0U;
    if (tracker->active_count > 0U) {
        tracker->active_count--;
    }
    tracker->total_completed++;

    if (atomic_load(&tracker->state) == ALRIOS_DRAIN_STATE_DRAINING &&
        tracker->active_count == 0U) {
        atomic_store(&tracker->state, ALRIOS_DRAIN_STATE_COMPLETED);
    }

    atomic_flag_clear(&tracker->lock);
    return ALRIOS_DRAIN_OK;
}

bool alrios_drain_is_draining(const alrios_drain_tracker_t *tracker) {
    if (tracker == NULL) {
        return false;
    }
    return atomic_load(&tracker->state) == ALRIOS_DRAIN_STATE_DRAINING;
}

size_t alrios_drain_active_count(alrios_drain_tracker_t *tracker) {
    if (tracker == NULL) {
        return 0U;
    }
    while (atomic_flag_test_and_set(&tracker->lock)) {
    }
    size_t count = tracker->active_count;
    atomic_flag_clear(&tracker->lock);
    return count;
}

int alrios_drain_begin(alrios_drain_tracker_t *tracker) {
    if (tracker == NULL) {
        return ALRIOS_DRAIN_ERR_INVALID;
    }

    while (atomic_flag_test_and_set(&tracker->lock)) {
    }

    tracker->drain_start_ms = drain_monotonic_ms();

    if (tracker->active_count == 0U) {
        atomic_store(&tracker->state, ALRIOS_DRAIN_STATE_COMPLETED);
    } else {
        atomic_store(&tracker->state, ALRIOS_DRAIN_STATE_DRAINING);
    }

    if (tracker->notify_pipe[1] >= 0) {
        char sig = 'D';
        ssize_t written = write(tracker->notify_pipe[1], &sig, 1);
        (void)written;
    }

    atomic_flag_clear(&tracker->lock);
    return ALRIOS_DRAIN_OK;
}

int alrios_drain_poll(alrios_drain_tracker_t *tracker) {
    if (tracker == NULL) {
        return ALRIOS_DRAIN_ERR_INVALID;
    }

    alrios_drain_state_t st = atomic_load(&tracker->state);
    if (st == ALRIOS_DRAIN_STATE_COMPLETED) {
        return ALRIOS_DRAIN_OK;
    }
    if (st == ALRIOS_DRAIN_STATE_TIMED_OUT) {
        return ALRIOS_DRAIN_ERR_TIMEOUT;
    }
    if (st != ALRIOS_DRAIN_STATE_DRAINING) {
        return 1;
    }

    while (atomic_flag_test_and_set(&tracker->lock)) {
    }

    if (tracker->active_count == 0U) {
        atomic_store(&tracker->state, ALRIOS_DRAIN_STATE_COMPLETED);
        atomic_flag_clear(&tracker->lock);
        return ALRIOS_DRAIN_OK;
    }

    uint64_t now = drain_monotonic_ms();
    if (now >= tracker->drain_start_ms + (uint64_t)tracker->config.drain_timeout_ms) {
        atomic_store(&tracker->state, ALRIOS_DRAIN_STATE_TIMED_OUT);
        tracker->total_timed_out += tracker->active_count;
        atomic_flag_clear(&tracker->lock);
        return ALRIOS_DRAIN_ERR_TIMEOUT;
    }

    atomic_flag_clear(&tracker->lock);
    return 1;
}

int alrios_drain_wait_complete(alrios_drain_tracker_t *tracker, int timeout_ms) {
    if (tracker == NULL) {
        return ALRIOS_DRAIN_ERR_INVALID;
    }

    uint32_t limit_ms = (timeout_ms > 0)
                            ? (uint32_t)timeout_ms
                            : tracker->config.drain_timeout_ms;
    if (limit_ms == 0U) {
        limit_ms = ALRIOS_DRAIN_DEFAULT_TIMEOUT_MS;
    }

    uint64_t deadline = drain_monotonic_ms() + (uint64_t)limit_ms;

    for (;;) {
        int pr = alrios_drain_poll(tracker);
        if (pr == ALRIOS_DRAIN_OK) {
            return ALRIOS_DRAIN_OK;
        }
        if (pr == ALRIOS_DRAIN_ERR_TIMEOUT) {
            return ALRIOS_DRAIN_ERR_TIMEOUT;
        }

        uint64_t now = drain_monotonic_ms();
        if (now >= deadline) {
            while (atomic_flag_test_and_set(&tracker->lock)) {
            }
            atomic_store(&tracker->state, ALRIOS_DRAIN_STATE_TIMED_OUT);
            tracker->total_timed_out += tracker->active_count;
            atomic_flag_clear(&tracker->lock);
            return ALRIOS_DRAIN_ERR_TIMEOUT;
        }

        drain_sleep_ms(5U);
    }
}

int alrios_drain_get_stats(alrios_drain_tracker_t *tracker,
                           alrios_drain_stats_t *out_stats) {
    if (tracker == NULL || out_stats == NULL) {
        return ALRIOS_DRAIN_ERR_INVALID;
    }

    while (atomic_flag_test_and_set(&tracker->lock)) {
    }

    out_stats->total_accepted = tracker->total_accepted;
    out_stats->total_completed = tracker->total_completed;
    out_stats->total_timed_out = tracker->total_timed_out;
    out_stats->active_streams = tracker->active_count;

    atomic_flag_clear(&tracker->lock);
    return ALRIOS_DRAIN_OK;
}

int alrios_drain_process_await(pid_t pid,
                               uint32_t timeout_ms,
                               uint32_t grace_period_ms,
                               int *out_exit_status) {
    if (pid <= 1) {
        return ALRIOS_DRAIN_ERR_INVALID;
    }

    if (out_exit_status != NULL) {
        *out_exit_status = 0;
    }

    uint32_t wait_limit_ms = (timeout_ms > 0U) ? timeout_ms : ALRIOS_DRAIN_DEFAULT_TIMEOUT_MS;
    uint32_t grace_limit_ms = (grace_period_ms > 0U) ? grace_period_ms : ALRIOS_DRAIN_DEFAULT_GRACE_MS;

    /* Phase 1: Graceful drain wait (No blind SIGKILL at 100ms) */
    uint64_t deadline = drain_monotonic_ms() + (uint64_t)wait_limit_ms;
    for (;;) {
        int status = 0;
        int wait_result = ar_process_wait_raw_nohang((int)pid, &status);
        if (wait_result > 0) {
            if (out_exit_status != NULL) {
                if (WIFEXITED(status)) {
                    *out_exit_status = WEXITSTATUS(status);
                } else if (WIFSIGNALED(status)) {
                    *out_exit_status = -WTERMSIG(status);
                }
            }
            return ALRIOS_DRAIN_OK;
        }
        if (wait_result < 0 &&
            ar_process_signal((int)pid, AR_PROCESS_SIGNAL_NONE, 0) != 0) {
            return ALRIOS_DRAIN_OK;
        }

        uint64_t now = drain_monotonic_ms();
        if (now >= deadline) {
            break;
        }

        drain_sleep_ms(10U);
    }

    /* Phase 2: Timeout reached, escalate to SIGTERM */
    (void)ar_process_signal((int)pid, AR_PROCESS_SIGNAL_TERM, 0);

    uint64_t grace_deadline = drain_monotonic_ms() + (uint64_t)grace_limit_ms;
    for (;;) {
        int status = 0;
        int wait_result = ar_process_wait_raw_nohang((int)pid, &status);
        if (wait_result > 0) {
            if (out_exit_status != NULL) {
                if (WIFEXITED(status)) {
                    *out_exit_status = WEXITSTATUS(status);
                } else if (WIFSIGNALED(status)) {
                    *out_exit_status = -WTERMSIG(status);
                }
            }
            return ALRIOS_DRAIN_OK;
        }
        if (wait_result < 0 &&
            ar_process_signal((int)pid, AR_PROCESS_SIGNAL_NONE, 0) != 0) {
            return ALRIOS_DRAIN_OK;
        }

        uint64_t now = drain_monotonic_ms();
        if (now >= grace_deadline) {
            break;
        }

        drain_sleep_ms(10U);
    }

    /* Phase 3: Unresponsive after timeout + grace period, escalate to SIGKILL */
    (void)ar_process_signal((int)pid, AR_PROCESS_SIGNAL_KILL, 0);

    for (int i = 0; i < 20; ++i) {
        int status = 0;
        int wait_result = ar_process_wait_raw_nohang((int)pid, &status);
        if (wait_result != 0) {
            if (out_exit_status != NULL && wait_result > 0) {
                if (WIFEXITED(status)) {
                    *out_exit_status = WEXITSTATUS(status);
                } else if (WIFSIGNALED(status)) {
                    *out_exit_status = -WTERMSIG(status);
                }
            }
            break;
        }
        drain_sleep_ms(10U);
    }

    return ALRIOS_DRAIN_ERR_TIMEOUT;
}
