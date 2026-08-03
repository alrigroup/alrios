/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

static int os_socket_reuseaddr(int fd, int enable) {
    int opt = enable ? 1 : 0;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
}

static int os_socket_create(int type) {
    int fd = socket(AF_INET, type, 0);
    if (fd == -1) return -errno;
    int flag = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
    return fd;
}

static int os_socket_bind(int fd, const char *addr, uint16_t port) {
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    if (inet_pton(AF_INET, addr, &sa.sin_addr) <= 0)
        return -EINVAL;

    if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) == -1)
        return -errno;
    return 0;
}

static int os_socket_listen(int fd, int backlog) {
    if (listen(fd, backlog) == -1) return -errno;
    return 0;
}

static int os_socket_accept(int fd) {
    struct sockaddr_in sa;
    socklen_t slen = sizeof(sa);
    int client = accept(fd, (struct sockaddr *)&sa, &slen);
    if (client == -1) return -errno;
    return client;
}

static int os_socket_connect(int fd, const char *addr, uint16_t port) {
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    if (inet_pton(AF_INET, addr, &sa.sin_addr) <= 0)
        return -EINVAL;

    if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) == -1)
        return -errno;
    return 0;
}

static int os_socket_send(int fd, const void *data, size_t len) {
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
    ssize_t sent = send(fd, data, len, MSG_NOSIGNAL);
    if (sent == -1) return -errno;
    return (int)sent;
}

static int os_socket_recv(int fd, void *buf, size_t len) {
    ssize_t received = recv(fd, buf, len, 0);
    if (received == -1) return -errno;
    return (int)received;
}

static int os_socket_set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void os_socket_close(int fd) {
    close(fd);
}
