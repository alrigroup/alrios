/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/guest_channel.h"
#include <assert.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

int main(void) {
    int sv[2] = {-1, -1};
    int cr = alrios_guest_channel_create(sv);
    (void)cr;
    assert(cr == 0);
    assert(sv[0] >= 0);
    assert(sv[1] >= 0);

    pid_t pid = 0;
    int pr = alrios_guest_channel_check_peer(sv[0], getuid(), &pid);
    (void)pr;
    assert(pr == 0);
    assert(pid > 0);

    const char *payload = "GUEST_ISO_TEST";
    uint32_t len = (uint32_t)strlen(payload);
    int sr = alrios_guest_channel_send(sv[1], ALRI_IPC_MSG_HEARTBEAT, payload, len);
    (void)sr;
    assert(sr == 0);

    alri_ipc_frame_hdr_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    char rbuf[64];
    memset(rbuf, 0, sizeof(rbuf));
    int rr = alrios_guest_channel_recv(sv[0], &hdr, rbuf, sizeof(rbuf));
    (void)rr;
    assert(rr == 0);
    assert(hdr.magic == ALRIOS_IPC_MAGIC);
    assert(hdr.msg_type == ALRI_IPC_MSG_HEARTBEAT);
    assert(hdr.payload_len == len);
    assert(memcmp(rbuf, payload, len) == 0);

    int bad_sr = alrios_guest_channel_send(sv[1], ALRI_IPC_MSG_HEARTBEAT, payload, ALRIOS_IPC_MAX_PAYLOAD + 1);
    (void)bad_sr;
    assert(bad_sr < 0);

    int epfd = alrios_ipc_create_epoll_channel(sv[0]);
    assert(epfd >= 0);

    int gr = alrios_guest_router_register_guest(sv[0], 1);
    (void)gr;
    assert(gr == 0);

    int pr_poll = alrios_guest_router_poll_and_route(epfd, 10);
    (void)pr_poll;
    assert(pr_poll == 0);

    close(epfd);
    close(sv[0]);
    close(sv[1]);

    printf("MP-014 (Authenticated Host-Guest SOCK_SEQPACKET Routing): PASS\n");
    return 0;
}
