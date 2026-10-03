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

#include "alrios/supervisor/context.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct registered_app {
    char app_id[128];
    char arapp_path[256];
    int active;
} registered_app_t;

struct alrios_supervisor_ctx {
    registered_app_t *apps;
    size_t capacity;
    size_t count;
};

int alrios_supervisor_ctx_create(size_t max_apps, alrios_supervisor_ctx_t **out_ctx) {
    if (!out_ctx || max_apps == 0U) {
        return ALRIOS_SUPERVISOR_ERR_INVALID;
    }
    *out_ctx = NULL;
    alrios_supervisor_ctx_t *ctx = calloc(1U, sizeof(*ctx));
    if (!ctx) {
        return ALRIOS_SUPERVISOR_ERR_INVALID;
    }
    ctx->apps = calloc(max_apps, sizeof(registered_app_t));
    if (!ctx->apps) {
        free(ctx);
        return ALRIOS_SUPERVISOR_ERR_INVALID;
    }
    ctx->capacity = max_apps;
    ctx->count = 0U;
    *out_ctx = ctx;
    return ALRIOS_SUPERVISOR_OK;
}

void alrios_supervisor_ctx_destroy(alrios_supervisor_ctx_t *ctx) {
    if (!ctx) return;
    free(ctx->apps);
    free(ctx);
}

int alrios_supervisor_register_app(alrios_supervisor_ctx_t *ctx, const char *app_id, const char *arapp_path) {
    if (!ctx || !app_id || !arapp_path) {
        return ALRIOS_SUPERVISOR_ERR_INVALID;
    }
    for (size_t i = 0; i < ctx->capacity; i++) {
        if (ctx->apps[i].active && strcmp(ctx->apps[i].app_id, app_id) == 0) {
            snprintf(ctx->apps[i].arapp_path, sizeof(ctx->apps[i].arapp_path), "%s", arapp_path);
            return ALRIOS_SUPERVISOR_OK;
        }
    }
    if (ctx->count >= ctx->capacity) {
        return ALRIOS_SUPERVISOR_ERR_CAPACITY;
    }
    for (size_t i = 0; i < ctx->capacity; i++) {
        if (!ctx->apps[i].active) {
            snprintf(ctx->apps[i].app_id, sizeof(ctx->apps[i].app_id), "%s", app_id);
            snprintf(ctx->apps[i].arapp_path, sizeof(ctx->apps[i].arapp_path), "%s", arapp_path);
            ctx->apps[i].active = 1;
            ctx->count++;
            return ALRIOS_SUPERVISOR_OK;
        }
    }
    return ALRIOS_SUPERVISOR_ERR_CAPACITY;
}

int alrios_supervisor_unregister_app(alrios_supervisor_ctx_t *ctx, const char *app_id) {
    if (!ctx || !app_id) {
        return ALRIOS_SUPERVISOR_ERR_INVALID;
    }
    for (size_t i = 0; i < ctx->capacity; i++) {
        if (ctx->apps[i].active && strcmp(ctx->apps[i].app_id, app_id) == 0) {
            memset(&ctx->apps[i], 0, sizeof(registered_app_t));
            if (ctx->count > 0) ctx->count--;
            return ALRIOS_SUPERVISOR_OK;
        }
    }
    return ALRIOS_SUPERVISOR_ERR_NOT_FOUND;
}

size_t alrios_supervisor_get_app_count(const alrios_supervisor_ctx_t *ctx) {
    if (!ctx) return 0U;
    return ctx->count;
}
