/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "server.h"
#include "router.h"
#include "ratelimit.h"
#include "api.h"
#include "log.h"

int bemf_entry(void) {
    alri_print_force(CYN "[BEMF]" RST " Starting Bemf HTTP server...\n");
    ratelimit_init();
    api_plugin_init();
    if (server_start(SERVER_PORT, OPERATION_MODE, api_plugin_handler) != 0) {
        alri_print_force(RED "[BEMF]" RST " Failed to start HTTP server\n");
        return -1;
    }
    return 0;
}
