/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARWS_PROXY_H
#define ARWS_PROXY_H

#define ARWS_PROXY_TIMEOUT_MS 10000

int arws_proxy_init(void);
int arws_proxy_forward(const char *target_url,
                       const unsigned char *raw_req, int raw_len,
                       unsigned char *out_resp, int max_resp_len);

#endif
