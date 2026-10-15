/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */
#include "hal_private.h"
#include <windows.h>
#include <stdlib.h>
#include <errno.h>

typedef struct {
    void *(*fn)(void *);
    void *arg;
} thread_wrapper_t;

static DWORD WINAPI thread_stub(LPVOID param) {
    thread_wrapper_t *w = (thread_wrapper_t *)param;
    w->fn(w->arg);
    free(w);
    return 0;
}

int os_thread_detach(void *thread) {
    HANDLE h = *(HANDLE *)thread;
    CloseHandle(h);
    free(thread);
    return 0;
}

void *os_thread_create(void *(*fn)(void *), void *arg) {
    thread_wrapper_t *w = malloc(sizeof(thread_wrapper_t));
    if (!w) return NULL;
    w->fn = fn;
    w->arg = arg;

    HANDLE *h = malloc(sizeof(HANDLE));
    if (!h) { free(w); return NULL; }

    *h = CreateThread(NULL, 0, thread_stub, w, 0, NULL);
    if (!*h) { free(h); free(w); return NULL; }
    return h;
}

int os_thread_join(void *thread) {
    HANDLE h = *(HANDLE *)thread;
    WaitForSingleObject(h, INFINITE);
    CloseHandle(h);
    free(thread);
    return 0;
}

void *os_mutex_create(void) {
    CRITICAL_SECTION *cs = malloc(sizeof(CRITICAL_SECTION));
    if (!cs) return NULL;
    InitializeCriticalSection(cs);
    return cs;
}

int os_mutex_lock(void *mutex) {
    EnterCriticalSection((CRITICAL_SECTION *)mutex);
    return 0;
}

int os_mutex_unlock(void *mutex) {
    LeaveCriticalSection((CRITICAL_SECTION *)mutex);
    return 0;
}

void os_mutex_destroy(void *mutex) {
    DeleteCriticalSection((CRITICAL_SECTION *)mutex);
    free(mutex);
}

void *os_cond_create(void) {
    CONDITION_VARIABLE *cv = malloc(sizeof(CONDITION_VARIABLE));
    if (!cv) return NULL;
    InitializeConditionVariable(cv);
    return cv;
}

int os_cond_signal(void *cond) {
    WakeConditionVariable((CONDITION_VARIABLE *)cond);
    return 0;
}

int os_cond_wait(void *cond, void *mutex) {
    SleepConditionVariableCS((CONDITION_VARIABLE *)cond, (CRITICAL_SECTION *)mutex, INFINITE);
    return 0;
}

void os_cond_destroy(void *cond) {
    free(cond);
}
