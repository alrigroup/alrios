/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/supervisor/guest_router.h"

struct ar_guest_router {
    int unused;
};

int ar_guest_router_create(const ar_guest_router_config_t *config,
                           ar_guest_router_t **out_router) {
    (void)config;
    if (out_router) *out_router = NULL;
    return AR_GUEST_ERR_UNSUPPORTED;
}

void ar_guest_router_destroy(ar_guest_router_t *router) {
    (void)router;
}

int ar_guest_router_register(ar_guest_router_t *router,
                             const char *app_id,
                             const ar_guest_identity_policy_t *policy,
                             int *out_guest_fd) {
    (void)router; (void)app_id; (void)policy;
    if (out_guest_fd) *out_guest_fd = -1;
    return AR_GUEST_ERR_UNSUPPORTED;
}

int ar_guest_router_attach(ar_guest_router_t *router,
                           const char *app_id,
                           int host_fd,
                           const ar_guest_identity_policy_t *policy) {
    (void)router; (void)app_id; (void)host_fd; (void)policy;
    return AR_GUEST_ERR_UNSUPPORTED;
}

int ar_guest_router_unregister(ar_guest_router_t *router, const char *app_id) {
    (void)router; (void)app_id;
    return AR_GUEST_ERR_UNSUPPORTED;
}

int ar_guest_router_has_guest(const ar_guest_router_t *router, const char *app_id) {
    (void)router; (void)app_id;
    return 0;
}

int ar_guest_router_send(ar_guest_router_t *router, const char *app_id,
                         uint16_t message_type, uint32_t flags,
                         uint64_t request_id, const void *payload,
                         uint32_t payload_length) {
    (void)router; (void)app_id; (void)message_type; (void)flags;
    (void)request_id; (void)payload; (void)payload_length;
    return AR_GUEST_ERR_UNSUPPORTED;
}

int ar_guest_router_broadcast(ar_guest_router_t *router,
                              uint16_t message_type, uint32_t flags,
                              uint64_t request_id, const void *payload,
                              uint32_t payload_length) {
    (void)router; (void)message_type; (void)flags;
    (void)request_id; (void)payload; (void)payload_length;
    return AR_GUEST_ERR_UNSUPPORTED;
}

int ar_guest_router_flush(ar_guest_router_t *router, const char *app_id) {
    (void)router; (void)app_id;
    return AR_GUEST_ERR_UNSUPPORTED;
}

int ar_guest_router_dispatch(ar_guest_router_t *router, int timeout_ms,
                             ar_guest_message_handler_fn handler,
                             void *user_data) {
    (void)router; (void)timeout_ms; (void)handler; (void)user_data;
    return AR_GUEST_ERR_UNSUPPORTED;
}

int ar_guest_router_get_stats(const ar_guest_router_t *router,
                              const char *app_id,
                              ar_guest_route_stats_t *out_stats) {
    (void)router; (void)app_id; (void)out_stats;
    return AR_GUEST_ERR_UNSUPPORTED;
}
