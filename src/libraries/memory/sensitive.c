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

#include "alrios/memory/sensitive.h"
#include "alrios/crypto_verify.h"
#include <alrios/platform/aros_hal.h>

#include <errno.h>
#include <string.h>

#if defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <windows.h>
typedef SRWLOCK alrios_lock_t;
#define ALRIOS_LOCK_INIT SRWLOCK_INIT
#define alrios_lock_acquire(l) AcquireSRWLockExclusive(l)
#define alrios_lock_release(l) ReleaseSRWLockExclusive(l)
#else
#include <pthread.h>
#include <unistd.h>
typedef pthread_mutex_t alrios_lock_t;
#define ALRIOS_LOCK_INIT PTHREAD_MUTEX_INITIALIZER
#define alrios_lock_acquire(l) (void)pthread_mutex_lock(l)
#define alrios_lock_release(l) (void)pthread_mutex_unlock(l)
#endif

#if defined(__linux__)
#include <sys/mman.h>
#ifndef MADV_DONTDUMP
#define MADV_DONTDUMP 16
#endif
#ifndef MADV_DODUMP
#define MADV_DODUMP 17
#endif
#endif

typedef struct sensitive_registry {
    alrios_lock_t lock;
    alrios_sensitive_range_t ranges[ALRIOS_SENSITIVE_MAX_REGISTRATIONS];
    size_t count;
    bool initialized;
} sensitive_registry_t;

static sensitive_registry_t g_registry = {
    .lock = ALRIOS_LOCK_INIT,
    .count = 0,
    .initialized = true
};

#if defined(__linux__)
static int apply_madvise(void *ptr, size_t size, int advice) {
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        page_size = 4096;
    }

    uintptr_t start = (uintptr_t)ptr;
    uintptr_t aligned_start = start & ~((uintptr_t)page_size - 1U);
    uintptr_t end = start + size;
    uintptr_t aligned_end = (end + (uintptr_t)page_size - 1U) & ~((uintptr_t)page_size - 1U);
    size_t aligned_len = (size_t)(aligned_end - aligned_start);

    /* madvise on the page-aligned range */
    int ret = madvise((void *)aligned_start, aligned_len, advice);
    if (ret != 0 && errno != EINVAL && errno != ENOMEM) {
        return ALRIOS_SENSITIVE_ERR_SYSTEM;
    }
    return ALRIOS_SENSITIVE_OK;
}
#elif defined(_WIN32)
static int apply_memlock(void *ptr, size_t size, bool lock) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    DWORD page_size = si.dwPageSize ? si.dwPageSize : 4096;

    uintptr_t start = (uintptr_t)ptr;
    uintptr_t aligned_start = start & ~((uintptr_t)page_size - 1U);
    uintptr_t end = start + size;
    uintptr_t aligned_end = (end + (uintptr_t)page_size - 1U) & ~((uintptr_t)page_size - 1U);
    size_t aligned_len = (size_t)(aligned_end - aligned_start);

    if (lock) {
        if (!VirtualLock((LPVOID)aligned_start, aligned_len)) {
            return ALRIOS_SENSITIVE_OK;
        }
    } else {
        VirtualUnlock((LPVOID)aligned_start, aligned_len);
    }
    return ALRIOS_SENSITIVE_OK;
}
#endif

int alrios_sensitive_register(void *ptr, size_t size) {
    if (!ptr || size == 0) {
        return ALRIOS_SENSITIVE_ERR_INVALID_ARGUMENT;
    }

    alrios_lock_acquire(&g_registry.lock);

    /* Check if already registered */
    for (size_t i = 0; i < g_registry.count; ++i) {
        if (g_registry.ranges[i].ptr == ptr && g_registry.ranges[i].size == size) {
            alrios_lock_release(&g_registry.lock);
            return ALRIOS_SENSITIVE_OK;
        }
    }

    if (g_registry.count >= ALRIOS_SENSITIVE_MAX_REGISTRATIONS) {
        alrios_lock_release(&g_registry.lock);
        return ALRIOS_SENSITIVE_ERR_CAPACITY;
    }

#if defined(__linux__)
    apply_madvise(ptr, size, MADV_DONTDUMP);
#elif defined(_WIN32)
    apply_memlock(ptr, size, true);
#endif

    g_registry.ranges[g_registry.count].ptr = ptr;
    g_registry.ranges[g_registry.count].size = size;
    g_registry.count++;

    alrios_lock_release(&g_registry.lock);
    return ALRIOS_SENSITIVE_OK;
}

int alrios_sensitive_unregister(void *ptr, size_t size) {
    if (!ptr || size == 0) {
        return ALRIOS_SENSITIVE_ERR_INVALID_ARGUMENT;
    }

    alrios_lock_acquire(&g_registry.lock);

    size_t found_idx = (size_t)-1;
    for (size_t i = 0; i < g_registry.count; ++i) {
        if (g_registry.ranges[i].ptr == ptr) {
            found_idx = i;
            break;
        }
    }

    if (found_idx == (size_t)-1) {
        alrios_lock_release(&g_registry.lock);
        return ALRIOS_SENSITIVE_ERR_NOT_FOUND;
    }

    /* Explicitly zeroize the sensitive memory */
    alrios_explicit_zeroize(ptr, size);

#if defined(__linux__)
    apply_madvise(ptr, size, MADV_DODUMP);
#elif defined(_WIN32)
    apply_memlock(ptr, size, false);
#endif

    /* Shift remaining entries down */
    for (size_t i = found_idx; i + 1 < g_registry.count; ++i) {
        g_registry.ranges[i] = g_registry.ranges[i + 1];
    }
    g_registry.count--;
    g_registry.ranges[g_registry.count].ptr = NULL;
    g_registry.ranges[g_registry.count].size = 0;

    alrios_lock_release(&g_registry.lock);
    return ALRIOS_SENSITIVE_OK;
}

bool alrios_sensitive_is_registered(const void *ptr, size_t size) {
    if (!ptr || size == 0) {
        return false;
    }

    uintptr_t check_start = (uintptr_t)ptr;
    uintptr_t check_end = check_start + size;

    alrios_lock_acquire(&g_registry.lock);

    for (size_t i = 0; i < g_registry.count; ++i) {
        uintptr_t reg_start = (uintptr_t)g_registry.ranges[i].ptr;
        uintptr_t reg_end = reg_start + g_registry.ranges[i].size;

        if (check_start < reg_end && check_end > reg_start) {
            alrios_lock_release(&g_registry.lock);
            return true;
        }
    }

    alrios_lock_release(&g_registry.lock);
    return false;
}

int alrios_sensitive_get_ranges(alrios_sensitive_range_t *out_ranges,
                                size_t max_ranges,
                                size_t *out_count) {
    alrios_lock_acquire(&g_registry.lock);

    if (out_count) {
        *out_count = g_registry.count;
    }

    if (out_ranges && max_ranges > 0) {
        size_t copy_count = g_registry.count < max_ranges ? g_registry.count : max_ranges;
        for (size_t i = 0; i < copy_count; ++i) {
            out_ranges[i] = g_registry.ranges[i];
        }
    }

    alrios_lock_release(&g_registry.lock);
    return ALRIOS_SENSITIVE_OK;
}

void alrios_sensitive_scrub_all(void) {
    alrios_lock_acquire(&g_registry.lock);

    for (size_t i = 0; i < g_registry.count; ++i) {
        if (g_registry.ranges[i].ptr && g_registry.ranges[i].size > 0) {
            alrios_explicit_zeroize(g_registry.ranges[i].ptr, g_registry.ranges[i].size);
        }
    }

    alrios_lock_release(&g_registry.lock);
}

void alrios_sensitive_clear_all(void) {
    alrios_lock_acquire(&g_registry.lock);

    for (size_t i = 0; i < g_registry.count; ++i) {
        if (g_registry.ranges[i].ptr && g_registry.ranges[i].size > 0) {
            alrios_explicit_zeroize(g_registry.ranges[i].ptr, g_registry.ranges[i].size);
#if defined(__linux__)
            apply_madvise(g_registry.ranges[i].ptr, g_registry.ranges[i].size, MADV_DODUMP);
#elif defined(_WIN32)
            apply_memlock(g_registry.ranges[i].ptr, g_registry.ranges[i].size, false);
#endif
        }
        g_registry.ranges[i].ptr = NULL;
        g_registry.ranges[i].size = 0;
    }
    g_registry.count = 0;

    alrios_lock_release(&g_registry.lock);
}
