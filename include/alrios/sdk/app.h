/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_SDK_APP_H
#define ALRIOS_SDK_APP_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Return codes */
#define AR_APP_OK                    0
#define AR_APP_ERR_INVALID_ARG      -1
#define AR_APP_ERR_TIMEOUT          (-ETIMEDOUT)
#define AR_APP_ERR_STATE            -3
#define AR_APP_ERR_NOT_FOUND        -4
#define AR_APP_ERR_UNAUTHORIZED     -5
#define AR_APP_ERR_STORAGE          -6
#define AR_APP_ERR_NETWORK          -7
#define AR_APP_ERR_VAULT            -8
#define AR_APP_ERR_BUFFER_TOO_SMALL -9
#define AR_APP_ERR_OOM              -10

/* Event identifiers */
#define AR_APP_EVENT_TICK    1U
#define AR_APP_EVENT_CUSTOM  2U

/* Maximum lengths */
#define AR_APP_ID_MAX_LEN        128U
#define AR_APP_VERSION_MAX_LEN    32U
#define AR_APP_STORAGE_PATH_MAX  512U
#define AR_APP_LOG_LINE_MAX     2048U
#define AR_APP_MAX_SECRETS        64U
#define AR_APP_SECRET_KEY_MAX     64U
#define AR_APP_SECRET_VAL_MAX   1024U

/* Application log levels */
typedef enum ar_app_log_level {
    AR_LOG_LEVEL_DEBUG = 0,
    AR_LOG_LEVEL_INFO  = 1,
    AR_LOG_LEVEL_WARN  = 2,
    AR_LOG_LEVEL_ERROR = 3,
    AR_LOG_LEVEL_FATAL = 4
} ar_app_log_level_t;

/* Application lifecycle states */
typedef enum ar_app_state {
    AR_APP_STATE_UNINITIALIZED = 0,
    AR_APP_STATE_INITIALIZED   = 1,
    AR_APP_STATE_RUNNING       = 2,
    AR_APP_STATE_STOPPED       = 3,
    AR_APP_STATE_DESTROYED     = 4,
    AR_APP_STATE_ERROR         = 5
} ar_app_state_t;

#define AR_APP_STATE_STARTED AR_APP_STATE_RUNNING

/* Forward declarations */
typedef struct ar_app_context ar_app_context_t;
typedef struct ar_app_descriptor ar_app_descriptor_t;

/* Lifecycle callback types */
typedef int (*ar_app_init_cb)(ar_app_context_t *ctx);
typedef int (*ar_app_start_cb)(ar_app_context_t *ctx);
typedef int (*ar_app_event_cb)(ar_app_context_t *ctx, uint32_t event_type, void *event_data);
typedef int (*ar_app_tick_cb)(ar_app_context_t *ctx);
typedef int (*ar_app_stop_cb)(ar_app_context_t *ctx);
typedef void (*ar_app_destroy_cb)(ar_app_context_t *ctx);

/* Application descriptor */
struct ar_app_descriptor {
    const char        *app_id;
    const char        *version;
    ar_app_init_cb     on_init;
    ar_app_start_cb    on_start;
    ar_app_event_cb    on_event; /* tick / event callback */
    ar_app_stop_cb     on_stop;
    ar_app_destroy_cb  on_destroy;
};

/**
 * AR_DECLARE_APP
 * Registers application descriptor and lifecycle callbacks,
 * exposing alrios_get_app_descriptor() and ar_app_get_descriptor().
 */
#define AR_DECLARE_APP(app_id_val, version_val, init_cb, start_cb, event_cb, stop_cb, destroy_cb) \
    static const ar_app_descriptor_t g_alrios_app_descriptor = { \
        .app_id     = (app_id_val), \
        .version    = (version_val), \
        .on_init    = (init_cb), \
        .on_start   = (start_cb), \
        .on_event   = (event_cb), \
        .on_stop    = (stop_cb), \
        .on_destroy = (destroy_cb) \
    }; \
    const ar_app_descriptor_t *alrios_get_app_descriptor(void) { \
        return &g_alrios_app_descriptor; \
    } \
    const ar_app_descriptor_t *ar_app_get_descriptor(void) { \
        return &g_alrios_app_descriptor; \
    }

const ar_app_descriptor_t *alrios_get_app_descriptor(void);
const ar_app_descriptor_t *ar_app_get_descriptor(void);

/* Context lifecycle */
ar_app_context_t *ar_app_context_create(const ar_app_descriptor_t *desc);
void ar_app_context_destroy(ar_app_context_t *ctx);

/* Lifecycle state machine drivers */
int ar_app_init(ar_app_context_t *ctx);
int ar_app_start(ar_app_context_t *ctx);
int ar_app_tick(ar_app_context_t *ctx);
int ar_app_event(ar_app_context_t *ctx, uint32_t event_type, void *event_data);
int ar_app_stop(ar_app_context_t *ctx);
int ar_app_destroy(ar_app_context_t *ctx);

/* State inspection */
ar_app_state_t ar_app_get_state(const ar_app_context_t *ctx);
const char *ar_app_state_to_string(ar_app_state_t state);
const char *ar_app_get_id(const ar_app_context_t *ctx);
const char *ar_app_get_version(const ar_app_context_t *ctx);
void ar_app_set_user_data(ar_app_context_t *ctx, void *user_data);
void *ar_app_get_user_data(const ar_app_context_t *ctx);

/* Storage API: returns strictly "/data/<app_id>" scoped path */
const char *ar_app_get_storage_path(const ar_app_context_t *ctx);
int ar_app_get_storage_path_buf(const ar_app_context_t *ctx, char *out_buf, size_t buf_size);

/* Vault / Secret API: reads through sealed memfd / vault context */
int ar_app_get_secret(
    const ar_app_context_t *ctx,
    const char *key,
    char *out_buf,
    size_t buf_size
);

int ar_app_context_set_vault_memfd(
    ar_app_context_t *ctx,
    int memfd
);

int ar_app_context_set_secret(
    ar_app_context_t *ctx,
    const char *key,
    const char *value
);

int ar_app_context_set_authorized_keys(
    ar_app_context_t *ctx,
    const char *const *authorized_keys,
    size_t count
);

/* Mandatory Timeout Networking API */
int ar_app_net_connect(
    ar_app_context_t *ctx,
    const char *host,
    uint16_t port,
    uint32_t timeout_ms
);

int ar_app_net_send(
    ar_app_context_t *ctx,
    int fd,
    const void *buf,
    size_t len,
    uint32_t timeout_ms
);

int ar_app_net_recv(
    ar_app_context_t *ctx,
    int fd,
    void *buf,
    size_t len,
    uint32_t timeout_ms
);

void ar_app_net_close(
    ar_app_context_t *ctx,
    int fd
);

/* Structured Logging API */
typedef void (*ar_app_log_sink_cb)(
    ar_app_log_level_t level,
    const char *tag,
    const char *json_formatted,
    void *user_data
);

void ar_app_set_log_sink(
    ar_app_context_t *ctx,
    ar_app_log_sink_cb sink,
    void *user_data
);

const char *ar_app_context_get_last_log(const ar_app_context_t *ctx);

int ar_app_log(
    ar_app_context_t *ctx,
    ar_app_log_level_t level,
    const char *fmt,
    ...
) __attribute__((format(printf, 3, 4)));

int ar_app_vlog(
    ar_app_context_t *ctx,
    ar_app_log_level_t level,
    const char *fmt,
    va_list args
);

#ifdef __cplusplus
}
#endif

#endif /* ALRIOS_SDK_APP_H */
