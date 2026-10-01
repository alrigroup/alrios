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

#include "alrios/ar.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AR_APP_OK 0
#define AR_APP_ERR_INVALID_ARGUMENT -101
#define AR_APP_ERR_TIMEOUT -102
#define AR_APP_ERR_NETWORK -103

typedef struct ar_app_context {
    const char *app_id;
    const char *persistent_storage_dir;
    void *user_data;
} ar_app_context_t;

typedef int (*ar_app_init_fn)(ar_app_context_t *ctx);
typedef int (*ar_app_run_fn)(ar_app_context_t *ctx);
typedef void (*ar_app_cleanup_fn)(ar_app_context_t *ctx);

typedef struct ar_app_spec {
    const char *name;
    const char *version;
    ar_app_init_fn init;
    ar_app_run_fn run;
    ar_app_cleanup_fn cleanup;
} ar_app_spec_t;

#define AR_DECLARE_APP(name_str, ver_str, init_cb, run_cb, cleanup_cb) \
    const ar_app_spec_t g_ar_app_manifest = { \
        .name = (name_str), \
        .version = (ver_str), \
        .init = (init_cb), \
        .run = (run_cb), \
        .cleanup = (cleanup_cb) \
    }

const char *ar_app_get_storage_path(const ar_app_context_t *ctx, const char *relative_filename, char *out_buf, size_t buf_size);
const char *ar_app_get_config_value(const ar_app_context_t *ctx, const char *key);

int ar_net_connect_timeout(const char *host, uint16_t port, uint32_t timeout_ms, int *out_fd);
int ar_net_send_timeout(int fd, const void *buf, size_t len, uint32_t timeout_ms, size_t *out_sent);
int ar_net_recv_timeout(int fd, void *buf, size_t len, uint32_t timeout_ms, size_t *out_recvd);

int ar_app_execute(const ar_app_spec_t *spec, const char *app_id, const char *storage_dir);

#ifdef __cplusplus
}
#endif

#endif
