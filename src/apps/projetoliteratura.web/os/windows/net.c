/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>

static int os_connect_timeout(int fd, const char *addr, uint16_t port, int timeout_ms) {
    SOCKET s = (SOCKET)fd;
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = inet_addr(addr);
    sa.sin_port = htons(port);

    u_long nonblock = 1;
    ioctlsocket(s, FIONBIO, &nonblock);

    int rc = connect(s, (const struct sockaddr *)&sa, sizeof(sa));
    if (rc == 0 || WSAGetLastError() != WSAEWOULDBLOCK) {
        u_long block = 0;
        ioctlsocket(s, FIONBIO, &block);
        return (rc == 0) ? 0 : -1;
    }

    fd_set wfds;
    struct timeval tv;
    FD_ZERO(&wfds);
    FD_SET(s, &wfds);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int sel = select(0, NULL, &wfds, NULL, &tv);
    u_long block = 0;
    ioctlsocket(s, FIONBIO, &block);
    if (sel <= 0) return -1;

    int soerr = 0;
    int slen = sizeof(soerr);
    getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&soerr, &slen);
    return (soerr == 0) ? 0 : -1;
}

static int os_set_recv_timeout(int fd, int timeout_ms) {
    DWORD to = (DWORD)timeout_ms;
    return setsockopt((SOCKET)fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&to, sizeof(to));
}
