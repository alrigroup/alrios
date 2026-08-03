/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include <winsock2.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    fd_set read_fds;
    fd_set write_fds;
    int max_fd;
    void *user_data[FD_SETSIZE];
    int fd_map[FD_SETSIZE];
    int fd_count;
} WinPoll;

static void *os_poll_create(void) {
    WinPoll *wp = (WinPoll *)malloc(sizeof(WinPoll));
    if (!wp) return NULL;
    memset(wp, 0, sizeof(WinPoll));
    for (int i = 0; i < FD_SETSIZE; i++) wp->fd_map[i] = -1;
    return (void *)wp;
}

static int poll_find(WinPoll *wp, int fd) {
    for (int i = 0; i < wp->fd_count; i++) {
        if (wp->fd_map[i] == fd) return i;
    }
    return -1;
}

static int os_poll_add(void *p, int fd, int events, void *user_data) {
    WinPoll *wp = (WinPoll *)p;
    if (wp->fd_count >= FD_SETSIZE) return -1;

    int idx = wp->fd_count;
    wp->fd_map[idx] = fd;
    wp->user_data[idx] = user_data;
    if (fd > wp->max_fd) wp->max_fd = fd;
    if (events & AR_EVENT_READ) FD_SET(fd, &wp->read_fds);
    if (events & AR_EVENT_WRITE) FD_SET(fd, &wp->write_fds);
    wp->fd_count++;
    return 0;
}

static int os_poll_mod(void *p, int fd, int events) {
    WinPoll *wp = (WinPoll *)p;
    int idx = poll_find(wp, fd);
    if (idx < 0) return -1;

    FD_CLR(fd, &wp->read_fds);
    FD_CLR(fd, &wp->write_fds);
    if (events & AR_EVENT_READ) FD_SET(fd, &wp->read_fds);
    if (events & AR_EVENT_WRITE) FD_SET(fd, &wp->write_fds);
    return 0;
}

static void os_poll_remove(void *p, int fd) {
    WinPoll *wp = (WinPoll *)p;
    int idx = poll_find(wp, fd);
    if (idx < 0) return;

    FD_CLR(fd, &wp->read_fds);
    FD_CLR(fd, &wp->write_fds);

    int last = wp->fd_count - 1;
    if (idx < last) {
        wp->fd_map[idx] = wp->fd_map[last];
        wp->user_data[idx] = wp->user_data[last];
    }
    wp->fd_count--;

    if (fd == wp->max_fd) {
        wp->max_fd = 0;
        for (int i = 0; i < wp->fd_count; i++) {
            if (wp->fd_map[i] > wp->max_fd) wp->max_fd = wp->fd_map[i];
        }
    }
}

static int os_poll_wait(void *p, ArPollEvent *events, int max_events, int timeout_ms) {
    WinPoll *wp = (WinPoll *)p;
    fd_set rfds = wp->read_fds;
    fd_set wfds = wp->write_fds;
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int nfds = select(wp->max_fd + 1, &rfds, &wfds, NULL, &tv);
    if (nfds <= 0) return nfds;

    int count = 0;
    for (int i = 0; i < wp->fd_count && count < max_events; i++) {
        int fd = wp->fd_map[i];
        int revents = 0;
        if (FD_ISSET(fd, &rfds)) revents |= AR_EVENT_READ;
        if (FD_ISSET(fd, &wfds)) revents |= AR_EVENT_WRITE;
        if (revents) {
            events[count].fd = fd;
            events[count].events = revents;
            events[count].user_data = wp->user_data[i];
            count++;
        }
    }
    return count;
}

static void os_poll_destroy(void *p) {
    free(p);
}
