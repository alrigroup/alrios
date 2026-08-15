/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ROUTER_H
#define ROUTER_H

#include "server.h"

void add_route(const char *path, const char *method, const char *domain, RequestHandler handler);
void router_dispatch(ClientConnection *conn, HttpRequest *req);

#endif
