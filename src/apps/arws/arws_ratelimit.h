/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARWS_RATELIMIT_H
#define ARWS_RATELIMIT_H

void arws_ratelimit_init(void);
void arws_ratelimit_set_rule(const char *host, const char *path_pattern,
                             int max_req, int window_sec);
int  arws_ratelimit_check(const char *ip, const char *host, const char *path);
int  arws_ratelimit_check_ip(const char *ip);
void arws_ratelimit_block_ip(const char *ip, int seconds);
int  arws_ratelimit_is_blocked(const char *ip);

#endif
