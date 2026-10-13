/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/ipc/guest_channel.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#pragma pack(push, 1)
typedef struct ar_guest_wire_header {
    uint32_t magic;
    uint16_t version;
    uint16_t message_type;
    uint32_t flags;
    uint64_t request_id;
    uint32_t payload_length;
} ar_guest_wire_header_t;
#pragma pack(pop)

_Static_assert(sizeof(ar_guest_wire_header_t) == AR_GUEST_WIRE_HEADER_SIZE,
               "ar_guest_wire_header_t must match AR_GUEST_WIRE_HEADER_SIZE");

struct ar_guest_channel {
    int fd;
    ar_guest_peer_identity_t peer;
    ar_guest_channel_config_t config;
};

static void drain_datagram(int fd) {
    char junk[1024];
    struct msghdr msg;
    struct iovec iov;

    memset(&msg, 0, sizeof(msg));
    iov.iov_base = junk;
    iov.iov_len = sizeof(junk);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1U;

    (void)recvmsg(fd, &msg, MSG_DONTWAIT);
}

int ar_guest_channel_socketpair(int out_fds[2]) {
    if (!out_fds) {
        return AR_GUEST_ERR_INVALID;
    }
    if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, out_fds) != 0) {
        return AR_GUEST_ERR_SYSTEM;
    }
    return AR_GUEST_OK;
}

int ar_guest_channel_attach(
    int fd,
    const ar_guest_channel_config_t *config,
    ar_guest_channel_t **out_channel) {
    struct ucred ucred;
    socklen_t ucred_len = (socklen_t)sizeof(ucred);
    int one = 1;
    ar_guest_channel_t *ch;
    int flags;

    if (fd < 0 || !out_channel) {
        return AR_GUEST_ERR_INVALID;
    }
    if (config != NULL) {
        if (config->max_payload_size > AR_GUEST_HARD_MAX_PAYLOAD_SIZE) {
            return AR_GUEST_ERR_LIMIT;
        }
    }

    memset(&ucred, 0, sizeof(ucred));
    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &ucred, &ucred_len) != 0) {
        return AR_GUEST_ERR_AUTH;
    }

    if (config != NULL && config->peer_policy.match_flags != 0U) {
        if ((config->peer_policy.match_flags & AR_GUEST_MATCH_PID) != 0U &&
            (uint32_t)ucred.pid != config->peer_policy.expected.pid) {
            return AR_GUEST_ERR_AUTH;
        }
        if ((config->peer_policy.match_flags & AR_GUEST_MATCH_UID) != 0U &&
            (uint32_t)ucred.uid != config->peer_policy.expected.uid) {
            return AR_GUEST_ERR_AUTH;
        }
        if ((config->peer_policy.match_flags & AR_GUEST_MATCH_GID) != 0U &&
            (uint32_t)ucred.gid != config->peer_policy.expected.gid) {
            return AR_GUEST_ERR_AUTH;
        }
    }

    if (setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &one, (socklen_t)sizeof(one)) != 0) {
        return AR_GUEST_ERR_SYSTEM;
    }

    (void)fcntl(fd, F_SETFD, FD_CLOEXEC);
    flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    ch = (ar_guest_channel_t *)calloc(1U, sizeof(ar_guest_channel_t));
    if (!ch) {
        return AR_GUEST_ERR_NOMEM;
    }

    ch->fd = fd;
    ch->peer.pid = (uint32_t)ucred.pid;
    ch->peer.uid = (uint32_t)ucred.uid;
    ch->peer.gid = (uint32_t)ucred.gid;

    if (config != NULL) {
        ch->config = *config;
    } else {
        ch->config.io_timeout_ms = AR_GUEST_DEFAULT_IO_TIMEOUT_MS;
        ch->config.max_payload_size = AR_GUEST_HARD_MAX_PAYLOAD_SIZE;
        ch->config.peer_policy.match_flags = 0U;
    }

    if (ch->config.max_payload_size == 0U) {
        ch->config.max_payload_size = AR_GUEST_HARD_MAX_PAYLOAD_SIZE;
    }

    *out_channel = ch;
    return AR_GUEST_OK;
}

void ar_guest_channel_close(ar_guest_channel_t *channel) {
    if (!channel) {
        return;
    }
    if (channel->fd >= 0) {
        (void)close(channel->fd);
        channel->fd = -1;
    }
    free(channel);
}

int ar_guest_channel_fd(const ar_guest_channel_t *channel) {
    if (!channel) {
        return -1;
    }
    return channel->fd;
}

int ar_guest_channel_peer(
    const ar_guest_channel_t *channel,
    ar_guest_peer_identity_t *out_peer) {
    if (!channel || !out_peer) {
        return AR_GUEST_ERR_INVALID;
    }
    *out_peer = channel->peer;
    return AR_GUEST_OK;
}

int ar_guest_channel_send(
    ar_guest_channel_t *channel,
    uint16_t message_type,
    uint32_t flags,
    uint64_t request_id,
    const void *payload,
    uint32_t payload_length) {
    ar_guest_wire_header_t wire_hdr;
    struct iovec iov[2];
    struct msghdr msg;
    char cbuf[CMSG_SPACE(sizeof(struct ucred))];
    struct cmsghdr *cmsg;
    struct ucred *cred;
    size_t iovcnt = 1U;
    size_t total_expected;
    ssize_t sent;

    if (!channel || channel->fd < 0) {
        return AR_GUEST_ERR_INVALID;
    }
    if (payload_length > 0U && !payload) {
        return AR_GUEST_ERR_INVALID;
    }
    if ((flags & ~AR_GUEST_FLAGS_ALLOWED) != 0U) {
        return AR_GUEST_ERR_INVALID;
    }
    if (payload_length > channel->config.max_payload_size ||
        payload_length > AR_GUEST_HARD_MAX_PAYLOAD_SIZE) {
        return AR_GUEST_ERR_LIMIT;
    }

    wire_hdr.magic = AR_GUEST_WIRE_MAGIC;
    wire_hdr.version = AR_GUEST_WIRE_VERSION;
    wire_hdr.message_type = message_type;
    wire_hdr.flags = flags;
    wire_hdr.request_id = request_id;
    wire_hdr.payload_length = payload_length;

    iov[0].iov_base = &wire_hdr;
    iov[0].iov_len = sizeof(wire_hdr);
    if (payload_length > 0U) {
        iov[1].iov_base = (void *)(uintptr_t)payload;
        iov[1].iov_len = (size_t)payload_length;
        iovcnt = 2U;
    }

    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = iov;
    msg.msg_iovlen = iovcnt;

    memset(cbuf, 0, sizeof(cbuf));
    msg.msg_control = cbuf;
    msg.msg_controllen = sizeof(cbuf);
    cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg != NULL) {
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_CREDENTIALS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(struct ucred));
        cred = (struct ucred *)CMSG_DATA(cmsg);
        cred->pid = getpid();
        cred->uid = geteuid();
        cred->gid = getegid();
        msg.msg_controllen = cmsg->cmsg_len;
    }

    total_expected = sizeof(wire_hdr) + (size_t)payload_length;

    if (channel->config.io_timeout_ms > 0) {
        struct pollfd pfd;
        int prc;

        pfd.fd = channel->fd;
        pfd.events = POLLOUT;
        pfd.revents = 0;

        do {
            prc = poll(&pfd, 1, channel->config.io_timeout_ms);
        } while (prc < 0 && errno == EINTR);

        if (prc == 0) {
            return AR_GUEST_ERR_TIMEOUT;
        }
        if (prc < 0) {
            return AR_GUEST_ERR_SYSTEM;
        }
        if ((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            return AR_GUEST_ERR_CLOSED;
        }
    }

    do {
        sent = sendmsg(channel->fd, &msg, MSG_DONTWAIT | MSG_NOSIGNAL);
    } while (sent < 0 && errno == EINTR);

    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return AR_GUEST_ERR_WOULD_BLOCK;
        }
        if (errno == EPIPE || errno == ECONNRESET) {
            return AR_GUEST_ERR_CLOSED;
        }
        return AR_GUEST_ERR_SYSTEM;
    }

    if ((size_t)sent != total_expected) {
        return AR_GUEST_ERR_PROTOCOL;
    }

    return AR_GUEST_OK;
}

int ar_guest_channel_receive(
    ar_guest_channel_t *channel,
    ar_guest_frame_header_t *out_header,
    void *out_payload,
    uint32_t payload_capacity) {
    ar_guest_wire_header_t wire_hdr;
    char cbuf[CMSG_SPACE(sizeof(struct ucred)) + 128];
    struct iovec peek_iov;
    struct msghdr peek_msg;
    struct cmsghdr *cmsg;
    struct ucred *cred = NULL;
    ssize_t peeked;

    if (!channel || channel->fd < 0 || !out_header) {
        return AR_GUEST_ERR_INVALID;
    }
    if (payload_capacity > 0U && !out_payload) {
        return AR_GUEST_ERR_INVALID;
    }

    if (channel->config.io_timeout_ms != 0) {
        struct pollfd pfd;
        int prc;

        pfd.fd = channel->fd;
        pfd.events = POLLIN;
        pfd.revents = 0;

        do {
            prc = poll(&pfd, 1, channel->config.io_timeout_ms);
        } while (prc < 0 && errno == EINTR);

        if (prc == 0) {
            return AR_GUEST_ERR_TIMEOUT;
        }
        if (prc < 0) {
            return AR_GUEST_ERR_SYSTEM;
        }
        if ((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0 &&
            (pfd.revents & POLLIN) == 0) {
            return AR_GUEST_ERR_CLOSED;
        }
    }

    memset(&wire_hdr, 0, sizeof(wire_hdr));
    peek_iov.iov_base = &wire_hdr;
    peek_iov.iov_len = sizeof(wire_hdr);

    memset(&peek_msg, 0, sizeof(peek_msg));
    peek_msg.msg_iov = &peek_iov;
    peek_msg.msg_iovlen = 1U;
    peek_msg.msg_control = cbuf;
    peek_msg.msg_controllen = sizeof(cbuf);

    do {
        peeked = recvmsg(channel->fd, &peek_msg, MSG_PEEK | MSG_TRUNC | MSG_DONTWAIT);
    } while (peeked < 0 && errno == EINTR);

    if (peeked == 0) {
        return AR_GUEST_ERR_CLOSED;
    }
    if (peeked < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return AR_GUEST_ERR_WOULD_BLOCK;
        }
        if (errno == ECONNRESET || errno == EPIPE) {
            return AR_GUEST_ERR_CLOSED;
        }
        return AR_GUEST_ERR_SYSTEM;
    }

    if (peeked < (ssize_t)sizeof(wire_hdr)) {
        drain_datagram(channel->fd);
        return AR_GUEST_ERR_PROTOCOL;
    }

    for (cmsg = CMSG_FIRSTHDR(&peek_msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&peek_msg, cmsg)) {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_CREDENTIALS) {
            cred = (struct ucred *)CMSG_DATA(cmsg);
            break;
        }
    }
    if (!cred) {
        drain_datagram(channel->fd);
        return AR_GUEST_ERR_AUTH;
    }

    if ((channel->config.peer_policy.match_flags & AR_GUEST_MATCH_PID) != 0U &&
        (uint32_t)cred->pid != channel->config.peer_policy.expected.pid) {
        drain_datagram(channel->fd);
        return AR_GUEST_ERR_AUTH;
    }
    if ((channel->config.peer_policy.match_flags & AR_GUEST_MATCH_UID) != 0U &&
        (uint32_t)cred->uid != channel->config.peer_policy.expected.uid) {
        drain_datagram(channel->fd);
        return AR_GUEST_ERR_AUTH;
    }
    if ((channel->config.peer_policy.match_flags & AR_GUEST_MATCH_GID) != 0U &&
        (uint32_t)cred->gid != channel->config.peer_policy.expected.gid) {
        drain_datagram(channel->fd);
        return AR_GUEST_ERR_AUTH;
    }
    if (channel->config.peer_policy.match_flags == 0U) {
        if ((uint32_t)cred->uid != channel->peer.uid ||
            (uint32_t)cred->pid != channel->peer.pid) {
            drain_datagram(channel->fd);
            return AR_GUEST_ERR_AUTH;
        }
    }

    if (wire_hdr.magic != AR_GUEST_WIRE_MAGIC || wire_hdr.version != AR_GUEST_WIRE_VERSION) {
        drain_datagram(channel->fd);
        return AR_GUEST_ERR_PROTOCOL;
    }
    if ((wire_hdr.flags & ~AR_GUEST_FLAGS_ALLOWED) != 0U) {
        drain_datagram(channel->fd);
        return AR_GUEST_ERR_PROTOCOL;
    }
    if (wire_hdr.payload_length > channel->config.max_payload_size ||
        wire_hdr.payload_length > AR_GUEST_HARD_MAX_PAYLOAD_SIZE) {
        drain_datagram(channel->fd);
        return AR_GUEST_ERR_LIMIT;
    }
    if ((size_t)peeked != sizeof(wire_hdr) + (size_t)wire_hdr.payload_length) {
        drain_datagram(channel->fd);
        return AR_GUEST_ERR_PROTOCOL;
    }

    if (payload_capacity < wire_hdr.payload_length) {
        out_header->message_type = wire_hdr.message_type;
        out_header->flags = wire_hdr.flags;
        out_header->request_id = wire_hdr.request_id;
        out_header->payload_length = wire_hdr.payload_length;
        return AR_GUEST_ERR_BUFFER_TOO_SMALL;
    }

    {
        struct iovec full_iov[2];
        ar_guest_wire_header_t full_hdr;
        struct msghdr full_msg;
        size_t full_cnt = 1U;
        ssize_t recvd;

        memset(&full_hdr, 0, sizeof(full_hdr));
        full_iov[0].iov_base = &full_hdr;
        full_iov[0].iov_len = sizeof(full_hdr);
        if (wire_hdr.payload_length > 0U) {
            full_iov[1].iov_base = out_payload;
            full_iov[1].iov_len = (size_t)wire_hdr.payload_length;
            full_cnt = 2U;
        }

        memset(&full_msg, 0, sizeof(full_msg));
        full_msg.msg_iov = full_iov;
        full_msg.msg_iovlen = full_cnt;

        do {
            recvd = recvmsg(channel->fd, &full_msg, MSG_DONTWAIT);
        } while (recvd < 0 && errno == EINTR);

        if (recvd < 0) {
            return AR_GUEST_ERR_SYSTEM;
        }
        if ((size_t)recvd != sizeof(full_hdr) + (size_t)wire_hdr.payload_length) {
            return AR_GUEST_ERR_PROTOCOL;
        }

        out_header->message_type = full_hdr.message_type;
        out_header->flags = full_hdr.flags;
        out_header->request_id = full_hdr.request_id;
        out_header->payload_length = full_hdr.payload_length;
    }

    return AR_GUEST_OK;
}
