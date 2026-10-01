/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/sdk/app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int g_init_called = 0;
static int g_run_called = 0;
static int g_cleanup_called = 0;
static int g_init_result = AR_APP_OK;
static int g_run_result = AR_APP_OK;
static int g_config_backend_calls = 0;
static int g_log_sink_calls = 0;

static void require(int condition, const char *expression) {
    if (condition == 0) {
        (void)fprintf(stderr, "requirement failed: %s\n", expression);
        exit(EXIT_FAILURE);
    }
}

#define REQUIRE(condition) require((condition), #condition)

static const char *test_config_get(void *backend_context, const char *key) {
    const char *expected_key = (const char *)backend_context;

    ++g_config_backend_calls;
    if (strcmp(key, expected_key) == 0) {
        return "in-memory-secret";
    }
    return NULL;
}

static int test_log_sink(void *backend_context,
                         const char *app_id,
                         ar_app_log_level_t level,
                         const char *event,
                         const char *message,
                         const ar_app_log_field_t *fields,
                         size_t field_count) {
    int *last_level = (int *)backend_context;

    ++g_log_sink_calls;
    *last_level = (int)level;
    REQUIRE(strcmp(app_id, "com.alri.worker") == 0);
    REQUIRE(strcmp(event, "worker.ready") == 0);
    REQUIRE(strcmp(message, "worker initialized") == 0);
    REQUIRE(field_count == 1U);
    REQUIRE(strcmp(fields[0].key, "region") == 0);
    REQUIRE(strcmp(fields[0].value, "test") == 0);
    return AR_APP_OK;
}

static int my_app_init(ar_app_context_t *ctx) {
    char storage_path[128];

    ++g_init_called;
    REQUIRE(ar_app_get_storage_path(ctx,
                                   "state/app.sqlite",
                                   storage_path,
                                   sizeof(storage_path)) != NULL);
    REQUIRE(strcmp(storage_path, "/custom/data/state/app.sqlite") == 0);
    return g_init_result;
}

static int my_app_run(ar_app_context_t *ctx) {
    ar_app_log_field_t fields[] = {{"region", "test"}};
    const char *secret;

    ++g_run_called;
    secret = ar_app_get_config_value(ctx, "API_SECRET");
    REQUIRE(secret != NULL);
    REQUIRE(strcmp(secret, "in-memory-secret") == 0);
    REQUIRE(ar_app_log(ctx,
                      AR_APP_LOG_INFO,
                      "worker.ready",
                      "worker initialized",
                      fields,
                      1U) == AR_APP_OK);
    return g_run_result;
}

static void my_app_cleanup(ar_app_context_t *ctx) {
    REQUIRE(ctx != NULL);
    ++g_cleanup_called;
}

AR_DECLARE_APP("sovereign_worker",
               "1.0.0",
               my_app_init,
               my_app_run,
               my_app_cleanup)

static void reset_lifecycle(void) {
    g_init_called = 0;
    g_run_called = 0;
    g_cleanup_called = 0;
    g_init_result = AR_APP_OK;
    g_run_result = AR_APP_OK;
}

static void test_lifecycle_and_context_apis(void) {
    const ar_app_spec_t *descriptor = alrios_get_app_descriptor();
    ar_app_context_t context;
    char path[32];
    int last_level = -1;

    memset(&context, 0, sizeof(context));
    context.app_id = "com.alri.worker";
    context.persistent_storage_dir = "/custom/data";
    context.config_get = test_config_get;
    context.config_backend_context = (void *)"API_SECRET";
    context.log_sink = test_log_sink;
    context.log_backend_context = &last_level;

    REQUIRE(descriptor == &g_ar_app_manifest);
    REQUIRE(strcmp(descriptor->name, "sovereign_worker") == 0);
    REQUIRE(strcmp(descriptor->version, "1.0.0") == 0);

    reset_lifecycle();
    g_config_backend_calls = 0;
    g_log_sink_calls = 0;
    REQUIRE(ar_app_execute_with_context(descriptor, &context) == AR_APP_OK);
    REQUIRE(g_init_called == 1);
    REQUIRE(g_run_called == 1);
    REQUIRE(g_cleanup_called == 1);
    REQUIRE(g_config_backend_calls == 1);
    REQUIRE(g_log_sink_calls == 1);
    REQUIRE(last_level == (int)AR_APP_LOG_INFO);

    REQUIRE(ar_app_get_storage_path(&context, "../secret", path, sizeof(path)) == NULL);
    REQUIRE(ar_app_get_storage_path(&context, "/etc/passwd", path, sizeof(path)) == NULL);
    REQUIRE(ar_app_get_storage_path(&context, "state//db", path, sizeof(path)) == NULL);
    REQUIRE(ar_app_get_config_value(&context, "/run/secrets/key") == NULL);
    REQUIRE(g_config_backend_calls == 1);

    context.config_get = NULL;
    REQUIRE(ar_app_get_config_value(&context, "API_SECRET") == NULL);
    context.config_get = test_config_get;

    context.log_sink = NULL;
    REQUIRE(ar_app_log(&context,
                      AR_APP_LOG_INFO,
                      "worker.ready",
                      "worker initialized",
                      NULL,
                      0U) == AR_APP_ERR_INVALID_ARGUMENT);
}

static void test_lifecycle_fail_closed(void) {
    ar_app_context_t context;
    ar_app_spec_t incomplete = g_ar_app_manifest;
    int last_level = -1;

    memset(&context, 0, sizeof(context));
    context.app_id = "com.alri.worker";
    context.persistent_storage_dir = "/custom/data";

    reset_lifecycle();
    g_init_result = AR_APP_ERR_IO;
    context.config_get = test_config_get;
    context.config_backend_context = (void *)"API_SECRET";
    context.log_sink = test_log_sink;
    context.log_backend_context = &last_level;
    REQUIRE(ar_app_execute_with_context(&g_ar_app_manifest, &context) == AR_APP_ERR_IO);
    REQUIRE(g_init_called == 1);
    REQUIRE(g_run_called == 0);
    REQUIRE(g_cleanup_called == 0);

    reset_lifecycle();
    g_run_result = AR_APP_ERR_NETWORK;
    REQUIRE(ar_app_execute_with_context(&g_ar_app_manifest, &context) == AR_APP_ERR_NETWORK);
    REQUIRE(g_init_called == 1);
    REQUIRE(g_run_called == 1);
    REQUIRE(g_cleanup_called == 1);

    incomplete.cleanup = NULL;
    REQUIRE(ar_app_execute_with_context(&incomplete, &context) == AR_APP_ERR_INVALID_ARGUMENT);
    REQUIRE(ar_app_execute(NULL, "app", "/data") == AR_APP_ERR_INVALID_ARGUMENT);
    REQUIRE(ar_app_execute(&g_ar_app_manifest, NULL, "/data") == AR_APP_ERR_INVALID_ARGUMENT);
    REQUIRE(ar_app_execute(&g_ar_app_manifest, "app", NULL) == AR_APP_ERR_INVALID_ARGUMENT);
}

static void test_timeout_network_api(void) {
    int sockets[2] = {-1, -1};
    int out_fd = 123;
    size_t transferred = 123U;
    char receive_buffer[8];
    const char payload[] = "ping";

    REQUIRE(ar_net_connect_timeout("127.0.0.1", 8080U, 0U, &out_fd) ==
           AR_APP_ERR_INVALID_ARGUMENT);
    REQUIRE(out_fd == -1);
    REQUIRE(ar_net_connect_timeout(NULL, 8080U, 1U, &out_fd) ==
           AR_APP_ERR_INVALID_ARGUMENT);
    REQUIRE(ar_net_connect_timeout("127.0.0.1", 0U, 1U, &out_fd) ==
           AR_APP_ERR_INVALID_ARGUMENT);

    REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    REQUIRE(ar_net_send_timeout(sockets[0],
                               payload,
                               sizeof(payload),
                               0U,
                               &transferred) == AR_APP_ERR_INVALID_ARGUMENT);
    REQUIRE(transferred == 0U);
    REQUIRE(ar_net_recv_timeout(sockets[1],
                               receive_buffer,
                               sizeof(receive_buffer),
                               0U,
                               &transferred) == AR_APP_ERR_INVALID_ARGUMENT);
    REQUIRE(transferred == 0U);

    REQUIRE(ar_net_recv_timeout(sockets[1],
                               receive_buffer,
                               sizeof(receive_buffer),
                               10U,
                               &transferred) == AR_APP_ERR_TIMEOUT);
    REQUIRE(transferred == 0U);

    REQUIRE(ar_net_send_timeout(sockets[0],
                               payload,
                               sizeof(payload),
                               100U,
                               &transferred) == AR_APP_OK);
    REQUIRE(transferred == sizeof(payload));
    REQUIRE(ar_net_recv_timeout(sockets[1],
                               receive_buffer,
                               sizeof(receive_buffer),
                               100U,
                               &transferred) == AR_APP_OK);
    REQUIRE(transferred == sizeof(payload));
    REQUIRE(memcmp(receive_buffer, payload, sizeof(payload)) == 0);

    REQUIRE(close(sockets[0]) == 0);
    REQUIRE(close(sockets[1]) == 0);
}

int main(void) {
    test_lifecycle_and_context_apis();
    test_lifecycle_fail_closed();
    test_timeout_network_api();
    (void)printf("MP-023 (AR-SDK lifecycle and bounded APIs): PASS\n");
    return 0;
}
