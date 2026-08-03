/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef RATELIMIT_H
#define RATELIMIT_H

void ratelimit_init();
int ratelimit_check(const char *ip, const char *path);

#endif
