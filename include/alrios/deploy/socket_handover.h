/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_DEPLOY_SOCKET_HANDOVER_H
#define ALRIOS_DEPLOY_SOCKET_HANDOVER_H

#include <stdatomic.h>
#include <stdint.h>
#include "alrios/platform/types.h"

#define ALRIOS_HANDOVER_PEER_SAME_EUID (1U << 0)
#define ALRIOS_HANDOVER_PEER_UID       (1U << 1)
#define ALRIOS_HANDOVER_PEER_GID       (1U << 2)
#define ALRIOS_HANDOVER_PEER_PID       (1U << 3)

typedef struct alrios_handover_peer_policy {
    uint32_t flags;
    uid_t uid;
    gid_t gid;
    pid_t pid;
} alrios_handover_peer_policy_t;

typedef struct alrios_handover_peer_credentials {
    pid_t pid;
    uid_t uid;
    gid_t gid;
    int available;
} alrios_handover_peer_credentials_t;

typedef struct alrios_listener_route {
    atomic_flag lock;
    int active_fd;
    uint64_t generation;
    int initialized;
} alrios_listener_route_t;

/*
 * The sender retains fd_to_send. On success the receiver owns a distinct,
 * non-blocking, close-on-exec descriptor and must close it. No descriptor is
 * returned on failure. timeout_ms must be positive.
 */
int alrios_socket_handover_send(int control_fd,
                                int fd_to_send,
                                uint64_t generation,
                                const alrios_handover_peer_policy_t *peer_policy,
                                int timeout_ms);

int alrios_socket_handover_receive(int control_fd,
                                   int *out_received_fd,
                                   uint64_t *out_generation,
                                   alrios_handover_peer_credentials_t *out_peer,
                                   const alrios_handover_peer_policy_t *peer_policy,
                                   int timeout_ms);

/*
 * route_init and route_publish transfer ownership of their descriptor to the
 * route on success. route_publish transfers the previous descriptor to
 * out_previous_fd. route_acquire returns a caller-owned duplicate, allowing
 * already accepted streams and in-flight accepts to outlive a route swap.
 */
int alrios_listener_route_init(alrios_listener_route_t *route,
                               int initial_fd,
                               uint64_t generation);
int alrios_listener_route_publish(alrios_listener_route_t *route,
                                  int new_fd,
                                  uint64_t generation,
                                  int *out_previous_fd);
int alrios_listener_route_acquire(alrios_listener_route_t *route,
                                  int *out_fd,
                                  uint64_t *out_generation);
int alrios_listener_route_take(alrios_listener_route_t *route, int *out_fd);

#endif /* ALRIOS_DEPLOY_SOCKET_HANDOVER_H */
