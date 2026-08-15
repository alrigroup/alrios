/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arws_gateway.h"
#include "modes.h"
#include "log.h"
#include "aros_hal.h"
#include <string.h>
#ifndef _WIN32
#include <strings.h>
#endif
#include <stdio.h>

#define ROUTE_HASH_SIZE 256

typedef struct RouteNode {
    ArwsRoute route;
    struct RouteNode *next;
} RouteNode;

static RouteNode *route_table[ROUTE_HASH_SIZE] = {0};
static void *route_mutex = NULL;

static int rr_counter = 0;

static unsigned int hash_route(const char *prefix, const char *method,
                               const char *host, const char *mode) {
    unsigned int hash = 5381;
    int c;
    if (prefix) while ((c = *prefix++)) hash = ((hash << 5) + hash) + c;
    if (method) while ((c = *method++)) hash = ((hash << 5) + hash) + c;
    if (host)   while ((c = *host++))   hash = ((hash << 5) + hash) + c;
    if (mode)   while ((c = *mode++))   hash = ((hash << 5) + hash) + c;
    return hash % ROUTE_HASH_SIZE;
}

void arws_route_init(void) {
    route_mutex = ar_mutex_create();
    ar_mutex_lock(route_mutex);
    memset(route_table, 0, sizeof(route_table));
    ar_mutex_unlock(route_mutex);
}

int arws_add_redirect_route(const char *prefix, const char *method,
                            const char *host, const char *mode,
                            const char *target_url) {
    if (!prefix || !method || !target_url) return -1;

    RouteNode *node = (RouteNode *)ar_mem_alloc(sizeof(RouteNode));
    if (!node) return -1;

    char normalized[256];
    if (strcmp(prefix, "*") == 0) {
        prefix = "/*";
    } else if (prefix[0] != '/' && prefix[0] != '*') {
        snprintf(normalized, sizeof(normalized), "/%s", prefix);
        prefix = normalized;
    }
    strncpy(node->route.prefix, prefix, sizeof(node->route.prefix) - 1);
    strncpy(node->route.method, method, sizeof(node->route.method) - 1);
    node->route.host[0] = '\0';
    node->route.mode[0] = '\0';
    if (host) strncpy(node->route.host, host, sizeof(node->route.host) - 1);
    if (mode) strncpy(node->route.mode, mode, sizeof(node->route.mode) - 1);
    node->route.backend_id = -1;
    node->route.use_handler = 0;
    node->route.use_stream = 0;
    node->route.use_redirect = 1;
    node->route.handler = NULL;
    strncpy(node->route.redirect_target, target_url, sizeof(node->route.redirect_target) - 1);

    unsigned int idx = hash_route(prefix, method, host, mode);

    ar_mutex_lock(route_mutex);
    node->next = route_table[idx];
    route_table[idx] = node;
    ar_mutex_unlock(route_mutex);

    alri_print(CYN "[ARWS]" RST " Redirect route: %s %s host='%s' -> %s\n",
               method, prefix, host ? host : "*", target_url);
    return 0;
}

int arws_add_route(const char *prefix, const char *method,
                   const char *host, const char *mode,
                   int backend_id) {
    if (!prefix || !method) return -1;

    RouteNode *node = (RouteNode *)ar_mem_alloc(sizeof(RouteNode));
    if (!node) return -1;

    char normalized[256];
    if (strcmp(prefix, "*") == 0) {
        prefix = "/*";
    } else if (prefix[0] != '/' && prefix[0] != '*') {
        snprintf(normalized, sizeof(normalized), "/%s", prefix);
        prefix = normalized;
    }
    strncpy(node->route.prefix, prefix, sizeof(node->route.prefix) - 1);
    strncpy(node->route.method, method, sizeof(node->route.method) - 1);
    node->route.host[0] = '\0';
    node->route.mode[0] = '\0';
    if (host) strncpy(node->route.host, host, sizeof(node->route.host) - 1);
    if (mode) strncpy(node->route.mode, mode, sizeof(node->route.mode) - 1);
    node->route.backend_id = backend_id;
    node->route.use_handler = 0;
    node->route.use_stream = 0;
    node->route.use_redirect = 0;
    node->route.handler = NULL;

    unsigned int idx = hash_route(prefix, method, host, mode);

    ar_mutex_lock(route_mutex);
    node->next = route_table[idx];
    route_table[idx] = node;
    ar_mutex_unlock(route_mutex);

    return 0;
}

int arws_add_proxy_route(const char *prefix, const char *method,
                         const char *host, const char *mode,
                         const char *target_url) {
    if (!prefix || !method || !target_url) return -1;

    RouteNode *node = (RouteNode *)ar_mem_alloc(sizeof(RouteNode));
    if (!node) return -1;

    char normalized[256];
    if (strcmp(prefix, "*") == 0) {
        prefix = "/*";
    } else if (prefix[0] != '/' && prefix[0] != '*') {
        snprintf(normalized, sizeof(normalized), "/%s", prefix);
        prefix = normalized;
    }
    strncpy(node->route.prefix, prefix, sizeof(node->route.prefix) - 1);
    strncpy(node->route.method, method, sizeof(node->route.method) - 1);
    node->route.host[0] = '\0';
    node->route.mode[0] = '\0';
    if (host) strncpy(node->route.host, host, sizeof(node->route.host) - 1);
    if (mode) strncpy(node->route.mode, mode, sizeof(node->route.mode) - 1);
    node->route.backend_id = -1;
    node->route.use_handler = 0;
    node->route.use_stream = 0;
    node->route.use_redirect = 0;
    node->route.handler = NULL;
    strncpy(node->route.proxy_target, target_url, sizeof(node->route.proxy_target) - 1);

    unsigned int idx = hash_route(prefix, method, host, mode);

    ar_mutex_lock(route_mutex);
    node->next = route_table[idx];
    route_table[idx] = node;
    ar_mutex_unlock(route_mutex);

    alri_print(CYN "[ARWS]" RST " Proxy route: %s %s host='%s' -> %s\n",
               method, prefix, host ? host : "*", target_url);
    return 0;
}

int arws_add_stream_route(const char *prefix, const char *method,
                          const char *host, const char *mode,
                          const char *target_url) {
    if (!prefix || !method || !target_url) return -1;

    RouteNode *node = (RouteNode *)ar_mem_alloc(sizeof(RouteNode));
    if (!node) return -1;

    char normalized[256];
    if (strcmp(prefix, "*") == 0) {
        prefix = "/*";
    } else if (prefix[0] != '/' && prefix[0] != '*') {
        snprintf(normalized, sizeof(normalized), "/%s", prefix);
        prefix = normalized;
    }
    strncpy(node->route.prefix, prefix, sizeof(node->route.prefix) - 1);
    strncpy(node->route.method, method, sizeof(node->route.method) - 1);
    node->route.host[0] = '\0';
    node->route.mode[0] = '\0';
    if (host) strncpy(node->route.host, host, sizeof(node->route.host) - 1);
    if (mode) strncpy(node->route.mode, mode, sizeof(node->route.mode) - 1);
    node->route.backend_id = -1;
    node->route.use_handler = 0;
    node->route.use_stream = 1;
    node->route.use_redirect = 0;
    node->route.handler = NULL;
    strncpy(node->route.proxy_target, target_url, sizeof(node->route.proxy_target) - 1);

    unsigned int idx = hash_route(prefix, method, host, mode);

    ar_mutex_lock(route_mutex);
    node->next = route_table[idx];
    route_table[idx] = node;
    ar_mutex_unlock(route_mutex);

    alri_print(CYN "[ARWS]" RST " Stream route: %s %s host='%s' -> %s\n",
               method, prefix, host ? host : "*", target_url);
    return 0;
}

int arws_add_handler(const char *prefix, const char *method,
                     const char *host, const char *mode,
                     RequestHandler handler) {
    if (!prefix || !method || !handler) return -1;

    RouteNode *node = (RouteNode *)ar_mem_alloc(sizeof(RouteNode));
    if (!node) return -1;

    strncpy(node->route.prefix, prefix, sizeof(node->route.prefix) - 1);
    strncpy(node->route.method, method, sizeof(node->route.method) - 1);
    node->route.host[0] = '\0';
    node->route.mode[0] = '\0';
    if (host) strncpy(node->route.host, host, sizeof(node->route.host) - 1);
    if (mode) strncpy(node->route.mode, mode, sizeof(node->route.mode) - 1);
    node->route.backend_id = -1;
    node->route.use_handler = 1;
    node->route.use_stream = 0;
    node->route.use_redirect = 0;
    node->route.handler = handler;

    unsigned int idx = hash_route(prefix, method, host, mode);

    ar_mutex_lock(route_mutex);
    node->next = route_table[idx];
    route_table[idx] = node;
    ar_mutex_unlock(route_mutex);

    return 0;
}

int arws_remove_route(const char *prefix, const char *method,
                      const char *host) {
    if (!prefix || !method) return -1;
    unsigned int idx = hash_route(prefix, method, host, NULL);

    ar_mutex_lock(route_mutex);
    RouteNode *current = route_table[idx];
    RouteNode *prev = NULL;

    while (current) {
        if (strcmp(current->route.prefix, prefix) == 0 &&
            strcmp(current->route.method, method) == 0 &&
            (host == NULL || current->route.host[0] == '\0' ||
             strcmp(current->route.host, host) == 0)) {
            if (prev) prev->next = current->next;
            else route_table[idx] = current->next;
            ar_mem_free(current);
            ar_mutex_unlock(route_mutex);
            return 0;
        }
        prev = current;
        current = current->next;
    }
    ar_mutex_unlock(route_mutex);
    return -1;
}

int arws_remove_routes_by_backend(int backend_id) {
    ar_mutex_lock(route_mutex);
    int removed = 0;
    for (int i = 0; i < ROUTE_HASH_SIZE; i++) {
        RouteNode *current = route_table[i];
        RouteNode *prev = NULL;
        while (current) {
            if (current->route.backend_id == backend_id) {
                RouteNode *to_free = current;
                if (prev) prev->next = current->next;
                else route_table[i] = current->next;
                current = current->next;
                ar_mem_free(to_free);
                removed++;
            } else {
                prev = current;
                current = current->next;
            }
        }
    }
    ar_mutex_unlock(route_mutex);
    return removed;
}

static int host_matches(const char *route_host, const char *req_host) {
    if (!req_host || req_host[0] == '\0') {
        return (route_host[0] == '\0' || strcmp(route_host, "*") == 0);
    }
    if (route_host[0] == '\0' || strcmp(route_host, "*") == 0) return 1;
    if (strcmp(route_host, req_host) == 0) return 1;
    if (strncasecmp(req_host, "www.", 4) == 0 &&
        strcmp(route_host, req_host + 4) == 0) return 1;
    return 0;
}

static int path_matches(const char *prefix, const char *req_path) {
    if (!prefix || !req_path) return 0;
    int prefix_len = (int)strlen(prefix);
    if (prefix_len == 0) return 1;
    if (prefix[prefix_len - 1] == '*') {
        return strncmp(prefix, req_path, prefix_len - 1) == 0;
    }
    return strcmp(prefix, req_path) == 0;
}

static int route_priority(const ArwsRoute *route, const char *path) {
    int plen = (int)strlen(route->prefix);
    int is_wildcard = (plen > 0 && route->prefix[plen-1] == '*');
    int effective = is_wildcard ? plen - 1 : plen;
    int path_len = (int)strlen(path);
    int is_exact = (effective == path_len && strncmp(route->prefix, path, effective) == 0);

    int prio = is_exact ? (is_wildcard ? (effective + 1000) : (effective + 2000)) : effective;

    if (route->host[0] != '\0' && strcmp(route->host, "*") != 0) {
        prio += 10000;
    }
    return prio;
}

int arws_route_match(ClientConnection *conn, HttpRequest *req,
                     const char *effective_mode, ArwsRoute *out_route) {
    (void)conn;
    if (!req) return 0;

    RouteNode *candidates[ARWS_MAX_BACKENDS];
    int candidate_count = 0;
    int best_priority = -1;

    ar_mutex_lock(route_mutex);
    for (int i = 0; i < ROUTE_HASH_SIZE; i++) {
        RouteNode *current = route_table[i];
        while (current) {
            if (strcmp(current->route.method, req->method) == 0 ||
                strcmp(current->route.method, "*") == 0) {

                if (host_matches(current->route.host, req->host)) {
                    if (path_matches(current->route.prefix, req->path)) {

                        int mode_match = 0;
                        if (current->route.mode[0] == '\0' ||
                            strcmp(current->route.mode, effective_mode) == 0 ||
                            strcmp(current->route.mode, "*") == 0) {
                            mode_match = 1;
                        }

                        if (mode_match) {
                            int prio = route_priority(&current->route, req->path);
                            if (prio > best_priority) {
                                best_priority = prio;
                                candidate_count = 0;
                                candidates[candidate_count++] = current;
                            } else if (prio == best_priority && candidate_count < ARWS_MAX_BACKENDS) {
                                candidates[candidate_count++] = current;
                            }
                        }
                    }
                }
            }
            current = current->next;
        }
    }

    RouteNode *chosen = NULL;
    if (candidate_count > 0) {
        int idx = rr_counter % candidate_count;
        rr_counter++;
        chosen = candidates[idx];
    }
    ar_mutex_unlock(route_mutex);

    if (chosen) {
        if (out_route)
            *out_route = chosen->route;
        return chosen->route.use_handler ? 2 : 1;
    }

    return 0;
}
