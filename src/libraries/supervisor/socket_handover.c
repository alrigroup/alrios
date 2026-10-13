/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "alrios/deploy/socket_handover.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define ALRIOS_HANDOVER_MAGIC    0x41525348U /* 'ARSH' */
#define ALRIOS_HANDOVER_VERSION  1U

typedef struct alrios_handover_frame {
    uint32_t magic;
    uint32_t version;
    uint64_t generation;
} alrios_handover_frame_t;

static uint64_t handover_monotonic_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0U;
    }
    return ((uint64_t)ts.tv_sec * 1000ULL) + ((uint64_t)ts.tv_nsec / 1000000ULL);
}

static int verify_peer_credentials(int control_fd,
                                  const alrios_handover_peer_policy_t *policy,
                                  alrios_handover_peer_credentials_t *out_creds) {
    if (out_creds != NULL) {
        memset(out_creds, 0, sizeof(*out_creds));
    }

#if defined(SO_PEERCRED)
    struct ucred ucred;
    socklen_t len = (socklen_t)sizeof(ucred);
    memset(&ucred, 0, sizeof(ucred));

    if (getsockopt(control_fd, SOL_SOCKET, SO_PEERCRED, &ucred, &len) != 0) {
        if (policy != NULL && policy->flags != 0U) {
            return -1;
        }
        return 0;
    }

    if (out_creds != NULL) {
        out_creds->pid = ucred.pid;
        out_creds->uid = ucred.uid;
        out_creds->gid = ucred.gid;
        out_creds->available = 1;
    }

    if (policy != NULL && policy->flags != 0U) {
        if ((policy->flags & ALRIOS_HANDOVER_PEER_SAME_EUID) != 0U &&
            ucred.uid != geteuid()) {
            return -1;
        }
        if ((policy->flags & ALRIOS_HANDOVER_PEER_UID) != 0U &&
            ucred.uid != policy->uid) {
            return -1;
        }
        if ((policy->flags & ALRIOS_HANDOVER_PEER_GID) != 0U &&
            ucred.gid != policy->gid) {
            return -1;
        }
        if ((policy->flags & ALRIOS_HANDOVER_PEER_PID) != 0U &&
            ucred.pid != policy->pid) {
            return -1;
        }
    }
    return 0;
#else
    if (policy != NULL && policy->flags != 0U) {
        return -1;
    }
    return 0;
#endif
}

int alrios_socket_handover_send(int control_fd,
                                int fd_to_send,
                                uint64_t generation,
                                const alrios_handover_peer_policy_t *peer_policy,
                                int timeout_ms) {
    if (control_fd < 0 || fd_to_send < 0 || timeout_ms <= 0) {
        return -1;
    }

    if (verify_peer_credentials(control_fd, peer_policy, NULL) != 0) {
        return -1;
    }

    uint64_t deadline = handover_monotonic_ms() + (uint64_t)timeout_ms;
    struct pollfd pfd;
    pfd.fd = control_fd;
    pfd.events = POLLOUT;
    pfd.revents = 0;

    for (;;) {
        uint64_t now = handover_monotonic_ms();
        if (now >= deadline) {
            return -1;
        }
        int remaining = (int)(deadline - now);
        int pr = poll(&pfd, 1, remaining);
        if (pr < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (pr == 0) {
            return -1;
        }
        if ((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            return -1;
        }
        if ((pfd.revents & POLLOUT) != 0) {
            break;
        }
    }

    alrios_handover_frame_t frame;
    frame.magic = ALRIOS_HANDOVER_MAGIC;
    frame.version = ALRIOS_HANDOVER_VERSION;
    frame.generation = generation;

    struct iovec iov;
    iov.iov_base = &frame;
    iov.iov_len = sizeof(frame);

    union {
        struct cmsghdr cm;
        char control[CMSG_SPACE(sizeof(int))];
    } ctl;
    memset(&ctl, 0, sizeof(ctl));

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = ctl.control;
    msg.msg_controllen = sizeof(ctl.control);

    struct cmsghdr *cmptr = CMSG_FIRSTHDR(&msg);
    if (cmptr == NULL) {
        return -1;
    }
    cmptr->cmsg_len = CMSG_LEN(sizeof(int));
    cmptr->cmsg_level = SOL_SOCKET;
    cmptr->cmsg_type = SCM_RIGHTS;
    memcpy(CMSG_DATA(cmptr), &fd_to_send, sizeof(int));

    for (;;) {
        ssize_t sent = sendmsg(control_fd, &msg, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) {
                uint64_t now = handover_monotonic_ms();
                if (now < deadline) {
                    continue;
                }
            }
            return -1;
        }
        if ((size_t)sent != sizeof(frame)) {
            return -1;
        }
        break;
    }

    return 0;
}

int alrios_socket_handover_receive(int control_fd,
                                   int *out_received_fd,
                                   uint64_t *out_generation,
                                   alrios_handover_peer_credentials_t *out_peer,
                                   const alrios_handover_peer_policy_t *peer_policy,
                                   int timeout_ms) {
    if (control_fd < 0 || out_received_fd == NULL || out_generation == NULL || timeout_ms <= 0) {
        return -1;
    }
    *out_received_fd = -1;
    *out_generation = 0U;

    if (verify_peer_credentials(control_fd, peer_policy, out_peer) != 0) {
        return -1;
    }

    uint64_t deadline = handover_monotonic_ms() + (uint64_t)timeout_ms;
    struct pollfd pfd;
    pfd.fd = control_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    for (;;) {
        uint64_t now = handover_monotonic_ms();
        if (now >= deadline) {
            return -1;
        }
        int remaining = (int)(deadline - now);
        int pr = poll(&pfd, 1, remaining);
        if (pr < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (pr == 0) {
            return -1;
        }
        if ((pfd.revents & (POLLERR | POLLNVAL)) != 0) {
            return -1;
        }
        if ((pfd.revents & (POLLIN | POLLHUP)) != 0) {
            break;
        }
    }

    alrios_handover_frame_t frame;
    memset(&frame, 0, sizeof(frame));

    struct iovec iov;
    iov.iov_base = &frame;
    iov.iov_len = sizeof(frame);

    union {
        struct cmsghdr cm;
        char control[CMSG_SPACE(sizeof(int))];
    } ctl;
    memset(&ctl, 0, sizeof(ctl));

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = ctl.control;
    msg.msg_controllen = sizeof(ctl.control);

    ssize_t recvd = -1;
    for (;;) {
        recvd = recvmsg(control_fd, &msg, 0);
        if (recvd < 0) {
            if (errno == EINTR) {
                uint64_t now = handover_monotonic_ms();
                if (now < deadline) {
                    continue;
                }
            }
            return -1;
        }
        break;
    }

    if (recvd != (ssize_t)sizeof(frame)) {
        return -1;
    }
    if ((msg.msg_flags & (MSG_CTRUNC | MSG_TRUNC)) != 0) {
        return -1;
    }
    if (frame.magic != ALRIOS_HANDOVER_MAGIC || frame.version != ALRIOS_HANDOVER_VERSION) {
        return -1;
    }

    struct cmsghdr *cmptr = CMSG_FIRSTHDR(&msg);
    if (cmptr == NULL ||
        cmptr->cmsg_level != SOL_SOCKET ||
        cmptr->cmsg_type != SCM_RIGHTS ||
        cmptr->cmsg_len < CMSG_LEN(sizeof(int))) {
        return -1;
    }

    int received_fd = -1;
    memcpy(&received_fd, CMSG_DATA(cmptr), sizeof(int));
    if (received_fd < 0) {
        return -1;
    }

    if (fcntl(received_fd, F_SETFD, FD_CLOEXEC) < 0) {
        (void)close(received_fd);
        return -1;
    }
    int fl = fcntl(received_fd, F_GETFL, 0);
    if (fl < 0 || fcntl(received_fd, F_SETFL, fl | O_NONBLOCK) < 0) {
        (void)close(received_fd);
        return -1;
    }

    *out_received_fd = received_fd;
    *out_generation = frame.generation;
    return 0;
}

int alrios_listener_route_init(alrios_listener_route_t *route,
                               int initial_fd,
                               uint64_t generation) {
    if (route == NULL || initial_fd < 0) {
        return -1;
    }
    atomic_flag_clear(&route->lock);
    while (atomic_flag_test_and_set(&route->lock)) {
    }
    route->active_fd = initial_fd;
    route->generation = generation;
    route->initialized = 1;
    atomic_flag_clear(&route->lock);
    return 0;
}

int alrios_listener_route_publish(alrios_listener_route_t *route,
                                  int new_fd,
                                  uint64_t generation,
                                  int *out_previous_fd) {
    if (route == NULL || new_fd < 0 || out_previous_fd == NULL) {
        return -1;
    }
    while (atomic_flag_test_and_set(&route->lock)) {
    }
    if (!route->initialized) {
        atomic_flag_clear(&route->lock);
        return -1;
    }
    *out_previous_fd = route->active_fd;
    route->active_fd = new_fd;
    route->generation = generation;
    atomic_flag_clear(&route->lock);
    return 0;
}

int alrios_listener_route_acquire(alrios_listener_route_t *route,
                                  int *out_fd,
                                  uint64_t *out_generation) {
    if (route == NULL || out_fd == NULL) {
        return -1;
    }
    while (atomic_flag_test_and_set(&route->lock)) {
    }
    if (!route->initialized || route->active_fd < 0) {
        atomic_flag_clear(&route->lock);
        return -1;
    }
    int dup_fd = fcntl(route->active_fd, F_DUPFD_CLOEXEC, 0);
    if (dup_fd < 0) {
        dup_fd = dup(route->active_fd);
        if (dup_fd >= 0) {
            (void)fcntl(dup_fd, F_SETFD, FD_CLOEXEC);
        }
    }
    if (out_generation != NULL) {
        *out_generation = route->generation;
    }
    atomic_flag_clear(&route->lock);

    if (dup_fd < 0) {
        return -1;
    }
    *out_fd = dup_fd;
    return 0;
}

int alrios_listener_route_take(alrios_listener_route_t *route, int *out_fd) {
    if (route == NULL || out_fd == NULL) {
        return -1;
    }
    while (atomic_flag_test_and_set(&route->lock)) {
    }
    if (!route->initialized || route->active_fd < 0) {
        atomic_flag_clear(&route->lock);
        return -1;
    }
    *out_fd = route->active_fd;
    route->active_fd = -1;
    route->initialized = 0;
    atomic_flag_clear(&route->lock);
    return 0;
}
