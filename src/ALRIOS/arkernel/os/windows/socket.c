/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include <winsock2.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

static int os_socket_reuseaddr(int fd, int enable) {
    BOOL opt = enable ? TRUE : FALSE;
    return setsockopt((SOCKET)fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
}

static int os_socket_create(int type) {
    static int wsock_inited = 0;
    if (!wsock_inited) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return -1;
        wsock_inited = 1;
    }
    SOCKET s = socket(AF_INET, type, 0);
    if (s == INVALID_SOCKET) return -1;
    BOOL nodelay = TRUE;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&nodelay, sizeof(nodelay));
    return (int)s;
}

static int os_socket_bind(int fd, const char *addr, uint16_t port) {
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = inet_addr(addr);
    if (sa.sin_addr.s_addr == INADDR_NONE) return -EINVAL;
    if (bind((SOCKET)fd, (struct sockaddr *)&sa, sizeof(sa)) == SOCKET_ERROR) return -1;
    return 0;
}

static int os_socket_listen(int fd, int backlog) {
    if (listen((SOCKET)fd, backlog) == SOCKET_ERROR) return -1;
    return 0;
}

static int os_socket_accept(int fd) {
    SOCKET client = accept((SOCKET)fd, NULL, NULL);
    if (client == INVALID_SOCKET) return -1;
    return (int)client;
}

static int os_socket_connect(int fd, const char *addr, uint16_t port) {
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = inet_addr(addr);
    if (sa.sin_addr.s_addr == INADDR_NONE) return -EINVAL;
    if (connect((SOCKET)fd, (struct sockaddr *)&sa, sizeof(sa)) == SOCKET_ERROR) return -1;
    return 0;
}

static int os_socket_send(int fd, const void *data, size_t len) {
    int sent = send((SOCKET)fd, data, (int)len, 0);
    if (sent == SOCKET_ERROR) return -1;
    return sent;
}

static int os_socket_recv(int fd, void *buf, size_t len) {
    int received = recv((SOCKET)fd, buf, (int)len, 0);
    if (received == SOCKET_ERROR) return -1;
    return received;
}

static int os_socket_set_nonblock(int fd) {
    u_long mode = 1;
    return ioctlsocket((SOCKET)fd, FIONBIO, &mode);
}

static void os_socket_close(int fd) {
    closesocket((SOCKET)fd);
}
