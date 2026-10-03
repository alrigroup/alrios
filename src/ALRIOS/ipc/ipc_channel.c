/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */

#include "alrios/ipc_channel.h"
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>

int alri_ipc_send(int socket_fd, alri_ipc_msg_type_t type, const void *payload, uint32_t payload_len) {
    if (socket_fd < 0 || (payload_len > 0 && !payload)) {
        return -1;
    }
    if (payload_len > ALRIOS_IPC_MAX_PAYLOAD) {
        return -2;
    }

    alri_ipc_frame_hdr_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = ALRIOS_IPC_MAGIC;
    hdr.msg_type = (uint16_t)type;
    hdr.flags = 0;
    hdr.payload_len = payload_len;

    struct iovec iov[2];
    iov[0].iov_base = &hdr;
    iov[0].iov_len = sizeof(alri_ipc_frame_hdr_t);

    size_t iovcnt = 1;
    if (payload_len > 0) {
        iov[1].iov_base = (void *)(uintptr_t)payload;
        iov[1].iov_len = (size_t)payload_len;
        iovcnt = 2;
    }

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = iov;
    msg.msg_iovlen = iovcnt;

    size_t total_expected = sizeof(alri_ipc_frame_hdr_t) + (size_t)payload_len;
    ssize_t sent;
    do {
        sent = sendmsg(socket_fd, &msg, MSG_NOSIGNAL);
    } while (sent < 0 && errno == EINTR);

    if (sent < 0 || (size_t)sent != total_expected) {
        return -3;
    }

    return 0;
}

int alri_ipc_recv(int socket_fd, alri_ipc_frame_hdr_t *out_hdr, void *out_buf, uint32_t max_buf_len) {
    if (socket_fd < 0 || !out_hdr) {
        return -1;
    }
    if (max_buf_len > 0 && !out_buf) {
        return -1;
    }

    struct iovec iov[2];
    iov[0].iov_base = out_hdr;
    iov[0].iov_len = sizeof(alri_ipc_frame_hdr_t);

    size_t iovcnt = 1;
    if (max_buf_len > 0 && out_buf) {
        iov[1].iov_base = out_buf;
        iov[1].iov_len = (size_t)max_buf_len;
        iovcnt = 2;
    }

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = iov;
    msg.msg_iovlen = iovcnt;

    ssize_t recvd;
    do {
        recvd = recvmsg(socket_fd, &msg, 0);
    } while (recvd < 0 && errno == EINTR);

    if (recvd < (ssize_t)sizeof(alri_ipc_frame_hdr_t)) {
        return -2;
    }

    if (out_hdr->magic != ALRIOS_IPC_MAGIC) {
        return -3;
    }

    if (out_hdr->payload_len > ALRIOS_IPC_MAX_PAYLOAD || out_hdr->payload_len > max_buf_len) {
        return -5;
    }

    if ((msg.msg_flags & MSG_TRUNC) != 0) {
        return -5;
    }

    if ((size_t)recvd != (sizeof(alri_ipc_frame_hdr_t) + (size_t)out_hdr->payload_len)) {
        return -4;
    }

    return 0;
}
