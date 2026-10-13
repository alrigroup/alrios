/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRI_HOTRELOAD_H
#define ALRI_HOTRELOAD_H

#include "alrios/supervisor/context.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int ar_hotreload_init(void);
int ar_hotreload_deploy(const char *app_name, const char *arapp_path, char *out_log, size_t out_log_size);
int ar_hotreload_status(const char *app_name, char *out_buf, size_t out_buf_size);

#ifdef __cplusplus
}
#endif

#endif /* ALRI_HOTRELOAD_H */
