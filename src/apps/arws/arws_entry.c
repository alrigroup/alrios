/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arws_gateway.h"
#include "arws_utils.h"
#include "arws_config.h"
#include "arws_ratelimit.h"
#include "arws_session.h"
#include "ar_svc.h"
#include "log.h"
#include <stdio.h>

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

EXPORT int arws_entry(void) {
    int rc = arws_config_load("storage/arws/arws.cfg");
    alri_print_force(CYN "[ARWS]" RST " Config load: %d\n", rc);

    int port = arws_config_get_port();
    int mode = arws_config_get_operation_mode();
    const char *bind = arws_config_get_bind_address();

    server_set_bind_address(bind);

    alri_print_force(CYN "[ARWS]" RST " Alri Web Services starting...\n");
    alri_print_force(CYN "[ARWS]" RST " Mode: %s, Port: %d, Bind: %s (opmode=%d)\n",
                     arws_config_get_mode_name(), port, bind, mode);

    if (arws_init() != 0) {
        alri_print_force(RED "[ARWS]" RST " Gateway init failed\n");
        return -1;
    }

    /* enable graceful stop/restart from the core control channel */
    ar_svc_set_stop_hook("arws", arws_stop);

    /* Sem regras hardcoded de rate limit: quem define o teto sao as apps,
       via IPC_REGISTER com rl=<max>,<window>. Sem regra, aplica-se o
       default alto do arws (rede de seguranca). */

    if (arws_start(port, mode) != 0) {
        alri_print_force(RED "[ARWS]" RST " Gateway start failed\n");
        return -1;
    }

    return 0;
}
