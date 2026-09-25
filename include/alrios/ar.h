/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ALRIOS Sovereign Operating System - Native AR-SDK Core
 * ==================================================================== */
#ifndef ALRIOS_AR_SDK_H
#define ALRIOS_AR_SDK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define AR_MAX_APP_ID_LEN 128

/* Core Context */
typedef struct ar_context ar_context_t;

/* Request-scoped Arena Allocator */
typedef struct ar_arena ar_arena_t;

/* Context API */
ar_context_t *ar_context_create(const char *app_id);
const char *ar_context_get_app_id(const ar_context_t *ctx);
void ar_context_destroy(ar_context_t *ctx);

/* Arena Allocator API */
ar_arena_t *ar_arena_create(size_t capacity);
void *ar_arena_alloc(ar_arena_t *arena, size_t size);
void *ar_arena_alloc_aligned(ar_arena_t *arena, size_t size, size_t alignment);
void *ar_arena_calloc(ar_arena_t *arena, size_t nmemb, size_t size);
void ar_arena_reset(ar_arena_t *arena);
size_t ar_arena_get_used(const ar_arena_t *arena);
size_t ar_arena_get_capacity(const ar_arena_t *arena);
void ar_arena_destroy(ar_arena_t *arena);

#endif /* ALRIOS_AR_SDK_H */
