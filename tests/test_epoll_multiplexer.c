/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */

#include "alrios/ipc_channel.h"

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

static void test_basic_creation_and_flags(void) {
    int sv[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

    const int ep = alrios_ipc_create_epoll_channel(sv[0]);
    assert(ep >= 0);

    /* Verify O_NONBLOCK was applied to guest socket */
    const int sock_flags = fcntl(sv[0], F_GETFL, 0);
    assert(sock_flags >= 0);
    assert((sock_flags & O_NONBLOCK) != 0);

    /* Verify epoll fd has FD_CLOEXEC set */
    const int ep_flags = fcntl(ep, F_GETFD, 0);
    assert(ep_flags >= 0);
    assert((ep_flags & FD_CLOEXEC) != 0);

    close(ep);
    close(sv[0]);
    close(sv[1]);
}

static void test_invalid_parameters(void) {
    assert(alrios_ipc_create_epoll_channel(-1) < 0);
    assert(alrios_ipc_create_epoll_channel(-42) < 0);
    assert(alrios_ipc_create_epoll_channel(65535) < 0);

    assert(alrios_ipc_set_nonblocking(-1) < 0);
    assert(alrios_ipc_set_nonblocking(65535) < 0);

    int ready_fd = -1;
    uint32_t events = 0;
    assert(alrios_ipc_epoll_wait(-1, &ready_fd, &events, 0) < 0);
    assert(alrios_ipc_epoll_add(-1, 0, EPOLLIN) < 0);
    assert(alrios_ipc_epoll_mod(-1, 0, EPOLLOUT) < 0);
    assert(alrios_ipc_epoll_del(-1, 0) < 0);
}

static void test_multiplexer_event_dispatch(void) {
    int sv[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

    const int ep = alrios_ipc_create_epoll_channel(sv[0]);
    assert(ep >= 0);

    /* No events initially pending with 0ms timeout */
    int ready_fd = -1;
    uint32_t events = 0;
    int rc = alrios_ipc_epoll_wait(ep, &ready_fd, &events, 0);
    assert(rc == 0);

    /* Send payload from peer */
    const char *msg = "ALRIOS_MULTIPLEX_TEST";
    const size_t msg_len = strlen(msg);
    const ssize_t sent = write(sv[1], msg, msg_len);
    assert(sent == (ssize_t)msg_len);

    /* epoll_wait should immediately report sv[0] is readable */
    rc = alrios_ipc_epoll_wait(ep, &ready_fd, &events, 100);
    assert(rc == 1);
    assert(ready_fd == sv[0]);
    assert((events & EPOLLIN) != 0);

    /* Drain the socket */
    char buf[64];
    memset(buf, 0, sizeof(buf));
    const ssize_t recvd = read(sv[0], buf, sizeof(buf) - 1);
    assert(recvd == (ssize_t)msg_len);
    assert(memcmp(buf, msg, msg_len) == 0);

    /* Subsequent wait should time out */
    rc = alrios_ipc_epoll_wait(ep, &ready_fd, &events, 0);
    assert(rc == 0);

    close(ep);
    close(sv[0]);
    close(sv[1]);
}

static void test_multi_socket_multiplexing(void) {
    int sv1[2];
    int sv2[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv1) == 0);
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv2) == 0);

    const int ep = alrios_ipc_create_epoll_channel(sv1[0]);
    assert(ep >= 0);

    /* Add second socketpair to the multiplexer */
    assert(alrios_ipc_epoll_add(ep, sv2[0], EPOLLIN | EPOLLRDHUP) == 0);

    /* Send message to second socket */
    const char msg2[] = "CHANNEL_2_PAYLOAD";
    const ssize_t sent = write(sv2[1], msg2, sizeof(msg2));
    assert(sent == (ssize_t)sizeof(msg2));

    int ready_fd = -1;
    uint32_t events = 0;
    int rc = alrios_ipc_epoll_wait(ep, &ready_fd, &events, 50);
    assert(rc == 1);
    assert(ready_fd == sv2[0]);
    assert((events & EPOLLIN) != 0);

    char buf[64];
    const ssize_t recvd = read(sv2[0], buf, sizeof(buf));
    assert(recvd == (ssize_t)sizeof(msg2));

    /* Modify events on sv2[0] */
    assert(alrios_ipc_epoll_mod(ep, sv2[0], EPOLLIN) == 0);

    /* Remove sv2[0] from epoll multiplexer */
    assert(alrios_ipc_epoll_del(ep, sv2[0]) == 0);

    close(ep);
    close(sv1[0]);
    close(sv1[1]);
    close(sv2[0]);
    close(sv2[1]);
}

int main(void) {
    test_basic_creation_and_flags();
    test_invalid_parameters();
    test_multiplexer_event_dispatch();
    test_multi_socket_multiplexing();

    printf("TASK-020 (Epoll IPC Multiplexer): PASS\n");
    return 0;
}
