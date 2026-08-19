/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARWS_HEALTH_H
#define ARWS_HEALTH_H

#include "arws_upstream.h"

/* Inicia a thread de monitoramento assíncrono de saúde dos nós */
int arws_health_start(void);

/* Para a thread de monitoramento */
void arws_health_stop(void);

#endif /* ARWS_HEALTH_H */
