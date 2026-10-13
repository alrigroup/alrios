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

#include "alrios/sdk/app.h"
#include "alrios/memloader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#ifdef assert
#undef assert
#endif
#define assert(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

/* State trackers for lifecycle callbacks */
static int g_init_called = 0;
static int g_start_called = 0;
static int g_event_called = 0;
static int g_tick_called = 0;
static int g_stop_called = 0;
static int g_destroy_called = 0;
static uint32_t g_last_event_type = 0;

static int mock_app_init(ar_app_context_t *ctx) {
    (void)ctx;
    g_init_called++;
    return AR_APP_OK;
}

static int mock_app_start(ar_app_context_t *ctx) {
    (void)ctx;
    g_start_called++;
    return AR_APP_OK;
}

static int mock_app_event(ar_app_context_t *ctx, uint32_t event_type, void *event_data) {
    (void)ctx;
    (void)event_data;
    g_event_called++;
    g_last_event_type = event_type;
    return AR_APP_OK;
}

static int mock_app_stop(ar_app_context_t *ctx) {
    (void)ctx;
    g_stop_called++;
    return AR_APP_OK;
}

static void mock_app_destroy(ar_app_context_t *ctx) {
    (void)ctx;
    g_destroy_called++;
}

/* Macro registration of the application */
AR_DECLARE_APP(
    "com.alrigroup.core_test",
    "1.0.0",
    mock_app_init,
    mock_app_start,
    mock_app_event,
    mock_app_stop,
    mock_app_destroy
)

static void reset_lifecycle_counters(void) {
    g_init_called = 0;
    g_start_called = 0;
    g_event_called = 0;
    g_tick_called = 0;
    g_stop_called = 0;
    g_destroy_called = 0;
    g_last_event_type = 0;
}

static void test_app_lifecycle_state_machine(void) {
    reset_lifecycle_counters();

    const ar_app_descriptor_t *desc = alrios_get_app_descriptor();
    assert(desc != NULL);
    assert(strcmp(desc->app_id, "com.alrigroup.core_test") == 0);
    assert(strcmp(desc->version, "1.0.0") == 0);

    /* Context creation using descriptor */
    ar_app_context_t *ctx = ar_app_context_create(desc);
    assert(ctx != NULL);
    assert(ar_app_get_state(ctx) == AR_APP_STATE_UNINITIALIZED);
    assert(strcmp(ar_app_state_to_string(ar_app_get_state(ctx)), "UNINITIALIZED") == 0);
    assert(strcmp(ar_app_get_id(ctx), "com.alrigroup.core_test") == 0);
    assert(strcmp(ar_app_get_version(ctx), "1.0.0") == 0);

    /* Cannot start or tick before init */
    assert(ar_app_start(ctx) == AR_APP_ERR_STATE);
    assert(ar_app_tick(ctx) == AR_APP_ERR_STATE);
    assert(ar_app_stop(ctx) == AR_APP_ERR_STATE);

    /* 1. Init transition */
    int rc = ar_app_init(ctx);
    (void)rc;
    assert(rc == AR_APP_OK);
    assert(g_init_called == 1);
    assert(ar_app_get_state(ctx) == AR_APP_STATE_INITIALIZED);
    assert(strcmp(ar_app_state_to_string(ar_app_get_state(ctx)), "INITIALIZED") == 0);

    /* Cannot init twice */
    assert(ar_app_init(ctx) == AR_APP_ERR_STATE);

    /* 2. Start transition */
    rc = ar_app_start(ctx);
    assert(rc == AR_APP_OK);
    assert(g_start_called == 1);
    assert(ar_app_get_state(ctx) == AR_APP_STATE_RUNNING);
    assert(strcmp(ar_app_state_to_string(ar_app_get_state(ctx)), "RUNNING") == 0);

    /* 3. Event and Tick dispatch */
    rc = ar_app_event(ctx, AR_APP_EVENT_CUSTOM, NULL);
    assert(rc == AR_APP_OK);
    assert(g_event_called == 1);
    assert(g_last_event_type == AR_APP_EVENT_CUSTOM);

    rc = ar_app_tick(ctx);
    assert(rc == AR_APP_OK);
    assert(g_event_called == 2);
    assert(g_last_event_type == AR_APP_EVENT_TICK);

    /* 4. Stop transition */
    rc = ar_app_stop(ctx);
    assert(rc == AR_APP_OK);
    assert(g_stop_called == 1);
    assert(ar_app_get_state(ctx) == AR_APP_STATE_STOPPED);
    assert(strcmp(ar_app_state_to_string(ar_app_get_state(ctx)), "STOPPED") == 0);

    /* Cannot tick when stopped */
    assert(ar_app_tick(ctx) == AR_APP_ERR_STATE);

    /* Can restart after stop */
    rc = ar_app_start(ctx);
    assert(rc == AR_APP_OK);
    assert(g_start_called == 2);
    assert(ar_app_get_state(ctx) == AR_APP_STATE_RUNNING);

    rc = ar_app_stop(ctx);
    assert(rc == AR_APP_OK);
    assert(g_stop_called == 2);
    assert(ar_app_get_state(ctx) == AR_APP_STATE_STOPPED);

    /* 5. Destroy transition */
    rc = ar_app_destroy(ctx);
    assert(rc == AR_APP_OK);
    assert(g_destroy_called == 1);
    assert(ar_app_get_state(ctx) == AR_APP_STATE_DESTROYED);
    assert(strcmp(ar_app_state_to_string(ar_app_get_state(ctx)), "DESTROYED") == 0);

    /* Idempotent destroy call */
    rc = ar_app_destroy(ctx);
    assert(rc == AR_APP_OK);
    assert(g_destroy_called == 1);

    /* User data wiring */
    int custom_val = 42;
    ar_app_set_user_data(ctx, &custom_val);
    assert(ar_app_get_user_data(ctx) == &custom_val);

    ar_app_context_destroy(ctx);

    /* Context creation with NULL descriptor falls back to declared descriptor */
    ar_app_context_t *implicit_ctx = ar_app_context_create(NULL);
    assert(implicit_ctx != NULL);
    assert(strcmp(ar_app_get_id(implicit_ctx), "com.alrigroup.core_test") == 0);
    ar_app_context_destroy(implicit_ctx);
}

static void test_app_storage_scoping(void) {
    const ar_app_descriptor_t *desc = alrios_get_app_descriptor();
    ar_app_context_t *ctx = ar_app_context_create(desc);
    assert(ctx != NULL);

    /* 1. Storage path strictly scoped to /data/<app_id> */
    const char *storage_path = ar_app_get_storage_path(ctx);
    (void)storage_path;
    assert(storage_path != NULL);
    assert(strcmp(storage_path, "/data/com.alrigroup.core_test") == 0);

    /* 2. Buffer retrieval */
    char buf[256];
    int rc = ar_app_get_storage_path_buf(ctx, buf, sizeof(buf));
    (void)rc;
    assert(rc == AR_APP_OK);
    assert(strcmp(buf, "/data/com.alrigroup.core_test") == 0);

    /* 3. Small buffer fails gracefully */
    char small_buf[8];
    rc = ar_app_get_storage_path_buf(ctx, small_buf, sizeof(small_buf));
    assert(rc == AR_APP_ERR_BUFFER_TOO_SMALL);

    /* 4. Invalid app IDs rejected at creation (no path traversal allowed) */
    ar_app_descriptor_t bad_desc1 = {
        .app_id = "../evil_escape",
        .version = "1.0.0"
    };
    (void)bad_desc1;
    assert(ar_app_context_create(&bad_desc1) == NULL);

    ar_app_descriptor_t bad_desc2 = {
        .app_id = "/absolute/escape",
        .version = "1.0.0"
    };
    (void)bad_desc2;
    assert(ar_app_context_create(&bad_desc2) == NULL);

    ar_app_descriptor_t bad_desc3 = {
        .app_id = "",
        .version = "1.0.0"
    };
    (void)bad_desc3;
    assert(ar_app_context_create(&bad_desc3) == NULL);

    ar_app_context_destroy(ctx);
}

static void test_secret_vault_no_direct_host_paths(void) {
    const ar_app_descriptor_t *desc = alrios_get_app_descriptor();
    ar_app_context_t *ctx = ar_app_context_create(desc);
    assert(ctx != NULL);

    char val_buf[256];

    /* 1. Direct host path attempts in key are strictly rejected */
    assert(ar_app_get_secret(ctx, "/etc/passwd", val_buf, sizeof(val_buf)) == AR_APP_ERR_INVALID_ARG);
    assert(ar_app_get_secret(ctx, "../secret.key", val_buf, sizeof(val_buf)) == AR_APP_ERR_INVALID_ARG);
    assert(ar_app_get_secret(ctx, "dir/name", val_buf, sizeof(val_buf)) == AR_APP_ERR_INVALID_ARG);
    assert(ar_app_get_secret(ctx, "c:\\windows\\system32", val_buf, sizeof(val_buf)) == AR_APP_ERR_INVALID_ARG);
    assert(ar_app_get_secret(ctx, "", val_buf, sizeof(val_buf)) == AR_APP_ERR_INVALID_ARG);

    /* 2. In-memory secret context API */
    int rc = ar_app_context_set_secret(ctx, "DATABASE_PASSWORD", "SuperSecurePassword123!");
    assert(rc == AR_APP_OK);

    rc = ar_app_get_secret(ctx, "DATABASE_PASSWORD", val_buf, sizeof(val_buf));
    assert(rc == AR_APP_OK);
    assert(strcmp(val_buf, "SuperSecurePassword123!") == 0);

    /* 3. Small buffer handling */
    char tiny[4];
    rc = ar_app_get_secret(ctx, "DATABASE_PASSWORD", tiny, sizeof(tiny));
    assert(rc == AR_APP_ERR_BUFFER_TOO_SMALL);

    /* 4. Missing key returns AR_APP_ERR_NOT_FOUND */
    rc = ar_app_get_secret(ctx, "NON_EXISTENT_KEY", val_buf, sizeof(val_buf));
    assert(rc == AR_APP_ERR_NOT_FOUND);

    /* 5. Authorized keys filtering */
    const char *authorized[1] = { "DATABASE_PASSWORD" };
    rc = ar_app_context_set_authorized_keys(ctx, authorized, 1);
    assert(rc == AR_APP_OK);

    /* Authorized key succeeds */
    rc = ar_app_get_secret(ctx, "DATABASE_PASSWORD", val_buf, sizeof(val_buf));
    assert(rc == AR_APP_OK);

    /* Non-authorized key fails with AR_APP_ERR_UNAUTHORIZED */
    rc = ar_app_context_set_secret(ctx, "OTHER_KEY", "restricted_value");
    assert(rc == AR_APP_OK);
    rc = ar_app_get_secret(ctx, "OTHER_KEY", val_buf, sizeof(val_buf));
    assert(rc == AR_APP_ERR_UNAUTHORIZED);

    /* 6. Sealed memfd vault integration */
    /* An unsealed descriptor (e.g. pipe) MUST be rejected */
    int pipefds[2];
    assert(pipe(pipefds) == 0);
    assert(ar_app_context_set_vault_memfd(ctx, pipefds[0]) == AR_APP_ERR_VAULT);
    close(pipefds[0]);
    close(pipefds[1]);

    /* Create properly sealed memfd using ALRIOS memloader API */
    const char vault_payload[] =
        "com.alrigroup.core_test:API_KEY=alri_live_key_999\n"
        "com.other.app:OTHER_SECRET=leak_attempt\n"
        "GLOBAL_PARAM=cluster_active\n";

    int sealed_fd = alrios_memfd_create_sealed("test_vault",
                                              (const uint8_t *)vault_payload,
                                              strlen(vault_payload));
    assert(sealed_fd >= 0);

    /* Add API_KEY and GLOBAL_PARAM to authorized keys list */
    const char *auth_ext[3] = { "DATABASE_PASSWORD", "API_KEY", "GLOBAL_PARAM" };
    rc = ar_app_context_set_authorized_keys(ctx, auth_ext, 3);
    assert(rc == AR_APP_OK);

    rc = ar_app_context_set_vault_memfd(ctx, sealed_fd);
    assert(rc == AR_APP_OK);

    /* Retrieve app-scoped secret from sealed memfd */
    rc = ar_app_get_secret(ctx, "API_KEY", val_buf, sizeof(val_buf));
    assert(rc == AR_APP_OK);
    assert(strcmp(val_buf, "alri_live_key_999") == 0);

    /* Unscoped secret from sealed memfd */
    rc = ar_app_get_secret(ctx, "GLOBAL_PARAM", val_buf, sizeof(val_buf));
    assert(rc == AR_APP_OK);
    assert(strcmp(val_buf, "cluster_active") == 0);

    /* Secret belonging to other app is strictly isolated and inaccessible */
    const char *auth_other[4] = { "DATABASE_PASSWORD", "API_KEY", "GLOBAL_PARAM", "OTHER_SECRET" };
    rc = ar_app_context_set_authorized_keys(ctx, auth_other, 4);
    assert(rc == AR_APP_OK);
    rc = ar_app_get_secret(ctx, "OTHER_SECRET", val_buf, sizeof(val_buf));
    assert(rc == AR_APP_ERR_NOT_FOUND);

    ar_app_context_destroy(ctx);
}

static void test_mandatory_timeout_networking(void) {
    const ar_app_descriptor_t *desc = alrios_get_app_descriptor();
    ar_app_context_t *ctx = ar_app_context_create(desc);
    assert(ctx != NULL);

    /* 1. Missing timeout is rejected as invalid argument */
    int sock = ar_app_net_connect(ctx, "127.0.0.1", 8080, 0);
    assert(sock == AR_APP_ERR_INVALID_ARG);

    /* 2. Zero port, NULL host rejected */
    assert(ar_app_net_connect(ctx, NULL, 8080, 100) == AR_APP_ERR_INVALID_ARG);
    assert(ar_app_net_connect(ctx, "127.0.0.1", 0, 100) == AR_APP_ERR_INVALID_ARG);
    assert(ar_app_net_connect(NULL, "127.0.0.1", 8080, 100) == AR_APP_ERR_INVALID_ARG);

    /* 3. Send and recv with 0 timeout rejected */
    char tmp[32];
    assert(ar_app_net_send(ctx, 0, "test", 4, 0) == AR_APP_ERR_INVALID_ARG);
    assert(ar_app_net_recv(ctx, 0, tmp, sizeof(tmp), 0) == AR_APP_ERR_INVALID_ARG);

    /* 4. Connect to non-routable address with mandatory timeout fails with timeout */
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    /* 198.51.100.1 is documentation / non-routable prefix (RFC 5737 TEST-NET-2) */
    int timed_out_sock = ar_app_net_connect(ctx, "198.51.100.1", 65432, 50);
    clock_gettime(CLOCK_MONOTONIC, &end);

    /* Must fail with timeout */
    assert(timed_out_sock == AR_APP_ERR_TIMEOUT);

    uint64_t elapsed_ms = (uint64_t)(end.tv_sec - start.tv_sec) * 1000 +
                          (uint64_t)(end.tv_nsec - start.tv_nsec) / 1000000;
    /* Ensure bounded execution */
    assert(elapsed_ms < 500);

    /* 5. Connect to local server succeeds within timeout */
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(server_fd >= 0);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    saddr.sin_port = 0; /* Auto-assign port */

    assert(bind(server_fd, (struct sockaddr *)&saddr, sizeof(saddr)) == 0);
    assert(listen(server_fd, 5) == 0);

    socklen_t slen = sizeof(saddr);
    assert(getsockname(server_fd, (struct sockaddr *)&saddr, &slen) == 0);
    uint16_t local_port = ntohs(saddr.sin_port);

    int client_fd = ar_app_net_connect(ctx, "127.0.0.1", local_port, 500);
    assert(client_fd >= 0);

    int accepted_fd = accept(server_fd, NULL, NULL);
    assert(accepted_fd >= 0);

    /* Test send with timeout */
    const char msg[] = "ALRI_SDK_PING";
    int sent = ar_app_net_send(ctx, client_fd, msg, strlen(msg), 200);
    assert(sent == (int)strlen(msg));

    /* Receive on peer */
    char recv_buf[64];
    memset(recv_buf, 0, sizeof(recv_buf));
    ssize_t n = recv(accepted_fd, recv_buf, sizeof(recv_buf) - 1, 0);
    assert(n == (ssize_t)strlen(msg));
    assert(strcmp(recv_buf, "ALRI_SDK_PING") == 0);

    /* Test recv with timeout when no data is waiting (should timeout) */
    clock_gettime(CLOCK_MONOTONIC, &start);
    int recv_rc = ar_app_net_recv(ctx, client_fd, recv_buf, sizeof(recv_buf), 40);
    clock_gettime(CLOCK_MONOTONIC, &end);
    assert(recv_rc == AR_APP_ERR_TIMEOUT);

    elapsed_ms = (uint64_t)(end.tv_sec - start.tv_sec) * 1000 +
                 (uint64_t)(end.tv_nsec - start.tv_nsec) / 1000000;
    assert(elapsed_ms < 300);

    ar_app_net_close(ctx, client_fd);
    close(accepted_fd);
    close(server_fd);

    ar_app_context_destroy(ctx);
}

static int g_sink_called = 0;
static char g_last_sink_line[AR_APP_LOG_LINE_MAX];

static void mock_log_sink(
    ar_app_log_level_t level,
    const char *tag,
    const char *json_formatted,
    void *user_data
) {
    (void)level;
    (void)tag;
    (void)user_data;
    g_sink_called++;
    snprintf(g_last_sink_line, sizeof(g_last_sink_line), "%s", json_formatted);
}

static void test_structured_logging(void) {
    const ar_app_descriptor_t *desc = alrios_get_app_descriptor();
    ar_app_context_t *ctx = ar_app_context_create(desc);
    assert(ctx != NULL);

    g_sink_called = 0;
    memset(g_last_sink_line, 0, sizeof(g_last_sink_line));
    ar_app_set_log_sink(ctx, mock_log_sink, NULL);

    int rc = ar_app_log(ctx, AR_LOG_LEVEL_INFO, "System initialized with code=%d", 100);
    assert(rc == AR_APP_OK);
    assert(g_sink_called == 1);

    const char *last = ar_app_context_get_last_log(ctx);
    assert(last != NULL);
    assert(strstr(last, "\"level\":\"INFO\"") != NULL);
    assert(strstr(last, "\"app_id\":\"com.alrigroup.core_test\"") != NULL);
    assert(strstr(last, "\"message\":\"System initialized with code=100\"") != NULL);
    assert(strstr(last, "\"timestamp\":") != NULL);
    assert(strcmp(last, g_last_sink_line) == 0);

    /* Verify JSON escaping for quotes and newlines */
    rc = ar_app_log(ctx, AR_LOG_LEVEL_WARN, "Quote \"escaped\" and \nnewline");
    assert(rc == AR_APP_OK);
    last = ar_app_context_get_last_log(ctx);
    assert(strstr(last, "\\\"escaped\\\"") != NULL);
    assert(strstr(last, "\\nnewline") != NULL);

    ar_app_context_destroy(ctx);
}

int main(void) {
    (void)printf("[TEST] Running MP-023 AR-SDK extended lifecycle, timeout network, storage and logging tests...\n");

    test_app_lifecycle_state_machine();
    (void)printf("  -> Lifecycle state machine: PASS\n");

    test_app_storage_scoping();
    (void)printf("  -> Storage scoping (/data/<app_id>): PASS\n");

    test_secret_vault_no_direct_host_paths();
    (void)printf("  -> Vault secret protection & zero host path exposure: PASS\n");

    test_mandatory_timeout_networking();
    (void)printf("  -> Mandatory timeout network API: PASS\n");

    test_structured_logging();
    (void)printf("  -> Structured JSON logging: PASS\n");

    (void)printf("[TEST] MP-023 AR-SDK test suite: ALL PASS\n");
    return 0;
}
