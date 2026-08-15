/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARWS_UTILS_H
#define ARWS_UTILS_H

#include "server.h"

void arws_sendpage(ClientConnection *conn, const char *html);
void arws_send_200(ClientConnection *conn, const char *body);
void arws_send_201(ClientConnection *conn, const char *body);
void arws_send_204(ClientConnection *conn);
void arws_send_301(ClientConnection *conn, const char *url);
void arws_send_302(ClientConnection *conn, const char *url);
void arws_send_400(ClientConnection *conn, const char *msg);
void arws_send_401(ClientConnection *conn, const char *msg);
void arws_send_403(ClientConnection *conn, const char *msg);
void arws_send_429(ClientConnection *conn, const char *msg);
void arws_send_404(ClientConnection *conn);
void arws_send_maintenance(ClientConnection *conn);
void arws_send_500(ClientConnection *conn, const char *msg);
void arws_send_501(ClientConnection *conn, const char *msg);
void arws_send_502(ClientConnection *conn, const char *msg);
void arws_send_503(ClientConnection *conn, const char *msg);
void arws_send_json(ClientConnection *conn, int status, const char *json);
void arws_send_error(ClientConnection *conn, int status, const char *msg);
int  arws_serve_file(ClientConnection *conn, const char *path, const char *content_type);

#endif
