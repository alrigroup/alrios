/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */
#include "hotreload.h"
#include "loader.h"
#include "alrios/supervisor/context.h"

int ar_hotreload_init(void) {
    return ar_supervisor_hotreload_init(loader_get_supervisor_context());
}

int ar_hotreload_deploy(const char *app_name, const char *arapp_path, char *out_log, size_t out_log_size) {
    return ar_supervisor_hotreload_deploy(
        loader_get_supervisor_context(), app_name, arapp_path, out_log, out_log_size);
}

int ar_hotreload_status(const char *app_name, char *out_buf, size_t out_buf_size) {
    return ar_supervisor_hotreload_status(
        loader_get_supervisor_context(), app_name, out_buf, out_buf_size);
}
