/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */
#include "hal_private.h"
#include <sys/epoll.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_POLL_EVENTS 256

typedef struct {
    int epfd;
    pthread_mutex_t lock;
    void **userdata_map;
    int map_capacity;
} os_poll_instance_t;

void *os_poll_create(void) {
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) return NULL;

    os_poll_instance_t *inst = (os_poll_instance_t *)calloc(1, sizeof(os_poll_instance_t));
    if (!inst) {
        close(epfd);
        return NULL;
    }

    inst->epfd = epfd;
    pthread_mutex_init(&inst->lock, NULL);
    inst->map_capacity = 2048;
    inst->userdata_map = (void **)calloc((size_t)inst->map_capacity, sizeof(void *));
    if (!inst->userdata_map) {
        close(epfd);
        pthread_mutex_destroy(&inst->lock);
        free(inst);
        return NULL;
    }

    return (void *)inst;
}

int os_poll_add(void *p, int fd, int events, void *user_data) {
    if (!p || fd < 0) return -1;
    os_poll_instance_t *inst = (os_poll_instance_t *)p;

    pthread_mutex_lock(&inst->lock);
    if (fd >= inst->map_capacity) {
        int new_cap = inst->map_capacity * 2;
        while (fd >= new_cap) new_cap *= 2;
        void **new_map = (void **)realloc(inst->userdata_map, (size_t)new_cap * sizeof(void *));
        if (!new_map) {
            pthread_mutex_unlock(&inst->lock);
            return -1;
        }
        memset(new_map + inst->map_capacity, 0, (size_t)(new_cap - inst->map_capacity) * sizeof(void *));
        inst->userdata_map = new_map;
        inst->map_capacity = new_cap;
    }
    inst->userdata_map[fd] = user_data;
    pthread_mutex_unlock(&inst->lock);

    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    if (events & AR_EVENT_READ) ev.events |= EPOLLIN;
    if (events & AR_EVENT_WRITE) ev.events |= EPOLLOUT;
    ev.data.fd = fd;

    return epoll_ctl(inst->epfd, EPOLL_CTL_ADD, fd, &ev);
}

int os_poll_mod(void *p, int fd, int events) {
    if (!p || fd < 0) return -1;
    os_poll_instance_t *inst = (os_poll_instance_t *)p;

    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    if (events & AR_EVENT_READ) ev.events |= EPOLLIN;
    if (events & AR_EVENT_WRITE) ev.events |= EPOLLOUT;
    ev.data.fd = fd;

    return epoll_ctl(inst->epfd, EPOLL_CTL_MOD, fd, &ev);
}

void os_poll_remove(void *p, int fd) {
    if (!p || fd < 0) return;
    os_poll_instance_t *inst = (os_poll_instance_t *)p;

    epoll_ctl(inst->epfd, EPOLL_CTL_DEL, fd, NULL);

    pthread_mutex_lock(&inst->lock);
    if (fd < inst->map_capacity) {
        inst->userdata_map[fd] = NULL;
    }
    pthread_mutex_unlock(&inst->lock);
}

int os_poll_wait(void *p, ArPollEvent *events, int max_events, int timeout_ms) {
    if (!p || !events || max_events <= 0) return -1;
    os_poll_instance_t *inst = (os_poll_instance_t *)p;

    struct epoll_event ev[MAX_POLL_EVENTS];
    int n = max_events > MAX_POLL_EVENTS ? MAX_POLL_EVENTS : max_events;
    int nfds = epoll_wait(inst->epfd, ev, n, timeout_ms);
    if (nfds <= 0) return nfds;

    pthread_mutex_lock(&inst->lock);
    for (int i = 0; i < nfds; i++) {
        int fd = ev[i].data.fd;
        events[i].fd = fd;
        events[i].events = 0;
        if (ev[i].events & (EPOLLIN | EPOLLHUP | EPOLLERR)) events[i].events |= AR_EVENT_READ;
        if (ev[i].events & EPOLLOUT) events[i].events |= AR_EVENT_WRITE;
        events[i].user_data = (fd >= 0 && fd < inst->map_capacity) ? inst->userdata_map[fd] : NULL;
    }
    pthread_mutex_unlock(&inst->lock);

    return nfds;
}

void os_poll_destroy(void *p) {
    if (!p) return;
    os_poll_instance_t *inst = (os_poll_instance_t *)p;
    close(inst->epfd);
    pthread_mutex_destroy(&inst->lock);
    free(inst->userdata_map);
    free(inst);
}
