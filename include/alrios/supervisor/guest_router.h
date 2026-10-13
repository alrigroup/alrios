/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_SUPERVISOR_GUEST_ROUTER_H
#define ALRIOS_SUPERVISOR_GUEST_ROUTER_H

#include "alrios/ipc/guest_channel.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AR_GUEST_ROUTER_MAX_GUESTS           64U
#define AR_GUEST_ROUTER_DEFAULT_QUEUE_DEPTH  64U
#define AR_GUEST_APP_ID_MAX_LEN              64U

typedef struct ar_guest_router ar_guest_router_t;

typedef enum ar_guest_flood_policy {
    AR_GUEST_FLOOD_DROP = 0,
    AR_GUEST_FLOOD_BACKPRESSURE = 1,
    AR_GUEST_FLOOD_DISCONNECT = 2
} ar_guest_flood_policy_t;

typedef struct ar_guest_route_stats {
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t messages_dropped;
    uint64_t auth_failures;
    uint64_t protocol_errors;
} ar_guest_route_stats_t;

typedef struct ar_guest_router_config {
    size_t max_guests;
    size_t max_queue_depth;
    uint32_t max_payload_size;
    int io_timeout_ms;
    ar_guest_flood_policy_t flood_policy;
} ar_guest_router_config_t;

typedef int (*ar_guest_message_handler_fn)(
    ar_guest_router_t *router,
    const char *app_id,
    const ar_guest_frame_header_t *header,
    const void *payload,
    void *user_data);

int ar_guest_router_create(
    const ar_guest_router_config_t *config,
    ar_guest_router_t **out_router);

void ar_guest_router_destroy(ar_guest_router_t *router);

int ar_guest_router_register(
    ar_guest_router_t *router,
    const char *app_id,
    const ar_guest_identity_policy_t *policy,
    int *out_guest_fd);

int ar_guest_router_attach(
    ar_guest_router_t *router,
    const char *app_id,
    int host_fd,
    const ar_guest_identity_policy_t *policy);

int ar_guest_router_unregister(
    ar_guest_router_t *router,
    const char *app_id);

int ar_guest_router_has_guest(
    const ar_guest_router_t *router,
    const char *app_id);

int ar_guest_router_send(
    ar_guest_router_t *router,
    const char *app_id,
    uint16_t message_type,
    uint32_t flags,
    uint64_t request_id,
    const void *payload,
    uint32_t payload_length);

int ar_guest_router_broadcast(
    ar_guest_router_t *router,
    uint16_t message_type,
    uint32_t flags,
    uint64_t request_id,
    const void *payload,
    uint32_t payload_length);

int ar_guest_router_flush(
    ar_guest_router_t *router,
    const char *app_id);

int ar_guest_router_dispatch(
    ar_guest_router_t *router,
    int timeout_ms,
    ar_guest_message_handler_fn handler,
    void *user_data);

int ar_guest_router_get_stats(
    const ar_guest_router_t *router,
    const char *app_id,
    ar_guest_route_stats_t *out_stats);

#ifdef __cplusplus
}
#endif

#endif /* ALRIOS_SUPERVISOR_GUEST_ROUTER_H */
