/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/guest_channel.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

int alrios_guest_channel_create(int sv[2]) {
    if (!sv) {
        return -1;
    }
    if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sv) < 0) {
        return -2;
    }
    if (alrios_ipc_set_nonblocking(sv[0]) != 0 || alrios_ipc_set_nonblocking(sv[1]) != 0) {
        const int saved_errno = errno;
        (void)close(sv[0]);
        (void)close(sv[1]);
        sv[0] = -1;
        sv[1] = -1;
        errno = saved_errno;
        return -3;
    }
    if (fcntl(sv[0], F_SETFD, FD_CLOEXEC) < 0 || fcntl(sv[1], F_SETFD, FD_CLOEXEC) < 0) {
        const int saved_errno = errno;
        (void)close(sv[0]);
        (void)close(sv[1]);
        sv[0] = -1;
        sv[1] = -1;
        errno = saved_errno;
        return -4;
    }
    return 0;
}

int alrios_guest_channel_check_peer(int fd, uid_t expected_uid, pid_t *out_pid) {
    if (fd < 0) {
        return -1;
    }
    struct ucred cred;
    socklen_t len = sizeof(cred);
    memset(&cred, 0, sizeof(cred));
    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) < 0) {
        return -2;
    }
    if (expected_uid != (uid_t)-1 && cred.uid != expected_uid) {
        return -3;
    }
    if (out_pid != NULL) {
        *out_pid = cred.pid;
    }
    return 0;
}

int alrios_guest_channel_send(int socket_fd, uint16_t msg_type, const void *payload, uint32_t payload_len) {
    if (socket_fd < 0 || (payload_len > 0 && !payload)) {
        return -1;
    }
    if (payload_len > ALRIOS_IPC_MAX_PAYLOAD) {
        return -2;
    }
    return alri_ipc_send(socket_fd, (alri_ipc_msg_type_t)msg_type, payload, payload_len);
}

int alrios_guest_channel_recv(int socket_fd, alri_ipc_frame_hdr_t *out_hdr, void *out_buf, uint32_t max_buf_len) {
    if (socket_fd < 0 || !out_hdr) {
        return -1;
    }
    if (max_buf_len > 0 && !out_buf) {
        return -1;
    }
    return alri_ipc_recv(socket_fd, out_hdr, out_buf, max_buf_len);
}
