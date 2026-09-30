/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/guest_channel.h"
#include <unistd.h>
#include <sys/epoll.h>
#include <errno.h>
#include <string.h>

int alrios_guest_router_init(void) {
    return 0;
}

int alrios_guest_router_register_guest(int guest_socket_fd, uint32_t guest_id) {
    if (guest_socket_fd < 0) {
        return -1;
    }
    (void)guest_id;
    pid_t pid = 0;
    if (alrios_guest_channel_check_peer(guest_socket_fd, (uid_t)-1, &pid) < 0) {
        return -2;
    }
    if (alrios_ipc_set_nonblocking(guest_socket_fd) < 0) {
        return -3;
    }
    return 0;
}

int alrios_guest_router_poll_and_route(int epfd, int timeout_ms) {
    if (epfd < 0) {
        return -1;
    }
    int ready_fd = -1;
    uint32_t events = 0;
    int rc = alrios_ipc_epoll_wait(epfd, &ready_fd, &events, timeout_ms);
    if (rc <= 0) {
        return rc;
    }
    if ((events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) != 0) {
        return -2;
    }
    if ((events & EPOLLIN) != 0) {
        alri_ipc_frame_hdr_t hdr;
        memset(&hdr, 0, sizeof(hdr));
        uint8_t buf[1024];
        memset(buf, 0, sizeof(buf));
        int recv_rc = alrios_guest_channel_recv(ready_fd, &hdr, buf, sizeof(buf));
        if (recv_rc < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;
            }
            return -3;
        }
        if (hdr.payload_len > ALRIOS_IPC_MAX_PAYLOAD) {
            return -4;
        }
        return 1;
    }
    return 0;
}

void alrios_guest_router_cleanup(void) {
}
