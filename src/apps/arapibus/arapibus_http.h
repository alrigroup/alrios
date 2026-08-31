/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARAPIBUS_HTTP_H
#define ARAPIBUS_HTTP_H

int arapibus_http_server_start(const char *bind_ip, int port);
void arapibus_http_server_stop(void);
int arapibus_http_is_running(void);

#endif /* ARAPIBUS_HTTP_H */
