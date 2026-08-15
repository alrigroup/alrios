/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef API_H
#define API_H

#include "server.h"

#define OPERATION_MODE MODE_INSECURE
#define SERVER_PORT 8080

typedef void (*RouteHandler)(ClientConnection *conn, HttpRequest *req);

typedef struct Route {
    char path[256];
    char method[16];
    RouteHandler handler;
    struct Route *next;
} Route;

void api_add_route(const char *path, const char *method, RouteHandler handler);
void send_page(ClientConnection *conn, const char *folder_name, const char *request_path);
void api_plugin_handler(ClientConnection *conn, HttpRequest *req);
void api_plugin_init();

#endif
