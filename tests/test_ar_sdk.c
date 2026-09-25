/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ALRIOS Sovereign Operating System - Native AR-SDK Unit Test Suite
 * ==================================================================== */
#include "alrios/ar.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

int main(void) {
    /* 1. Context Creation & App ID Validation */
    ar_context_t *ctx = ar_context_create("com.alrigroup.core");
    assert(ctx != NULL);
    assert(strcmp(ar_context_get_app_id(ctx), "com.alrigroup.core") == 0);
    ar_context_destroy(ctx);

    /* 1.1 Edge cases for context creation */
    assert(ar_context_create(NULL) == NULL);
    assert(ar_context_create("") == NULL);
    assert(ar_context_get_app_id(NULL) == NULL);
    ar_context_destroy(NULL); /* Safe no-op */

    /* 2. Arena Allocator Basic Lifecycle */
    ar_arena_t *arena = ar_arena_create(1024);
    assert(arena != NULL);
    assert(ar_arena_get_capacity(arena) == 1024);
    assert(ar_arena_get_used(arena) == 0);

    /* 2.1 Basic Allocation and Safe String Manipulation */
    char *buf = (char *)ar_arena_alloc(arena, 64);
    assert(buf != NULL);
    strncpy(buf, "ARENA_ALLOCATED", 64 - 1);
    buf[63] = '\0';
    assert(strcmp(buf, "ARENA_ALLOCATED") == 0);
    assert(ar_arena_get_used(arena) >= 64);

    /* 2.2 Aligned Allocation Verification */
    void *ptr1 = ar_arena_alloc_aligned(arena, 13, 16);
    assert(ptr1 != NULL);
    assert(((uintptr_t)ptr1 % 16) == 0);

    void *ptr2 = ar_arena_alloc_aligned(arena, 32, 64);
    assert(ptr2 != NULL);
    assert(((uintptr_t)ptr2 % 64) == 0);

    /* 2.3 Calloc verification (Zero-initialized) */
    uint8_t *zeros = (uint8_t *)ar_arena_calloc(arena, 32, sizeof(uint8_t));
    assert(zeros != NULL);
    for (size_t i = 0; i < 32; i++) {
        assert(zeros[i] == 0);
        zeros[i] = (uint8_t)(i + 1); /* Mutate */
    }

    /* 2.4 Arena Reset (Request-scoped lifecycle) */
    ar_arena_reset(arena);
    assert(ar_arena_get_used(arena) == 0);

    /* Allocate again in the same buffer, verify it starts from fresh offset */
    char *fresh_buf = (char *)ar_arena_alloc(arena, 128);
    assert(fresh_buf != NULL);
    assert(fresh_buf == (char *)ar_arena_alloc(arena, 0) || ar_arena_get_used(arena) >= 128);

    /* 2.5 Out-of-memory / Boundary limits */
    size_t remaining = ar_arena_get_capacity(arena) - ar_arena_get_used(arena);
    void *overflow = ar_arena_alloc(arena, remaining + 1);
    assert(overflow == NULL);

    /* 2.6 Edge cases & NULL resilience */
    assert(ar_arena_create(0) == NULL);
    assert(ar_arena_alloc(NULL, 10) == NULL);
    assert(ar_arena_alloc(arena, 0) == NULL);
    assert(ar_arena_calloc(NULL, 10, 10) == NULL);
    assert(ar_arena_calloc(arena, 0, 10) == NULL);
    assert(ar_arena_calloc(arena, 10, 0) == NULL);
    assert(ar_arena_calloc(arena, SIZE_MAX, 2) == NULL);
    assert(ar_arena_get_used(NULL) == 0);
    assert(ar_arena_get_capacity(NULL) == 0);
    ar_arena_reset(NULL); /* Safe no-op */

    ar_arena_destroy(arena);
    ar_arena_destroy(NULL); /* Safe no-op */

    printf("TASK-012 (AR-SDK Core & Arena): PASS\n");
    return 0;
}
