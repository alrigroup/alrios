/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include <sys/epoll.h>
#include <stdlib.h>
#include <string.h>

#define POLL_FD_LIMIT 2048
#define MAX_POLL_EVENTS 256

static void *fd_userdata[POLL_FD_LIMIT];

static void *os_poll_create(void) {
    int epfd = epoll_create1(0);
    if (epfd < 0) return NULL;
    memset(fd_userdata, 0, sizeof(fd_userdata));
    return (void *)(intptr_t)epfd;
}

static int os_poll_add(void *p, int fd, int events, void *user_data) {
    if (fd < 0 || fd >= POLL_FD_LIMIT) return -1;
    struct epoll_event ev;
    ev.events = 0;
    if (events & AR_EVENT_READ) ev.events |= EPOLLIN;
    if (events & AR_EVENT_WRITE) ev.events |= EPOLLOUT;
    ev.data.fd = fd;
    fd_userdata[fd] = user_data;
    return epoll_ctl((int)(intptr_t)p, EPOLL_CTL_ADD, fd, &ev);
}

static int os_poll_mod(void *p, int fd, int events) {
    if (fd < 0 || fd >= POLL_FD_LIMIT) return -1;
    struct epoll_event ev;
    ev.events = 0;
    if (events & AR_EVENT_READ) ev.events |= EPOLLIN;
    if (events & AR_EVENT_WRITE) ev.events |= EPOLLOUT;
    ev.data.fd = fd;
    return epoll_ctl((int)(intptr_t)p, EPOLL_CTL_MOD, fd, &ev);
}

static void os_poll_remove(void *p, int fd) {
    if (fd >= 0 && fd < POLL_FD_LIMIT) {
        fd_userdata[fd] = NULL;
        epoll_ctl((int)(intptr_t)p, EPOLL_CTL_DEL, fd, NULL);
    }
}

static int os_poll_wait(void *p, ArPollEvent *events, int max_events, int timeout_ms) {
    struct epoll_event ev[MAX_POLL_EVENTS];
    int n = max_events > MAX_POLL_EVENTS ? MAX_POLL_EVENTS : max_events;
    int nfds = epoll_wait((int)(intptr_t)p, ev, n, timeout_ms);
    if (nfds <= 0) return nfds;

    for (int i = 0; i < nfds; i++) {
        int fd = ev[i].data.fd;
        events[i].fd = fd;
        events[i].events = 0;
        if (ev[i].events & (EPOLLIN | EPOLLHUP | EPOLLERR)) events[i].events |= AR_EVENT_READ;
        if (ev[i].events & EPOLLOUT) events[i].events |= AR_EVENT_WRITE;
        events[i].user_data = (fd >= 0 && fd < POLL_FD_LIMIT) ? fd_userdata[fd] : NULL;
    }
    return nfds;
}

static void os_poll_destroy(void *p) {
    if (p) close((int)(intptr_t)p);
}
