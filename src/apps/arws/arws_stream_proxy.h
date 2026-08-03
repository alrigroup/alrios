/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARWS_STREAM_PROXY_H
#define ARWS_STREAM_PROXY_H

#include "server.h"

#define ARWS_STREAM_BUF_SIZE 16384
#define ARWS_STREAM_TIMEOUT_MS 30000

int arws_stream_proxy_forward(ClientConnection *conn, HttpRequest *req,
                              const char *target_url);

int arws_build_http_request(ClientConnection *conn, HttpRequest *req,
                            unsigned char *buf, int bufsize);

#endif
