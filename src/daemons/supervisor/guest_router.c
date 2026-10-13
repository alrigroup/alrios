/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/supervisor/guest_router.h"

#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct ar_guest_queue_entry {
    uint16_t message_type;
    uint32_t flags;
    uint64_t request_id;
    uint32_t payload_length;
    uint8_t *payload;
} ar_guest_queue_entry_t;

typedef struct ar_guest_route_entry {
    char app_id[AR_GUEST_APP_ID_MAX_LEN];
    int active;
    int quarantined;
    ar_guest_channel_t *channel;
    ar_guest_identity_policy_t policy;
    ar_guest_route_stats_t stats;

    ar_guest_queue_entry_t *queue;
    size_t queue_capacity;
    size_t queue_head;
    size_t queue_count;
} ar_guest_route_entry_t;

struct ar_guest_router {
    ar_guest_router_config_t config;
    ar_guest_route_entry_t *entries;
    size_t guest_count;
};

static void free_entry_queue(ar_guest_route_entry_t *entry) {
    if (entry->queue != NULL) {
        while (entry->queue_count > 0U) {
            ar_guest_queue_entry_t *q = &entry->queue[entry->queue_head];
            free(q->payload);
            q->payload = NULL;
            entry->queue_head = (entry->queue_head + 1U) % entry->queue_capacity;
            entry->queue_count--;
        }
        free(entry->queue);
        entry->queue = NULL;
    }
    entry->queue_capacity = 0U;
    entry->queue_head = 0U;
    entry->queue_count = 0U;
}

static int flush_entry_queue(ar_guest_route_entry_t *entry) {
    while (entry->queue_count > 0U) {
        ar_guest_queue_entry_t *q = &entry->queue[entry->queue_head];
        int rc = ar_guest_channel_send(
            entry->channel,
            q->message_type,
            q->flags,
            q->request_id,
            q->payload,
            q->payload_length);
        if (rc == AR_GUEST_OK) {
            free(q->payload);
            q->payload = NULL;
            entry->queue_head = (entry->queue_head + 1U) % entry->queue_capacity;
            entry->queue_count--;
        } else {
            return rc;
        }
    }
    return AR_GUEST_OK;
}

static ar_guest_route_entry_t *find_entry(ar_guest_router_t *router, const char *app_id) {
    size_t i;
    if (!router || !app_id) {
        return NULL;
    }
    for (i = 0U; i < router->config.max_guests; ++i) {
        if (router->entries[i].active &&
            strncmp(router->entries[i].app_id, app_id, AR_GUEST_APP_ID_MAX_LEN) == 0) {
            return &router->entries[i];
        }
    }
    return NULL;
}

int ar_guest_router_create(
    const ar_guest_router_config_t *config,
    ar_guest_router_t **out_router) {
    ar_guest_router_t *router;

    if (!out_router) {
        return AR_GUEST_ERR_INVALID;
    }

    router = (ar_guest_router_t *)calloc(1U, sizeof(ar_guest_router_t));
    if (!router) {
        return AR_GUEST_ERR_NOMEM;
    }

    if (config != NULL) {
        router->config = *config;
    } else {
        router->config.max_guests = AR_GUEST_ROUTER_MAX_GUESTS;
        router->config.max_queue_depth = AR_GUEST_ROUTER_DEFAULT_QUEUE_DEPTH;
        router->config.max_payload_size = AR_GUEST_HARD_MAX_PAYLOAD_SIZE;
        router->config.io_timeout_ms = 0;
        router->config.flood_policy = AR_GUEST_FLOOD_DROP;
    }

    if (router->config.max_guests == 0U) {
        router->config.max_guests = AR_GUEST_ROUTER_MAX_GUESTS;
    }
    if (router->config.max_queue_depth == 0U) {
        router->config.max_queue_depth = AR_GUEST_ROUTER_DEFAULT_QUEUE_DEPTH;
    }
    if (router->config.max_payload_size == 0U ||
        router->config.max_payload_size > AR_GUEST_HARD_MAX_PAYLOAD_SIZE) {
        router->config.max_payload_size = AR_GUEST_HARD_MAX_PAYLOAD_SIZE;
    }

    router->entries = (ar_guest_route_entry_t *)calloc(
        router->config.max_guests, sizeof(ar_guest_route_entry_t));
    if (!router->entries) {
        free(router);
        return AR_GUEST_ERR_NOMEM;
    }

    *out_router = router;
    return AR_GUEST_OK;
}

void ar_guest_router_destroy(ar_guest_router_t *router) {
    size_t i;
    if (!router) {
        return;
    }
    if (router->entries != NULL) {
        for (i = 0U; i < router->config.max_guests; ++i) {
            if (router->entries[i].active) {
                if (router->entries[i].channel != NULL) {
                    ar_guest_channel_close(router->entries[i].channel);
                    router->entries[i].channel = NULL;
                }
                free_entry_queue(&router->entries[i]);
                router->entries[i].active = 0;
            }
        }
        free(router->entries);
        router->entries = NULL;
    }
    free(router);
}

int ar_guest_router_attach(
    ar_guest_router_t *router,
    const char *app_id,
    int host_fd,
    const ar_guest_identity_policy_t *policy) {
    ar_guest_channel_config_t chan_cfg;
    ar_guest_channel_t *channel = NULL;
    ar_guest_route_entry_t *entry = NULL;
    size_t i;
    int rc;

    if (!router || !app_id || host_fd < 0 || app_id[0] == '\0') {
        return AR_GUEST_ERR_INVALID;
    }
    if (strlen(app_id) >= AR_GUEST_APP_ID_MAX_LEN) {
        return AR_GUEST_ERR_INVALID;
    }
    if (find_entry(router, app_id) != NULL) {
        return AR_GUEST_ERR_EXISTS;
    }
    if (router->guest_count >= router->config.max_guests) {
        return AR_GUEST_ERR_LIMIT;
    }

    memset(&chan_cfg, 0, sizeof(chan_cfg));
    chan_cfg.max_payload_size = router->config.max_payload_size;
    chan_cfg.io_timeout_ms = 0;
    if (policy != NULL) {
        chan_cfg.peer_policy = *policy;
    }

    rc = ar_guest_channel_attach(host_fd, &chan_cfg, &channel);
    if (rc != AR_GUEST_OK) {
        return rc;
    }

    for (i = 0U; i < router->config.max_guests; ++i) {
        if (!router->entries[i].active) {
            entry = &router->entries[i];
            break;
        }
    }
    if (!entry) {
        ar_guest_channel_close(channel);
        return AR_GUEST_ERR_LIMIT;
    }

    memset(entry, 0, sizeof(*entry));
    entry->queue = (ar_guest_queue_entry_t *)calloc(
        router->config.max_queue_depth, sizeof(ar_guest_queue_entry_t));
    if (!entry->queue) {
        ar_guest_channel_close(channel);
        return AR_GUEST_ERR_NOMEM;
    }
    entry->queue_capacity = router->config.max_queue_depth;
    entry->queue_head = 0U;
    entry->queue_count = 0U;
    entry->channel = channel;
    entry->active = 1;
    entry->quarantined = 0;
    strncpy(entry->app_id, app_id, sizeof(entry->app_id) - 1U);
    if (policy != NULL) {
        entry->policy = *policy;
    }

    router->guest_count++;
    return AR_GUEST_OK;
}

int ar_guest_router_register(
    ar_guest_router_t *router,
    const char *app_id,
    const ar_guest_identity_policy_t *policy,
    int *out_guest_fd) {
    int sv[2];
    int rc;

    if (!router || !app_id || !out_guest_fd || app_id[0] == '\0') {
        return AR_GUEST_ERR_INVALID;
    }
    if (strlen(app_id) >= AR_GUEST_APP_ID_MAX_LEN) {
        return AR_GUEST_ERR_INVALID;
    }
    if (find_entry(router, app_id) != NULL) {
        return AR_GUEST_ERR_EXISTS;
    }
    if (router->guest_count >= router->config.max_guests) {
        return AR_GUEST_ERR_LIMIT;
    }

    rc = ar_guest_channel_socketpair(sv);
    if (rc != AR_GUEST_OK) {
        return rc;
    }

    rc = ar_guest_router_attach(router, app_id, sv[0], policy);
    if (rc != AR_GUEST_OK) {
        (void)close(sv[0]);
        (void)close(sv[1]);
        return rc;
    }

    *out_guest_fd = sv[1];
    return AR_GUEST_OK;
}

int ar_guest_router_unregister(
    ar_guest_router_t *router,
    const char *app_id) {
    ar_guest_route_entry_t *entry = find_entry(router, app_id);
    if (!entry) {
        return AR_GUEST_ERR_NOT_FOUND;
    }
    if (entry->channel != NULL) {
        ar_guest_channel_close(entry->channel);
        entry->channel = NULL;
    }
    free_entry_queue(entry);
    memset(entry, 0, sizeof(*entry));
    if (router->guest_count > 0U) {
        router->guest_count--;
    }
    return AR_GUEST_OK;
}

int ar_guest_router_has_guest(
    const ar_guest_router_t *router,
    const char *app_id) {
    size_t i;
    if (!router || !app_id) {
        return 0;
    }
    for (i = 0U; i < router->config.max_guests; ++i) {
        if (router->entries[i].active && !router->entries[i].quarantined &&
            strncmp(router->entries[i].app_id, app_id, AR_GUEST_APP_ID_MAX_LEN) == 0) {
            return 1;
        }
    }
    return 0;
}

int ar_guest_router_send(
    ar_guest_router_t *router,
    const char *app_id,
    uint16_t message_type,
    uint32_t flags,
    uint64_t request_id,
    const void *payload,
    uint32_t payload_length) {
    ar_guest_route_entry_t *entry;
    int rc;

    if (!router || !app_id) {
        return AR_GUEST_ERR_INVALID;
    }
    entry = find_entry(router, app_id);
    if (!entry) {
        return AR_GUEST_ERR_NOT_FOUND;
    }
    if (entry->quarantined) {
        return AR_GUEST_ERR_CLOSED;
    }

    if (entry->queue_count > 0U) {
        (void)flush_entry_queue(entry);
    }

    if (entry->queue_count == 0U) {
        rc = ar_guest_channel_send(
            entry->channel,
            message_type,
            flags,
            request_id,
            payload,
            payload_length);
        if (rc == AR_GUEST_OK) {
            entry->stats.messages_sent++;
            return AR_GUEST_OK;
        }
        if (rc == AR_GUEST_ERR_CLOSED) {
            entry->quarantined = 1;
            return AR_GUEST_ERR_CLOSED;
        }
        if (rc != AR_GUEST_ERR_WOULD_BLOCK) {
            return rc;
        }
    }

    if (entry->queue_count < entry->queue_capacity) {
        size_t tail = (entry->queue_head + entry->queue_count) % entry->queue_capacity;
        ar_guest_queue_entry_t *q = &entry->queue[tail];

        q->message_type = message_type;
        q->flags = flags;
        q->request_id = request_id;
        q->payload_length = payload_length;
        if (payload_length > 0U && payload != NULL) {
            q->payload = (uint8_t *)malloc(payload_length);
            if (!q->payload) {
                return AR_GUEST_ERR_NOMEM;
            }
            memcpy(q->payload, payload, payload_length);
        } else {
            q->payload = NULL;
        }

        entry->queue_count++;
        entry->stats.messages_sent++;
        return AR_GUEST_OK;
    }

    entry->stats.messages_dropped++;
    if (router->config.flood_policy == AR_GUEST_FLOOD_DISCONNECT) {
        entry->quarantined = 1;
        return AR_GUEST_ERR_CLOSED;
    }
    if (router->config.flood_policy == AR_GUEST_FLOOD_BACKPRESSURE) {
        return AR_GUEST_ERR_BACKPRESSURE;
    }
    return AR_GUEST_ERR_WOULD_BLOCK;
}

int ar_guest_router_broadcast(
    ar_guest_router_t *router,
    uint16_t message_type,
    uint32_t flags,
    uint64_t request_id,
    const void *payload,
    uint32_t payload_length) {
    size_t i;
    int any_sent = 0;

    if (!router) {
        return AR_GUEST_ERR_INVALID;
    }

    for (i = 0U; i < router->config.max_guests; ++i) {
        if (router->entries[i].active && !router->entries[i].quarantined) {
            int rc = ar_guest_router_send(
                router,
                router->entries[i].app_id,
                message_type,
                flags,
                request_id,
                payload,
                payload_length);
            if (rc == AR_GUEST_OK) {
                any_sent = 1;
            }
        }
    }
    return any_sent ? AR_GUEST_OK : AR_GUEST_ERR_NOT_FOUND;
}

int ar_guest_router_flush(
    ar_guest_router_t *router,
    const char *app_id) {
    ar_guest_route_entry_t *entry;
    if (!router || !app_id) {
        return AR_GUEST_ERR_INVALID;
    }
    entry = find_entry(router, app_id);
    if (!entry) {
        return AR_GUEST_ERR_NOT_FOUND;
    }
    return flush_entry_queue(entry);
}

int ar_guest_router_dispatch(
    ar_guest_router_t *router,
    int timeout_ms,
    ar_guest_message_handler_fn handler,
    void *user_data) {
    struct pollfd pfds[AR_GUEST_ROUTER_MAX_GUESTS];
    size_t pfd_entry_index[AR_GUEST_ROUTER_MAX_GUESTS];
    nfds_t nfds = 0U;
    size_t i;
    int prc;
    int processed = 0;

    if (!router) {
        return AR_GUEST_ERR_INVALID;
    }

    for (i = 0U; i < router->config.max_guests; ++i) {
        if (router->entries[i].active && !router->entries[i].quarantined &&
            router->entries[i].channel != NULL) {
            int fd = ar_guest_channel_fd(router->entries[i].channel);
            if (fd >= 0 && nfds < AR_GUEST_ROUTER_MAX_GUESTS) {
                pfds[nfds].fd = fd;
                pfds[nfds].events = POLLIN;
                if (router->entries[i].queue_count > 0U) {
                    pfds[nfds].events |= POLLOUT;
                }
                pfds[nfds].revents = 0;
                pfd_entry_index[nfds] = i;
                nfds++;
            }
        }
    }

    if (nfds == 0U) {
        return 0;
    }

    do {
        prc = poll(pfds, nfds, timeout_ms);
    } while (prc < 0 && errno == EINTR);

    if (prc <= 0) {
        return 0;
    }

    for (i = 0U; i < (size_t)nfds; ++i) {
        ar_guest_route_entry_t *entry = &router->entries[pfd_entry_index[i]];

        if ((pfds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0 &&
            (pfds[i].revents & POLLIN) == 0) {
            entry->quarantined = 1;
            continue;
        }

        if ((pfds[i].revents & POLLOUT) != 0 && entry->queue_count > 0U) {
            (void)flush_entry_queue(entry);
        }

        if ((pfds[i].revents & POLLIN) != 0) {
            size_t quota = 16U;
            while (quota > 0U) {
                ar_guest_frame_header_t hdr;
                uint8_t payload_buf[AR_GUEST_HARD_MAX_PAYLOAD_SIZE];
                int rc = ar_guest_channel_receive(
                    entry->channel,
                    &hdr,
                    payload_buf,
                    (uint32_t)sizeof(payload_buf));

                if (rc == AR_GUEST_OK) {
                    entry->stats.messages_received++;
                    processed++;
                    quota--;
                    if (handler != NULL) {
                        (void)handler(router, entry->app_id, &hdr, payload_buf, user_data);
                    }
                } else if (rc == AR_GUEST_ERR_WOULD_BLOCK) {
                    break;
                } else if (rc == AR_GUEST_ERR_CLOSED) {
                    entry->quarantined = 1;
                    break;
                } else if (rc == AR_GUEST_ERR_AUTH) {
                    entry->stats.auth_failures++;
                    break;
                } else if (rc == AR_GUEST_ERR_PROTOCOL || rc == AR_GUEST_ERR_LIMIT) {
                    entry->stats.protocol_errors++;
                    break;
                } else {
                    break;
                }
            }
        }
    }

    return processed;
}

int ar_guest_router_get_stats(
    const ar_guest_router_t *router,
    const char *app_id,
    ar_guest_route_stats_t *out_stats) {
    size_t i;
    if (!router || !app_id || !out_stats) {
        return AR_GUEST_ERR_INVALID;
    }
    for (i = 0U; i < router->config.max_guests; ++i) {
        if (router->entries[i].active &&
            strncmp(router->entries[i].app_id, app_id, AR_GUEST_APP_ID_MAX_LEN) == 0) {
            *out_stats = router->entries[i].stats;
            return AR_GUEST_OK;
        }
    }
    return AR_GUEST_ERR_NOT_FOUND;
}
