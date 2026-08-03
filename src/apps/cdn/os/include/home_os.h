/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef HOME_OS_H
#define HOME_OS_H

#include <stdint.h>

int home_os_get_exe_dir(char *buf, int size);
int home_os_connect_timeout(int fd, const char *addr, uint16_t port, int timeout_ms);
int home_os_set_recv_timeout(int fd, int timeout_ms);

#endif
