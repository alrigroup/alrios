/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "alrios/fd_pass.h"
#include "alrios/deploy_drain.h"

int alrios_supervisor_handover_listener(int control_socket, int listener_fd) {
    if (control_socket < 0 || listener_fd < 0) return -1;
    return alrios_send_live_fd(control_socket, listener_fd);
}

int alrios_supervisor_receive_listener(int control_socket) {
    if (control_socket < 0) return -1;
    return alrios_recv_live_fd(control_socket);
}

int alrios_supervisor_begin_drain(alrios_drain_tracker_t *tracker,
                                  const alrios_drain_config_t *config) {
    return alrios_drain_wait(tracker, config);
}
