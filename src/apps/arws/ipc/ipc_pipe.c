/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "ipc.h"
#include "aros_hal.h"
#include <string.h>

int ipc_send_frame(int fd, int type, const unsigned char *data, int length) {
    unsigned char header[5];
    header[0] = (unsigned char)(length >> 24) & 0xFF;
    header[1] = (unsigned char)(length >> 16) & 0xFF;
    header[2] = (unsigned char)(length >> 8) & 0xFF;
    header[3] = (unsigned char)length & 0xFF;
    header[4] = (unsigned char)type;

    if (ar_socket_send(fd, header, 5) != 5)
        return -1;

    if (length > 0 && data) {
        int written = 0;
        while (written < length) {
            int n = ar_socket_send(fd, data + written, length - written);
            if (n <= 0) return -1;
            written += n;
        }
    }
    return 0;
}

int ipc_recv_frame(int fd, IpcFrame *frame) {
    unsigned char header[5];
    int n = 0, r;
    while (n < 5) {
        r = ar_socket_recv(fd, header + n, 5 - n);
        if (r <= 0) return -1;
        n += r;
    }

    frame->length = ((int)header[0] << 24) |
                    ((int)header[1] << 16) |
                    ((int)header[2] << 8) |
                    ((int)header[3]);
    frame->type = header[4];

    if (frame->length > 0) {
        frame->data = (unsigned char *)ar_mem_alloc(frame->length + 1);
        if (!frame->data) return -1;
        n = 0;
        while (n < frame->length) {
            r = ar_socket_recv(fd, frame->data + n, frame->length - n);
            if (r <= 0) { ar_mem_free(frame->data); return -1; }
            n += r;
        }
        frame->data[frame->length] = '\0';
    } else {
        frame->data = NULL;
    }
    return 0;
}

void ipc_free_frame(IpcFrame *frame) {
    if (frame->data) {
        ar_mem_free(frame->data);
        frame->data = NULL;
    }
    frame->length = 0;
}

int ipc_send_raw(int fd, const unsigned char *data, int length) {
    int written = 0, n;
    while (written < length) {
        n = ar_socket_send(fd, data + written, length - written);
        if (n <= 0) return -1;
        written += n;
    }
    return 0;
}

int ipc_recv_raw(int fd, unsigned char *buf, int maxlen) {
    return ar_socket_recv(fd, buf, maxlen);
}
