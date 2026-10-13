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

#include "alrios/deploy/socket_handover.h"
#include "alrios/deploy/drain.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "[FAIL] %s:%d: Assertion '%s' failed\n", __FILE__, __LINE__, #cond); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static uint64_t test_time_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0U;
    }
    return ((uint64_t)ts.tv_sec * 1000ULL) + ((uint64_t)ts.tv_nsec / 1000000ULL);
}

static void test_sleep_ms(uint32_t ms) {
    struct timespec req;
    req.tv_sec = (time_t)(ms / 1000U);
    req.tv_nsec = (long)((ms % 1000U) * 1000000ULL);
    while (nanosleep(&req, &req) != 0 && errno == EINTR) {
    }
}

/*
 * Test 1: SCM_RIGHTS listener transfer and route management
 * Validates atomic listener transfer and generation tracking.
 */
static void test_socket_handover_transfer(void) {
    printf("[1/4] Testing SCM_RIGHTS listener inheritance and route swap...\n");

    int sv[2];
    TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

    /* Create listener socket on loopback port 0 (ephemeral) */
    int listener_fd = socket(AF_INET, SOCK_STREAM, 0);
    TEST_ASSERT(listener_fd >= 0);

    int opt = 1;
    TEST_ASSERT(setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == 0);

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sin.sin_port = 0;

    TEST_ASSERT(bind(listener_fd, (struct sockaddr *)&sin, sizeof(sin)) == 0);
    TEST_ASSERT(listen(listener_fd, 16) == 0);

    socklen_t sin_len = sizeof(sin);
    TEST_ASSERT(getsockname(listener_fd, (struct sockaddr *)&sin, &sin_len) == 0);
    uint16_t port = ntohs(sin.sin_port);

    /* Initialize route with Generation 1 */
    alrios_listener_route_t route;
    TEST_ASSERT(alrios_listener_route_init(&route, listener_fd, 1U) == 0);

    /* Transfer listener socket via SCM_RIGHTS across sv */
    alrios_handover_peer_policy_t policy;
    memset(&policy, 0, sizeof(policy));
    policy.flags = ALRIOS_HANDOVER_PEER_SAME_EUID;

    TEST_ASSERT(alrios_socket_handover_send(sv[0], listener_fd, 2U, &policy, 1000) == 0);

    int received_fd = -1;
    uint64_t recv_gen = 0;
    alrios_handover_peer_credentials_t creds;
    memset(&creds, 0, sizeof(creds));

    TEST_ASSERT(alrios_socket_handover_receive(sv[1], &received_fd, &recv_gen, &creds, &policy, 1000) == 0);
    TEST_ASSERT(received_fd >= 0);
    TEST_ASSERT(recv_gen == 2U);
    TEST_ASSERT(creds.available == 1);

    /* Swap route with new generation */
    int prev_fd = -1;
    TEST_ASSERT(alrios_listener_route_publish(&route, received_fd, recv_gen, &prev_fd) == 0);
    TEST_ASSERT(prev_fd == listener_fd);

    /* Acquire route descriptor */
    int acq_fd = -1;
    uint64_t acq_gen = 0;
    TEST_ASSERT(alrios_listener_route_acquire(&route, &acq_fd, &acq_gen) == 0);
    TEST_ASSERT(acq_fd >= 0);
    TEST_ASSERT(acq_gen == 2U);
    TEST_ASSERT(acq_fd != received_fd); /* Distinct duplicate descriptor */

    /* Verify connectivity through acquired route */
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    TEST_ASSERT(client_fd >= 0);

    struct sockaddr_in dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    dst.sin_port = htons(port);

    TEST_ASSERT(connect(client_fd, (struct sockaddr *)&dst, sizeof(dst)) == 0);

    /* Accept connection from inherited listener */
    struct pollfd pfd;
    pfd.fd = acq_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    TEST_ASSERT(poll(&pfd, 1, 1000) > 0);

    int accepted_fd = accept(acq_fd, NULL, NULL);
    TEST_ASSERT(accepted_fd >= 0);

    /* Data roundtrip */
    const char ping[] = "PING-HANDOVER";
    TEST_ASSERT(write(client_fd, ping, sizeof(ping)) == (ssize_t)sizeof(ping));

    char buf[32];
    TEST_ASSERT(read(accepted_fd, buf, sizeof(buf)) == (ssize_t)sizeof(ping));
    TEST_ASSERT(memcmp(buf, ping, sizeof(ping)) == 0);

    close(client_fd);
    close(accepted_fd);
    close(acq_fd);
    close(prev_fd);

    int final_fd = -1;
    TEST_ASSERT(alrios_listener_route_take(&route, &final_fd) == 0);
    TEST_ASSERT(final_fd == received_fd);
    close(final_fd);

    close(sv[0]);
    close(sv[1]);
    printf("    -> SCM_RIGHTS listener transfer passed\n");
}

/*
 * Test 2: In-flight WebSocket stream survival during handover & drain
 * Verifies that active, established streaming connections survive
 * while listener transitions and drain occurs.
 */
static void test_websocket_stream_survival(void) {
    printf("[2/4] Testing in-flight WebSocket / HTTP stream survival...\n");

    /* Create listener */
    int listener_fd = socket(AF_INET, SOCK_STREAM, 0);
    TEST_ASSERT(listener_fd >= 0);
    int opt = 1;
    TEST_ASSERT(setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == 0);

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sin.sin_port = 0;

    TEST_ASSERT(bind(listener_fd, (struct sockaddr *)&sin, sizeof(sin)) == 0);
    TEST_ASSERT(listen(listener_fd, 8) == 0);

    socklen_t sin_len = sizeof(sin);
    TEST_ASSERT(getsockname(listener_fd, (struct sockaddr *)&sin, &sin_len) == 0);
    uint16_t port = ntohs(sin.sin_port);

    /* Establish active WebSocket connection BEFORE handover/drain */
    int client_ws = socket(AF_INET, SOCK_STREAM, 0);
    TEST_ASSERT(client_ws >= 0);

    struct sockaddr_in dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    dst.sin_port = htons(port);

    TEST_ASSERT(connect(client_ws, (struct sockaddr *)&dst, sizeof(dst)) == 0);

    int server_ws = accept(listener_fd, NULL, NULL);
    TEST_ASSERT(server_ws >= 0);

    /* Initialize drain tracker */
    alrios_drain_config_t cfg;
    cfg.drain_timeout_ms = 3000U;
    cfg.grace_period_ms = 500U;
    cfg.max_streams = 128U;

    alrios_drain_tracker_t tracker;
    TEST_ASSERT(alrios_drain_tracker_init(&tracker, &cfg) == ALRIOS_DRAIN_OK);

    /* Register active WebSocket stream */
    uint64_t stream_id = 1001ULL;
    TEST_ASSERT(alrios_drain_stream_register(&tracker, server_ws, stream_id, NULL) == ALRIOS_DRAIN_OK);
    TEST_ASSERT(alrios_drain_active_count(&tracker) == 1U);

    /* Begin drain: listener will no longer take new streams for this slot */
    TEST_ASSERT(alrios_drain_begin(&tracker) == ALRIOS_DRAIN_OK);
    TEST_ASSERT(alrios_drain_is_draining(&tracker) == true);

    /*
     * Stream must remain fully alive and functional during drain.
     * Exchange bidirectional traffic while draining is in effect.
     */
    const char ws_client_msg[] = "WS_FRAME:CLIENT_ALIVE";
    TEST_ASSERT(write(client_ws, ws_client_msg, sizeof(ws_client_msg)) == (ssize_t)sizeof(ws_client_msg));

    char srv_buf[64];
    TEST_ASSERT(read(server_ws, srv_buf, sizeof(srv_buf)) == (ssize_t)sizeof(ws_client_msg));
    TEST_ASSERT(strcmp(srv_buf, ws_client_msg) == 0);

    const char ws_server_msg[] = "WS_FRAME:SERVER_DRAINING_ACK";
    TEST_ASSERT(write(server_ws, ws_server_msg, sizeof(ws_server_msg)) == (ssize_t)sizeof(ws_server_msg));

    char cli_buf[64];
    TEST_ASSERT(read(client_ws, cli_buf, sizeof(cli_buf)) == (ssize_t)sizeof(ws_server_msg));
    TEST_ASSERT(strcmp(cli_buf, ws_server_msg) == 0);

    /* Check tracker status while stream is still running */
    TEST_ASSERT(alrios_drain_poll(&tracker) == 1); /* Still active */

    /* Complete stream gracefully */
    TEST_ASSERT(alrios_drain_stream_complete(&tracker, stream_id) == ALRIOS_DRAIN_OK);
    TEST_ASSERT(alrios_drain_active_count(&tracker) == 0U);

    /* Wait should return OK immediately now */
    TEST_ASSERT(alrios_drain_wait_complete(&tracker, 1000) == ALRIOS_DRAIN_OK);

    alrios_drain_stats_t stats;
    TEST_ASSERT(alrios_drain_get_stats(&tracker, &stats) == ALRIOS_DRAIN_OK);
    TEST_ASSERT(stats.total_accepted == 1U);
    TEST_ASSERT(stats.total_completed == 1U);
    TEST_ASSERT(stats.total_timed_out == 0U);
    TEST_ASSERT(stats.active_streams == 0U);

    alrios_drain_tracker_destroy(&tracker);

    close(client_ws);
    close(server_ws);
    close(listener_fd);

    printf("    -> In-flight WebSocket stream survival passed\n");
}

/*
 * Test 3: Configurable drain timeout enforcement
 * Verifies that the drain tracker respects custom timeouts
 * and flags timeout error if streams outstay the limit.
 */
static void test_drain_timeout_configuration(void) {
    printf("[3/4] Testing configurable drain timeout and expiry...\n");

    alrios_drain_config_t cfg;
    cfg.drain_timeout_ms = 200U; /* 200 ms bounded timeout */
    cfg.grace_period_ms = 50U;
    cfg.max_streams = 10U;

    alrios_drain_tracker_t tracker;
    TEST_ASSERT(alrios_drain_tracker_init(&tracker, &cfg) == ALRIOS_DRAIN_OK);

    /* Register stream that will NOT complete */
    TEST_ASSERT(alrios_drain_stream_register(&tracker, -1, 42ULL, NULL) == ALRIOS_DRAIN_OK);
    TEST_ASSERT(alrios_drain_begin(&tracker) == ALRIOS_DRAIN_OK);

    uint64_t t0 = test_time_ms();
    int rc = alrios_drain_wait_complete(&tracker, 200);
    uint64_t elapsed = test_time_ms() - t0;

    TEST_ASSERT(rc == ALRIOS_DRAIN_ERR_TIMEOUT);
    TEST_ASSERT(elapsed >= 180U); /* Must have waited for the configured duration */

    alrios_drain_stats_t stats;
    TEST_ASSERT(alrios_drain_get_stats(&tracker, &stats) == ALRIOS_DRAIN_OK);
    TEST_ASSERT(stats.total_timed_out == 1U);

    alrios_drain_tracker_destroy(&tracker);
    printf("    -> Configurable drain timeout enforcement passed\n");
}

/*
 * Test 4: No blind SIGKILL at 100ms - Graceful process drain
 * Tests alrios_drain_process_await against a child process.
 * Verifies child gets time to complete its drain without getting SIGKILL'd at 100ms.
 */
static void test_no_blind_sigkill(void) {
    printf("[4/4] Testing elimination of blind SIGKILL at 100ms...\n");

    /* Fork a child process simulating an app server draining connections */
    pid_t pid = fork();
    TEST_ASSERT(pid >= 0);

    if (pid == 0) {
        /*
         * Child process:
         * Simulates ongoing WebSocket session requiring 300ms to gracefully conclude.
         * If the old supervisor killed with SIGKILL at 100ms, child would be terminated
         * prematurely with SIGKILL.
         */
        test_sleep_ms(300U);
        _exit(42);
    }

    /*
     * Parent:
     * Await drain with a 600ms timeout.
     * The process must finish gracefully on its own with exit code 42,
     * NOT by SIGKILL.
     */
    uint64_t t0 = test_time_ms();
    int exit_status = -1;
    int rc = alrios_drain_process_await(pid, 600U, 100U, &exit_status);
    uint64_t elapsed = test_time_ms() - t0;

    TEST_ASSERT(rc == ALRIOS_DRAIN_OK);
    TEST_ASSERT(exit_status == 42);
    TEST_ASSERT(elapsed >= 250U); /* Truly waited without premature kill */

    /* Also verify stubborn process escalation behavior: timeout -> SIGTERM -> SIGKILL */
    pid_t stubborn = fork();
    TEST_ASSERT(stubborn >= 0);
    if (stubborn == 0) {
        /* Block signals and sleep indefinitely */
        sigset_t set;
        sigfillset(&set);
        sigdelset(&set, SIGKILL);
        sigprocmask(SIG_SETMASK, &set, NULL);
        for (;;) {
            sleep(10);
        }
    }

    exit_status = 0;
    /* Timeout 100ms, grace 100ms -> escalates to SIGKILL */
    rc = alrios_drain_process_await(stubborn, 100U, 100U, &exit_status);
    TEST_ASSERT(rc == ALRIOS_DRAIN_ERR_TIMEOUT);
    TEST_ASSERT(exit_status == -SIGKILL);

    printf("    -> Elimination of blind SIGKILL passed\n");
}

int main(void) {
    printf("=== ALRIOS Integration: SCM_RIGHTS Listener Handover & Connection Drain ===\n");
    test_socket_handover_transfer();
    test_websocket_stream_survival();
    test_drain_timeout_configuration();
    test_no_blind_sigkill();
    printf("=== All MP-019 tests PASSED ===\n");
    return 0;
}
