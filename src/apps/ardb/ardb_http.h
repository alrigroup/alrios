/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARDB_HTTP_H
#define ARDB_HTTP_H

int ardb_http_server_start(const char *bind_ip, int port);
void ardb_http_server_stop(void);
int ardb_http_is_running(void);

#endif /* ARDB_HTTP_H */
