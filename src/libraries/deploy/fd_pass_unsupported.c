/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/fd_pass.h"

int alrios_send_live_fd(int unix_sock, int fd_to_send) {
    (void)unix_sock;
    (void)fd_to_send;
    return -1;
}

int alrios_recv_live_fd(int unix_sock) {
    (void)unix_sock;
    return -1;
}
