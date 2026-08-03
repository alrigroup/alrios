/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "ar_ipc.h"
#include "aros_hal.h"
#include <string.h>

int ar_ipc_send_frame(int fd, int type, const void *data, uint32_t len) {
    unsigned char header[5];
    header[0] = (unsigned char)(len >> 24) & 0xFF;
    header[1] = (unsigned char)(len >> 16) & 0xFF;
    header[2] = (unsigned char)(len >> 8) & 0xFF;
    header[3] = (unsigned char)len & 0xFF;
    header[4] = (unsigned char)type;

    uint32_t hwritten = 0;
    while (hwritten < 5) {
        int n = ar_socket_send(fd, header + hwritten, 5 - hwritten);
        if (n <= 0) return -1;
        hwritten += n;
    }

    if (len > 0 && data) {
        uint32_t written = 0;
        int n;
        while (written < len) {
            n = ar_socket_send(fd, (const char*)data + written, len - written);
            if (n <= 0) return -1;
            written += n;
        }
    }
    return 0;
}

int ar_ipc_recv_frame(int fd, int *type, void *buf, uint32_t *len) {
    unsigned char header[5];
    uint32_t n = 0;
    int r;
    while (n < 5) {
        r = ar_socket_recv(fd, header + n, 5 - n);
        if (r <= 0) return -1;
        n += r;
    }

    uint32_t frame_len = ((uint32_t)header[0] << 24) |
                         ((uint32_t)header[1] << 16) |
                         ((uint32_t)header[2] << 8) |
                         ((uint32_t)header[3]);
    *type = header[4];

    if (frame_len > 0) {
        if (frame_len >= *len) return -1;
        n = 0;
        while (n < frame_len) {
            r = ar_socket_recv(fd, (char*)buf + n, frame_len - n);
            if (r <= 0) return -1;
            n += r;
        }
        ((unsigned char*)buf)[frame_len] = '\0';
    }
    *len = frame_len;
    return 0;
}

int ar_ipc_send_raw(int fd, const void *data, uint32_t len) {
    uint32_t written = 0;
    int n;
    while (written < len) {
        n = ar_socket_send(fd, (const char*)data + written, len - written);
        if (n <= 0) return -1;
        written += n;
    }
    return 0;
}

int ar_ipc_recv_raw(int fd, void *buf, uint32_t maxlen) {
    int n = ar_socket_recv(fd, buf, maxlen);
    return n;
}

int ar_ipc_server_start(uint16_t port) {
    int fd = ar_socket_create(1);
    if (fd < 0) return -1;

    ar_socket_reuseaddr(fd, 1);

    if (ar_socket_bind(fd, "127.0.0.1", port) < 0) {
        ar_socket_close(fd);
        return -1;
    }

    if (ar_socket_listen(fd, 256) < 0) {
        ar_socket_close(fd);
        return -1;
    }

    return fd;
}

int ar_ipc_server_stop(int server_fd) {
    if (server_fd >= 0)
        ar_socket_close(server_fd);
    return 0;
}

int ar_ipc_client_connect(const char *host, uint16_t port) {
    int fd = ar_socket_create(1);
    if (fd < 0) return -1;

    if (ar_socket_connect(fd, host, port) < 0) {
        ar_socket_close(fd);
        return -1;
    }

    return fd;
}
