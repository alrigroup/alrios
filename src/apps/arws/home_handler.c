/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "home_web_data.h"
#include "server.h"
#include "arws_utils.h"
#include <string.h>

static int origin_is_allowed(HttpRequest *req) {
    if (!req->host || req->host[0] == '\0') return 0;
    if (strcmp(req->host, "alrigroup.com") == 0) return 1;
    if (strcmp(req->host, "www.alrigroup.com") == 0) return 1;
    if (strcmp(req->host, "localhost") == 0) return 1;
    if (strncmp(req->host, "localhost:", 10) == 0) return 1;
    return 0;
}

void home_handler_html(ClientConnection *conn, HttpRequest *req) {
    (void)req;
    server_send_response(conn, 200, "text/html; charset=utf-8", home_html);
}

void home_handler_css(ClientConnection *conn, HttpRequest *req) {
    if (!origin_is_allowed(req)) {
        arws_send_403(conn, "Forbidden");
        return;
    }
    server_send_response(conn, 200, "text/css; charset=utf-8", home_style_css);
}

void home_handler_js(ClientConnection *conn, HttpRequest *req) {
    if (!origin_is_allowed(req)) {
        arws_send_403(conn, "Forbidden");
        return;
    }
    server_send_response(conn, 200, "application/javascript; charset=utf-8", home_script_js);
}

#ifdef __cplusplus
}
#endif
