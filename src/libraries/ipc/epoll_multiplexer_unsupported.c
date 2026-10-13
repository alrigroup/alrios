/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/ipc_channel.h"

int alrios_ipc_set_nonblocking(int fd) {
    (void)fd;
    return -1;
}

int alrios_ipc_create_epoll_channel(int socket_fd) {
    (void)socket_fd;
    return -1;
}

int alrios_ipc_epoll_wait(int epfd, int *ready_fd, uint32_t *events, int timeout_ms) {
    (void)epfd;
    (void)ready_fd;
    (void)events;
    (void)timeout_ms;
    return -1;
}

int alrios_ipc_epoll_add(int epfd, int socket_fd, uint32_t events) {
    (void)epfd;
    (void)socket_fd;
    (void)events;
    return -1;
}

int alrios_ipc_epoll_mod(int epfd, int socket_fd, uint32_t events) {
    (void)epfd;
    (void)socket_fd;
    (void)events;
    return -1;
}

int alrios_ipc_epoll_del(int epfd, int socket_fd) {
    (void)epfd;
    (void)socket_fd;
    return -1;
}
