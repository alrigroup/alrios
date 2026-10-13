/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */

#include "alrios/ipc_channel.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

int alrios_ipc_set_nonblocking(int fd) {
    if (fd < 0) {
        return -1;
    }

    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }

    if ((flags & O_NONBLOCK) != 0) {
        return 0;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return -1;
    }

    return 0;
}

int alrios_ipc_create_epoll_channel(int socket_fd) {
    if (socket_fd < 0) {
        return -1;
    }

    if (alrios_ipc_set_nonblocking(socket_fd) != 0) {
        return -4;
    }

    const int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) {
        return -2;
    }

    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLHUP;
    ev.data.fd = socket_fd;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, socket_fd, &ev) < 0) {
        const int saved_errno = errno;
        (void)close(epfd);
        errno = saved_errno;
        return -3;
    }

    return epfd;
}

int alrios_ipc_epoll_add(int epfd, int socket_fd, uint32_t events) {
    if (epfd < 0 || socket_fd < 0) {
        return -1;
    }

    if (alrios_ipc_set_nonblocking(socket_fd) != 0) {
        return -4;
    }

    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = events;
    ev.data.fd = socket_fd;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, socket_fd, &ev) < 0) {
        return -3;
    }

    return 0;
}

int alrios_ipc_epoll_mod(int epfd, int socket_fd, uint32_t events) {
    if (epfd < 0 || socket_fd < 0) {
        return -1;
    }

    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = events;
    ev.data.fd = socket_fd;

    if (epoll_ctl(epfd, EPOLL_CTL_MOD, socket_fd, &ev) < 0) {
        return -3;
    }

    return 0;
}

int alrios_ipc_epoll_del(int epfd, int socket_fd) {
    if (epfd < 0 || socket_fd < 0) {
        return -1;
    }

    if (epoll_ctl(epfd, EPOLL_CTL_DEL, socket_fd, NULL) < 0) {
        return -3;
    }

    return 0;
}

int alrios_ipc_epoll_wait(int epfd, int *ready_fd, uint32_t *events, int timeout_ms) {
    if (epfd < 0) {
        return -1;
    }

    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    int nfds;

    do {
        nfds = epoll_wait(epfd, &ev, 1, timeout_ms);
    } while (nfds < 0 && errno == EINTR);

    if (nfds < 0) {
        return -1;
    }

    if (nfds == 0) {
        return 0;
    }

    if (ready_fd != NULL) {
        *ready_fd = ev.data.fd;
    }

    if (events != NULL) {
        *events = ev.events;
    }

    return 1;
}
