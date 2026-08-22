/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "router.h"
#include "aros_hal.h"
#include <string.h>
#ifndef _WIN32
#include <strings.h>
#endif
#include <stdlib.h>
#include <stdio.h>

#define ROUTE_HASH_SIZE 128

typedef struct Route {
    char path[256];
    char method[16];
    char domain[128];
    RequestHandler handler;
    struct Route *next;
} Route;

static Route *route_hash[ROUTE_HASH_SIZE] = {0};

static unsigned int hash_route(const char *path, const char *method) {
    unsigned int hash = 5381;
    int c;
    while ((c = *path++)) hash = ((hash << 5) + hash) + c;
    while ((c = *method++)) hash = ((hash << 5) + hash) + c;
    return hash % ROUTE_HASH_SIZE;
}

void add_route(const char *path, const char *method, const char *domain, RequestHandler handler) {
    Route *new_route = (Route *)ar_mem_alloc(sizeof(Route));
    strncpy(new_route->path, path, sizeof(new_route->path) - 1);
    strncpy(new_route->method, method, sizeof(new_route->method) - 1);
    new_route->domain[0] = '\0';
    if (domain && domain[0] != '\0') {
        strncpy(new_route->domain, domain, sizeof(new_route->domain) - 1);
    }
    new_route->handler = handler;
    unsigned int idx = hash_route(path, method);
    new_route->next = route_hash[idx];
    route_hash[idx] = new_route;
}

static int is_loopback_domain(const char *h) {
    if (!h || !h[0]) return 0;
    if (strcasecmp(h, "localhost") == 0) return 1;
    if (strcasecmp(h, "0.0.0.0") == 0) return 1;
    if (strncmp(h, "127.", 4) == 0) return 1;
    if (strcasecmp(h, "::1") == 0 || strcasecmp(h, "[::1]") == 0) return 1;
    return 0;
}

static int domain_matches(const char *route_domain, const char *req_host) {
    if (route_domain[0] == '\0' || strcmp(route_domain, "*") == 0) return 1;
    if (req_host[0] == '\0') return 0;
    if (strcasecmp(route_domain, req_host) == 0) return 1;
    if (is_loopback_domain(route_domain) && is_loopback_domain(req_host)) return 1;
    if (strncasecmp(req_host, "www.", 4) == 0 && strcasecmp(route_domain, req_host + 4) == 0) return 1;
    if (strncasecmp(route_domain, "www.", 4) == 0 && strcasecmp(route_domain + 4, req_host) == 0) return 1;
    return 0;
}

void router_dispatch(ClientConnection *conn, HttpRequest *req) {
    unsigned int idx = hash_route(req->path, req->method);
    Route *current = route_hash[idx];
    while (current != NULL) {
        if (strcmp(current->path, req->path) == 0 && strcmp(current->method, req->method) == 0) {
            if (domain_matches(current->domain, req->host)) {
                current->handler(conn, req);
                return;
            }
        }
        current = current->next;
    }
    Route *catch_all = NULL;
    for (int i = 0; i < ROUTE_HASH_SIZE; i++) {
        current = route_hash[i];
        while (current != NULL) {
            int len = strlen(current->path);
            if (len > 0 && current->path[len - 1] == '*') {
                if (strncmp(current->path, req->path, len - 1) == 0 && strcmp(current->method, req->method) == 0) {
                    if (current->domain[0] == '\0') {
                        if (!catch_all) catch_all = current;
                    } else if (domain_matches(current->domain, req->host)) {
                        current->handler(conn, req);
                        return;
                    }
                }
            }
            current = current->next;
        }
    }
    if (catch_all) {
        catch_all->handler(conn, req);
        return;
    }
    server_send_404(conn);
}
