/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_SUPERVISOR_CONTEXT_H
#define ALRIOS_SUPERVISOR_CONTEXT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALRIOS_SUPERVISOR_OK 0
#define ALRIOS_SUPERVISOR_ERR_INVALID -3001
#define ALRIOS_SUPERVISOR_ERR_CAPACITY -3002
#define ALRIOS_SUPERVISOR_ERR_NOT_FOUND -3003

typedef struct alrios_supervisor_ctx alrios_supervisor_ctx_t;

int alrios_supervisor_ctx_create(size_t max_apps, alrios_supervisor_ctx_t **out_ctx);
void alrios_supervisor_ctx_destroy(alrios_supervisor_ctx_t *ctx);

int alrios_supervisor_register_app(alrios_supervisor_ctx_t *ctx, const char *app_id, const char *arapp_path);
int alrios_supervisor_unregister_app(alrios_supervisor_ctx_t *ctx, const char *app_id);
size_t alrios_supervisor_get_app_count(const alrios_supervisor_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif
