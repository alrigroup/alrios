/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARAPICLOUD_HTTP_H
#define ARAPICLOUD_HTTP_H

int arapicloud_http_server_start(const char *bind_ip, int port);
void arapicloud_http_server_stop(void);
int arapicloud_http_is_running(void);

#endif /* ARAPICLOUD_HTTP_H */
