/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARWN_GATEWAY_H
#define ARWN_GATEWAY_H

#include "arwn.h"
#include "arwn_server.h"

/* Conector ARWS (Fase 2): registra as rotas do app via IPC 9500 (frame
 * 5-byte existente), mantém canal de controle com heartbeat + query
 * (ping/status/routes) e reconecta com backoff (padrão home_server.c).
 */

/* Inicia a thread de registro/controle. Retorna 0 se a thread subiu
   (a reconexão é interna). `server` é usado para responder "routes". */
int arwn_gateway_start(arwn_app_t *app, arwn_server_t *server);

/* Server corrente para o thread do gateway (por processo). */
arwn_server_t *arwn_server_for_gateway(void);

#endif /* ARWN_GATEWAY_H */