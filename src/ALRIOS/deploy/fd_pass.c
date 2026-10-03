/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "alrios/fd_pass.h"
#include <sys/socket.h>
#include <sys/uio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

int alrios_send_live_fd(int unix_sock, int fd_to_send) {
    if (unix_sock < 0 || fd_to_send < 0) {
        return -1;
    }

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));

    char iov_base = 'F';
    struct iovec iov = {
        .iov_base = &iov_base,
        .iov_len = sizeof(iov_base)
    };
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    union {
        struct cmsghdr cm;
        char control[CMSG_SPACE(sizeof(int))];
    } control_un;
    memset(&control_un, 0, sizeof(control_un));

    msg.msg_control = control_un.control;
    msg.msg_controllen = sizeof(control_un.control);

    struct cmsghdr *cmptr = CMSG_FIRSTHDR(&msg);
    if (!cmptr) {
        return -1;
    }

    cmptr->cmsg_len = CMSG_LEN(sizeof(int));
    cmptr->cmsg_level = SOL_SOCKET;
    cmptr->cmsg_type = SCM_RIGHTS;
    *((int *)(void *)CMSG_DATA(cmptr)) = fd_to_send;

    ssize_t sent = 0;
    do {
        sent = sendmsg(unix_sock, &msg, MSG_NOSIGNAL);
    } while (sent < 0 && errno == EINTR);

    return (sent < 0) ? -1 : 0;
}

int alrios_recv_live_fd(int unix_sock) {
    if (unix_sock < 0) {
        return -1;
    }

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));

    char iov_base = 0;
    struct iovec iov = {
        .iov_base = &iov_base,
        .iov_len = sizeof(iov_base)
    };
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    union {
        struct cmsghdr cm;
        char control[CMSG_SPACE(sizeof(int))];
    } control_un;
    memset(&control_un, 0, sizeof(control_un));

    msg.msg_control = control_un.control;
    msg.msg_controllen = sizeof(control_un.control);

    ssize_t recvd = 0;
    do {
        recvd = recvmsg(unix_sock, &msg, 0);
    } while (recvd < 0 && errno == EINTR);

    if (recvd <= 0) {
        return -1;
    }

    /* Check MSG_CTRUNC or MSG_TRUNC */
    if ((msg.msg_flags & MSG_CTRUNC) || (msg.msg_flags & MSG_TRUNC)) {
        return -2;
    }

    struct cmsghdr *cmptr = CMSG_FIRSTHDR(&msg);
    if (!cmptr ||
        cmptr->cmsg_level != SOL_SOCKET ||
        cmptr->cmsg_type != SCM_RIGHTS ||
        cmptr->cmsg_len < CMSG_LEN(sizeof(int))) {
        return -2;
    }

    int received_fd = -1;
    memcpy(&received_fd, CMSG_DATA(cmptr), sizeof(int));
    if (received_fd < 0) {
        return -2;
    }

    return received_fd;
}
