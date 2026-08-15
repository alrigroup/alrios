/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arwn_gateway.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arwn_config.h"
#include "ar_ipc.h"
#include "aros_hal.h"

#define ARWN_GATEWAY_DEFAULT_PORT 9500
#define ARWN_GATEWAY_MAX_ATTEMPTS 30
#define ARWN_GATEWAY_RECV_TIMEOUT_MS 800
#define ARWN_GATEWAY_IDLE_ROUNDS 60
#define ARWN_GATEWAY_SLEEP_MS 1000

typedef struct {
    char host[64];
    uint16_t port;
    char route_host[128];
    char route_path[128];
    char route_mode[16];
    char server_bind[64];
    uint16_t server_port;
} arwn_gw_cfg_t;

/* lê [arws] + [app] do config */
static void gateway_read_cfg(arwn_app_t *app, arwn_gw_cfg_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));

    const char *gw = arwn_config_get(app, "arws", "gateway", "127.0.0.1:9500");
    /* divide host:port */
    const char *colon = strrchr(gw, ':');
    if (colon) {
        size_t hl = (size_t)(colon - gw);
        if (hl > 0 && hl < sizeof(cfg->host)) {
            memcpy(cfg->host, gw, hl);
            cfg->host[hl] = '\0';
        }
        int p = atoi(colon + 1);
        cfg->port = (uint16_t)(p > 0 ? p : ARWN_GATEWAY_DEFAULT_PORT);
    } else {
        snprintf(cfg->host, sizeof(cfg->host), "%s", gw);
        cfg->port = ARWN_GATEWAY_DEFAULT_PORT;
    }

    snprintf(cfg->route_host, sizeof(cfg->route_host), "%s",
             arwn_config_get(app, "arws", "route.host", "localhost"));
    snprintf(cfg->route_path, sizeof(cfg->route_path), "%s",
             arwn_config_get(app, "arws", "route.path", "/*"));
    snprintf(cfg->route_mode, sizeof(cfg->route_mode), "%s",
             arwn_config_get(app, "arws", "route.mode", "production"));
    snprintf(cfg->server_bind, sizeof(cfg->server_bind), "%s",
             arwn_config_get(app, "app", "bind", "127.0.0.1"));
    cfg->server_port = (uint16_t)arwn_config_get_int(app, "app", "port", 3001);
}

static int gw_register(int fd, const char *name, const arwn_gw_cfg_t *cfg) {
    char payload[512];
    int len = snprintf(payload, sizeof(payload),
                       "%s %s %s %s %s proxy=http://%s:%u type=proxy",
                       name, cfg->route_path, "GET", cfg->route_host,
                       cfg->route_mode, cfg->server_bind, cfg->server_port);
    if (len <= 0 || len >= (int)sizeof(payload)) return -1;

    if (ar_ipc_send_frame(fd, IPC_REGISTER, payload, (uint32_t)len + 1) != 0)
        return -1;

    unsigned char type = 0;
    char ack[128];
    uint32_t alen = sizeof(ack);
    if (ar_ipc_recv_frame(fd, (int *)&type, ack, &alen) != 0) return -1;
    if (type != IPC_ACK) return -1;

    printf("[arwn] registered GET %s host=%s -> proxy://%s:%u (%s)\n",
           cfg->route_path, cfg->route_host, cfg->server_bind,
           cfg->server_port, ack);
    return 0;
}

static void build_routes_text(arwn_server_t *server, char *out, int size) {
    int used = 0;
    int n = arwn_server_route_count(server);
    for (int i = 0; i < n; i++) {
        const arwn_route_t *r = arwn_server_route(server, i);
        int k = snprintf(out + used, (size_t)(size - used),
                         "%-16s %s (%zu bytes)\n",
                         r->path, r->content_type, r->size);
        if (k < 0 || used + k >= size) break;
        used += k;
    }
    out[used] = '\0';
}

static void handle_query(int fd, arwn_server_t *server, const char *q, int len) {
    char cmd[128] = {0};
    int i = 0;
    while (i < len && i < 127 && q[i] != ' ' && q[i] != '\t' && q[i] != '\n') {
        cmd[i] = q[i];
        i++;
    }

    char resp[4096];
    int rlen = 0;
    if (strcmp(cmd, "ping") == 0) {
        rlen = snprintf(resp, sizeof(resp), "pong");
    } else if (strcmp(cmd, "status") == 0) {
        rlen = snprintf(resp, sizeof(resp), "arwn RUNNING routes=%d",
                        arwn_server_route_count(server));
    } else if (strcmp(cmd, "routes") == 0) {
        rlen = snprintf(resp, sizeof(resp), "%s",
                        "arwn routes: (list below)\n");
        int used = rlen;
        char list[2048];
        build_routes_text(server, list, sizeof(list));
        int k = snprintf(resp + used, sizeof(resp) - (size_t)used, "%s", list);
        if (k > 0) rlen += k;
        if (rlen >= (int)sizeof(resp)) rlen = (int)sizeof(resp) - 1;
    } else {
        rlen = snprintf(resp, sizeof(resp), "unknown command: %s", cmd);
    }
    if (rlen < 0) rlen = 0;
    if (rlen >= (int)sizeof(resp)) rlen = (int)sizeof(resp) - 1;

    ar_ipc_send_frame(fd, IPC_QUERY_RESP, resp, (uint32_t)rlen + 1);
}

/* loop de controle: heartbeat quando ocioso, responde queries */
static void gw_control_loop(int fd, arwn_server_t *server) {
    ar_socket_set_recv_timeout(fd, ARWN_GATEWAY_RECV_TIMEOUT_MS);

    char buf[AR_IPC_BUF_SIZE];
    int idle = 0;

    while (1) {
        unsigned char type = 0;
        uint32_t len = sizeof(buf);
        int r = ar_ipc_recv_frame(fd, (int *)&type, buf, &len);
        if (r == 0) {
            idle = 0;
            if (type == IPC_QUERY) {
                handle_query(fd, server, buf, (int)strlen(buf));
            }
            continue;
        }

        idle++;
        if (idle > ARWN_GATEWAY_IDLE_ROUNDS) break;

        if (ar_ipc_send_frame(fd, IPC_HEARTBEAT, NULL, 0) < 0) break;
    }

    ar_socket_close(fd);
    printf("[arwn] control channel closed\n");
}

/* ------------------------------------------------------------------ */

static void *gateway_thread(void *arg) {
    arwn_app_t *app = (arwn_app_t *)arg;

    arwn_gw_cfg_t cfg;
    gateway_read_cfg(app, &cfg);

    char name[64];
    snprintf(name, sizeof(name), "%s", arwn_app_name(app));

    printf("[arwn] gateway %s:%u (route %s host=%s)\n",
           cfg.host, cfg.port, cfg.route_path, cfg.route_host);

    arwn_server_t *server = arwn_server_for_gateway();

    /* registro/controle com reconexão e backoff (padrão home_server) */
    for (int attempt = 1; attempt <= ARWN_GATEWAY_MAX_ATTEMPTS; attempt++) {
        int fd = ar_socket_create(1);
        if (fd >= 0 && ar_socket_connect(fd, cfg.host, cfg.port) == 0) {
            if (gw_register(fd, name, &cfg) == 0) {
                printf("[arwn] routes registered + control channel open (attempt %d)\n",
                       attempt);
                gw_control_loop(fd, server);
                printf("[arwn] control channel lost, reconnecting...\n");
                ar_sleep_ms(ARWN_GATEWAY_SLEEP_MS);
                continue;
            }
        }
        if (fd >= 0) ar_socket_close(fd);
        printf("[arwn] register retry %d/%d\n", attempt, ARWN_GATEWAY_MAX_ATTEMPTS);
        ar_sleep_ms(ARWN_GATEWAY_SLEEP_MS);
    }

    fprintf(stderr, "[arwn] failed to register routes after %d attempts\n",
            ARWN_GATEWAY_MAX_ATTEMPTS);
    return NULL;
}

/* registro do server para o thread do gateway (o app pode não ter campo
   para ele; mantemos um ponteiro global por processo). */
static arwn_server_t *g_server = NULL;

arwn_server_t *arwn_server_for_gateway(void) {
    return g_server;
}

int arwn_gateway_start(arwn_app_t *app, arwn_server_t *server) {
    if (!app || !server) return -1;
    g_server = server;

    void *th = ar_thread_create(gateway_thread, app);
    if (!th) return -1;
    ar_thread_detach(th);
    return 0;
}