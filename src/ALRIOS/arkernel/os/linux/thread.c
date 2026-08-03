/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include <pthread.h>
#include <stdlib.h>
#include <errno.h>

static int os_thread_detach(void *thread) {
    return pthread_detach(*(pthread_t *)thread);
}

static void *os_thread_create(void *(*fn)(void *), void *arg) {
    pthread_t *thread = malloc(sizeof(pthread_t));
    if (!thread) return NULL;

    if (pthread_create(thread, NULL, fn, arg) != 0) {
        free(thread);
        return NULL;
    }
    return thread;
}

static int os_thread_join(void *thread) {
    int ret = pthread_join(*(pthread_t *)thread, NULL);
    free(thread);
    return ret;
}

static void *os_mutex_create(void) {
    pthread_mutex_t *m = malloc(sizeof(pthread_mutex_t));
    if (!m) return NULL;

    if (pthread_mutex_init(m, NULL) != 0) {
        free(m);
        return NULL;
    }
    return m;
}

static int os_mutex_lock(void *mutex) {
    return pthread_mutex_lock((pthread_mutex_t *)mutex);
}

static int os_mutex_unlock(void *mutex) {
    return pthread_mutex_unlock((pthread_mutex_t *)mutex);
}

static void os_mutex_destroy(void *mutex) {
    pthread_mutex_destroy((pthread_mutex_t *)mutex);
    free(mutex);
}

static void *os_cond_create(void) {
    pthread_cond_t *c = malloc(sizeof(pthread_cond_t));
    if (!c) return NULL;

    if (pthread_cond_init(c, NULL) != 0) {
        free(c);
        return NULL;
    }
    return c;
}

static int os_cond_signal(void *cond) {
    return pthread_cond_signal((pthread_cond_t *)cond);
}

static int os_cond_wait(void *cond, void *mutex) {
    return pthread_cond_wait((pthread_cond_t *)cond, (pthread_mutex_t *)mutex);
}

static void os_cond_destroy(void *cond) {
    pthread_cond_destroy((pthread_cond_t *)cond);
    free(cond);
}
