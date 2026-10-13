/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ALRIOS Sovereign Operating System - Native AR-SDK Core Implementation
 * ==================================================================== */
#include "alrios/ar.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define AR_DEFAULT_ALIGNMENT (sizeof(void *))

struct ar_context {
    char app_id[AR_MAX_APP_ID_LEN];
};

struct ar_arena {
    uint8_t *buffer;
    size_t capacity;
    size_t offset;
};

ar_context_t *ar_context_create(const char *app_id) {
    if (!app_id) {
        return NULL;
    }

    size_t len = strlen(app_id);
    if (len == 0 || len >= AR_MAX_APP_ID_LEN) {
        return NULL;
    }

    ar_context_t *ctx = (ar_context_t *)malloc(sizeof(ar_context_t));
    if (!ctx) {
        return NULL;
    }

    strncpy(ctx->app_id, app_id, sizeof(ctx->app_id) - 1);
    ctx->app_id[sizeof(ctx->app_id) - 1] = '\0';
    return ctx;
}

const char *ar_context_get_app_id(const ar_context_t *ctx) {
    if (!ctx) {
        return NULL;
    }
    return ctx->app_id;
}

void ar_context_destroy(ar_context_t *ctx) {
    if (ctx) {
        /* Scrub sensitive memory before deallocation */
        explicit_bzero(ctx, sizeof(ar_context_t));
        free(ctx);
    }
}

ar_arena_t *ar_arena_create(size_t capacity) {
    if (capacity == 0) {
        return NULL;
    }

    ar_arena_t *a = (ar_arena_t *)malloc(sizeof(ar_arena_t));
    if (!a) {
        return NULL;
    }

    a->buffer = (uint8_t *)calloc(1, capacity);
    if (!a->buffer) {
        free(a);
        return NULL;
    }

    a->capacity = capacity;
    a->offset = 0;
    return a;
}

void *ar_arena_alloc_aligned(ar_arena_t *arena, size_t size, size_t alignment) {
    if (!arena || !arena->buffer || size == 0) {
        return NULL;
    }

    /* Alignment must be a power of 2 */
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        alignment = AR_DEFAULT_ALIGNMENT;
    }

    uintptr_t current_addr = (uintptr_t)(arena->buffer + arena->offset);
    uintptr_t aligned_addr = (current_addr + (alignment - 1)) & ~(uintptr_t)(alignment - 1);
    size_t padding = (size_t)(aligned_addr - current_addr);

    /* Check integer overflow and capacity limit */
    if (padding > arena->capacity - arena->offset) {
        return NULL;
    }
    if (size > arena->capacity - (arena->offset + padding)) {
        return NULL;
    }

    size_t new_offset = arena->offset + padding + size;
    void *ptr = (void *)aligned_addr;
    arena->offset = new_offset;
    return ptr;
}

void *ar_arena_alloc(ar_arena_t *arena, size_t size) {
    return ar_arena_alloc_aligned(arena, size, AR_DEFAULT_ALIGNMENT);
}

void *ar_arena_calloc(ar_arena_t *arena, size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) {
        return NULL;
    }
    /* Check multiplication overflow */
    if (nmemb > SIZE_MAX / size) {
        return NULL;
    }
    size_t total = nmemb * size;
    void *ptr = ar_arena_alloc(arena, total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void ar_arena_reset(ar_arena_t *arena) {
    if (arena && arena->buffer) {
        /* Zero out request memory to prevent data remanence across scoped requests */
        explicit_bzero(arena->buffer, arena->offset);
        arena->offset = 0;
    }
}

size_t ar_arena_get_used(const ar_arena_t *arena) {
    if (!arena) {
        return 0;
    }
    return arena->offset;
}

size_t ar_arena_get_capacity(const ar_arena_t *arena) {
    if (!arena) {
        return 0;
    }
    return arena->capacity;
}

void ar_arena_destroy(ar_arena_t *arena) {
    if (arena) {
        if (arena->buffer) {
            explicit_bzero(arena->buffer, arena->capacity);
            free(arena->buffer);
            arena->buffer = NULL;
        }
        explicit_bzero(arena, sizeof(ar_arena_t));
        free(arena);
    }
}
