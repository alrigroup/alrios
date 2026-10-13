/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/ipc/guest_channel.h"
#include "alrios/supervisor/guest_router.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(cond) do { \
    if (!(cond)) { \
        (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                      __FILE__, __LINE__, #cond); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void test_peer_credentials_and_handshake(void) {
    int sv[2];
    ar_guest_channel_t *ch_host = NULL;
    ar_guest_channel_t *ch_guest = NULL;
    ar_guest_peer_identity_t host_peer;
    ar_guest_peer_identity_t guest_peer;
    ar_guest_identity_policy_t host_policy;
    ar_guest_channel_config_t host_cfg;
    int rc;

    rc = ar_guest_channel_socketpair(sv);
    CHECK(rc == AR_GUEST_OK);

    memset(&host_policy, 0, sizeof(host_policy));
    host_policy.match_flags = AR_GUEST_MATCH_UID;
    host_policy.expected.uid = geteuid();

    memset(&host_cfg, 0, sizeof(host_cfg));
    host_cfg.max_payload_size = 4096U;
    host_cfg.io_timeout_ms = 1000;
    host_cfg.peer_policy = host_policy;

    rc = ar_guest_channel_attach(sv[0], &host_cfg, &ch_host);
    CHECK(rc == AR_GUEST_OK);

    rc = ar_guest_channel_attach(sv[1], NULL, &ch_guest);
    CHECK(rc == AR_GUEST_OK);

    rc = ar_guest_channel_peer(ch_host, &host_peer);
    CHECK(rc == AR_GUEST_OK);
    CHECK(host_peer.pid == (uint32_t)getpid());
    CHECK(host_peer.uid == (uint32_t)geteuid());

    rc = ar_guest_channel_peer(ch_guest, &guest_peer);
    CHECK(rc == AR_GUEST_OK);
    CHECK(guest_peer.pid == (uint32_t)getpid());
    CHECK(guest_peer.uid == (uint32_t)geteuid());

    /* Test send and receive */
    {
        const char *msg = "hello sovereign kernel";
        ar_guest_frame_header_t hdr;
        char recv_buf[128];

        rc = ar_guest_channel_send(ch_guest, 101U, 0U, 42ULL, msg, (uint32_t)strlen(msg));
        CHECK(rc == AR_GUEST_OK);

        memset(&hdr, 0, sizeof(hdr));
        memset(recv_buf, 0, sizeof(recv_buf));
        rc = ar_guest_channel_receive(ch_host, &hdr, recv_buf, sizeof(recv_buf));
        CHECK(rc == AR_GUEST_OK);
        CHECK(hdr.message_type == 101U);
        CHECK(hdr.request_id == 42ULL);
        CHECK(hdr.payload_length == (uint32_t)strlen(msg));
        CHECK(memcmp(recv_buf, msg, strlen(msg)) == 0);
    }

    ar_guest_channel_close(ch_host);
    ar_guest_channel_close(ch_guest);
}

static void test_peer_identity_policy_rejection(void) {
    int sv[2];
    ar_guest_channel_t *ch = NULL;
    ar_guest_channel_config_t cfg;
    int rc;

    rc = ar_guest_channel_socketpair(sv);
    CHECK(rc == AR_GUEST_OK);

    memset(&cfg, 0, sizeof(cfg));
    cfg.peer_policy.match_flags = AR_GUEST_MATCH_UID;
    cfg.peer_policy.expected.uid = geteuid() + 99999U; /* Wrong UID */

    rc = ar_guest_channel_attach(sv[0], &cfg, &ch);
    CHECK(rc == AR_GUEST_ERR_AUTH);
    CHECK(ch == NULL);

    (void)close(sv[0]);
    (void)close(sv[1]);
}

static void test_malformed_framing_and_bounds_rejection(void) {
    int sv[2];
    ar_guest_channel_t *ch_host = NULL;
    ar_guest_channel_config_t cfg;
    int rc;

    rc = ar_guest_channel_socketpair(sv);
    CHECK(rc == AR_GUEST_OK);

    memset(&cfg, 0, sizeof(cfg));
    cfg.max_payload_size = 512U;
    cfg.io_timeout_ms = 100;

    rc = ar_guest_channel_attach(sv[0], &cfg, &ch_host);
    CHECK(rc == AR_GUEST_OK);

    /* 1. Send garbage bytes directly over sv[1] (no credentials / bad magic) */
    {
        uint8_t garbage[32];
        ar_guest_frame_header_t hdr;
        char buf[64];

        memset(garbage, 0xA5, sizeof(garbage));
        (void)write(sv[1], garbage, sizeof(garbage));

        rc = ar_guest_channel_receive(ch_host, &hdr, buf, sizeof(buf));
        CHECK(rc == AR_GUEST_ERR_AUTH || rc == AR_GUEST_ERR_PROTOCOL);
    }

    /* 2. Test payload exceeding channel limit */
    {
        uint8_t large_payload[600];
        ar_guest_channel_t *ch_guest = NULL;

        rc = ar_guest_channel_attach(sv[1], NULL, &ch_guest);
        CHECK(rc == AR_GUEST_OK);

        memset(large_payload, 0x42, sizeof(large_payload));
        /* ch_guest has default 64KB max, so send succeeds at guest */
        rc = ar_guest_channel_send(ch_guest, 1U, 0U, 1ULL, large_payload, sizeof(large_payload));
        CHECK(rc == AR_GUEST_OK);

        /* Host has max_payload_size = 512, so receive must reject with ERR_LIMIT */
        {
            ar_guest_frame_header_t hdr;
            char buf[1024];
            rc = ar_guest_channel_receive(ch_host, &hdr, buf, sizeof(buf));
            CHECK(rc == AR_GUEST_ERR_LIMIT);
        }

        ar_guest_channel_close(ch_guest);
    }

    ar_guest_channel_close(ch_host);
}

static void test_buffer_too_small_preservation(void) {
    int sv[2];
    ar_guest_channel_t *ch_host = NULL;
    ar_guest_channel_t *ch_guest = NULL;
    const char *payload = "1234567890abcdefghijklmnopqrstuvwxyz";
    ar_guest_frame_header_t hdr;
    char tiny_buf[8];
    char full_buf[64];
    int rc;

    rc = ar_guest_channel_socketpair(sv);
    CHECK(rc == AR_GUEST_OK);

    rc = ar_guest_channel_attach(sv[0], NULL, &ch_host);
    CHECK(rc == AR_GUEST_OK);
    rc = ar_guest_channel_attach(sv[1], NULL, &ch_guest);
    CHECK(rc == AR_GUEST_OK);

    rc = ar_guest_channel_send(ch_guest, 12U, 0U, 99ULL, payload, (uint32_t)strlen(payload));
    CHECK(rc == AR_GUEST_OK);

    /* First read into undersized buffer */
    rc = ar_guest_channel_receive(ch_host, &hdr, tiny_buf, sizeof(tiny_buf));
    CHECK(rc == AR_GUEST_ERR_BUFFER_TOO_SMALL);
    CHECK(hdr.payload_length == (uint32_t)strlen(payload));

    /* Datagram must not have been dropped - read again with adequate buffer */
    memset(full_buf, 0, sizeof(full_buf));
    rc = ar_guest_channel_receive(ch_host, &hdr, full_buf, sizeof(full_buf));
    CHECK(rc == AR_GUEST_OK);
    CHECK(hdr.payload_length == (uint32_t)strlen(payload));
    CHECK(memcmp(full_buf, payload, strlen(payload)) == 0);

    ar_guest_channel_close(ch_host);
    ar_guest_channel_close(ch_guest);
}

typedef struct test_counter {
    int count;
} test_counter_t;

static int on_guest_message(
    ar_guest_router_t *router,
    const char *app_id,
    const ar_guest_frame_header_t *header,
    const void *payload,
    void *user_data) {
    test_counter_t *ctr = (test_counter_t *)user_data;
    (void)router;
    (void)app_id;
    (void)header;
    (void)payload;
    ctr->count++;
    return 0;
}

static void test_router_backpressure_and_flood_isolation(void) {
    ar_guest_router_t *router = NULL;
    ar_guest_router_config_t rcfg;
    int guest1_fd = -1;
    int guest2_fd = -1;
    ar_guest_channel_t *ch_guest1 = NULL;
    ar_guest_channel_t *ch_guest2 = NULL;
    ar_guest_route_stats_t stats1;
    ar_guest_route_stats_t stats2;
    test_counter_t counter = {0};
    int rc;
    int i;

    memset(&rcfg, 0, sizeof(rcfg));
    rcfg.max_guests = 8U;
    rcfg.max_queue_depth = 4U;
    rcfg.max_payload_size = 1024U;
    rcfg.flood_policy = AR_GUEST_FLOOD_BACKPRESSURE;

    rc = ar_guest_router_create(&rcfg, &router);
    CHECK(rc == AR_GUEST_OK);

    rc = ar_guest_router_register(router, "app.service.one", NULL, &guest1_fd);
    CHECK(rc == AR_GUEST_OK);
    CHECK(guest1_fd >= 0);

    rc = ar_guest_router_register(router, "app.service.two", NULL, &guest2_fd);
    CHECK(rc == AR_GUEST_OK);
    CHECK(guest2_fd >= 0);

    rc = ar_guest_channel_attach(guest1_fd, NULL, &ch_guest1);
    CHECK(rc == AR_GUEST_OK);

    rc = ar_guest_channel_attach(guest2_fd, NULL, &ch_guest2);
    CHECK(rc == AR_GUEST_OK);

    CHECK(ar_guest_router_has_guest(router, "app.service.one") == 1);
    CHECK(ar_guest_router_has_guest(router, "app.service.two") == 1);
    CHECK(ar_guest_router_has_guest(router, "app.service.missing") == 0);

    /* Fill socket buffer and router queue of guest1 to trigger backpressure */
    for (i = 0; i < 500; ++i) {
        char payload[32];
        snprintf(payload, sizeof(payload), "flood-%d", i);
        rc = ar_guest_router_send(router, "app.service.one", 1U, 0U, (uint64_t)i, payload, (uint32_t)strlen(payload));
        if (rc == AR_GUEST_ERR_BACKPRESSURE || rc == AR_GUEST_ERR_WOULD_BLOCK) {
            break;
        }
        CHECK(rc == AR_GUEST_OK);
    }
    CHECK(rc == AR_GUEST_ERR_BACKPRESSURE);

    /* Verify guest2 is unimpacted and receives messages */
    rc = ar_guest_router_send(router, "app.service.two", 2U, 0U, 999ULL, "test", 4U);
    CHECK(rc == AR_GUEST_OK);

    /* Guest 2 drains message */
    {
        ar_guest_frame_header_t hdr;
        char buf[32];
        rc = ar_guest_channel_receive(ch_guest2, &hdr, buf, sizeof(buf));
        CHECK(rc == AR_GUEST_OK);
        CHECK(hdr.message_type == 2U);
        CHECK(hdr.request_id == 999ULL);
    }

    /* Inbound dispatch from guest to host supervisor */
    rc = ar_guest_channel_send(ch_guest2, 44U, 0U, 777ULL, "ping", 4U);
    CHECK(rc == AR_GUEST_OK);

    rc = ar_guest_router_dispatch(router, 100, on_guest_message, &counter);
    CHECK(rc > 0);
    CHECK(counter.count >= 1);

    rc = ar_guest_router_get_stats(router, "app.service.one", &stats1);
    CHECK(rc == AR_GUEST_OK);
    CHECK(stats1.messages_dropped > 0U);

    rc = ar_guest_router_get_stats(router, "app.service.two", &stats2);
    CHECK(rc == AR_GUEST_OK);
    CHECK(stats2.messages_received >= 1U);

    ar_guest_channel_close(ch_guest1);
    ar_guest_channel_close(ch_guest2);
    ar_guest_router_destroy(router);
}

static void test_clean_disconnection(void) {
    ar_guest_router_t *router = NULL;
    int guest_fd = -1;
    ar_guest_channel_t *ch_guest = NULL;
    int rc;

    rc = ar_guest_router_create(NULL, &router);
    CHECK(rc == AR_GUEST_OK);

    rc = ar_guest_router_register(router, "app.ephemeral", NULL, &guest_fd);
    CHECK(rc == AR_GUEST_OK);

    rc = ar_guest_channel_attach(guest_fd, NULL, &ch_guest);
    CHECK(rc == AR_GUEST_OK);

    /* Guest closes its channel */
    ar_guest_channel_close(ch_guest);

    /* Supervisor router dispatch should notice disconnect non-blockingly */
    (void)ar_guest_router_dispatch(router, 50, NULL, NULL);

    /* Unregister should cleanly succeed */
    rc = ar_guest_router_unregister(router, "app.ephemeral");
    CHECK(rc == AR_GUEST_OK);
    CHECK(ar_guest_router_has_guest(router, "app.ephemeral") == 0);

    ar_guest_router_destroy(router);
}

int main(void) {
    (void)printf("[test_guest_isolation] Running peer credentials & handshake test...\n");
    test_peer_credentials_and_handshake();

    (void)printf("[test_guest_isolation] Running identity policy rejection test...\n");
    test_peer_identity_policy_rejection();

    (void)printf("[test_guest_isolation] Running framing & bounds rejection test...\n");
    test_malformed_framing_and_bounds_rejection();

    (void)printf("[test_guest_isolation] Running buffer too small preservation test...\n");
    test_buffer_too_small_preservation();

    (void)printf("[test_guest_isolation] Running router backpressure & flood isolation test...\n");
    test_router_backpressure_and_flood_isolation();

    (void)printf("[test_guest_isolation] Running clean disconnection test...\n");
    test_clean_disconnection();

    (void)printf("[test_guest_isolation] All tests passed successfully.\n");
    return 0;
}
