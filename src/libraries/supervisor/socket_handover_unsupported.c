/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/deploy/socket_handover.h"

#include <string.h>

int alrios_socket_handover_send(int control_fd,
                                int fd_to_send,
                                uint64_t generation,
                                const alrios_handover_peer_policy_t *peer_policy,
                                int timeout_ms) {
    (void)control_fd;
    (void)fd_to_send;
    (void)generation;
    (void)peer_policy;
    (void)timeout_ms;
    return -1;
}

int alrios_socket_handover_receive(int control_fd,
                                   int *out_received_fd,
                                   uint64_t *out_generation,
                                   alrios_handover_peer_credentials_t *out_peer,
                                   const alrios_handover_peer_policy_t *peer_policy,
                                   int timeout_ms) {
    (void)control_fd;
    (void)peer_policy;
    (void)timeout_ms;
    if (out_received_fd) *out_received_fd = -1;
    if (out_generation) *out_generation = 0U;
    if (out_peer) memset(out_peer, 0, sizeof(*out_peer));
    return -1;
}

int alrios_listener_route_init(alrios_listener_route_t *route,
                               int initial_fd,
                               uint64_t generation) {
    (void)route;
    (void)initial_fd;
    (void)generation;
    return -1;
}

int alrios_listener_route_publish(alrios_listener_route_t *route,
                                  int new_fd,
                                  uint64_t generation,
                                  int *out_previous_fd) {
    (void)route;
    (void)new_fd;
    (void)generation;
    if (out_previous_fd) *out_previous_fd = -1;
    return -1;
}

int alrios_listener_route_acquire(alrios_listener_route_t *route,
                                  int *out_fd,
                                  uint64_t *out_generation) {
    (void)route;
    if (out_fd) *out_fd = -1;
    if (out_generation) *out_generation = 0U;
    return -1;
}

int alrios_listener_route_take(alrios_listener_route_t *route, int *out_fd) {
    (void)route;
    if (out_fd) *out_fd = -1;
    return -1;
}
