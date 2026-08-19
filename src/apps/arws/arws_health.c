/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arws_health.h"
#include "log.h"
#include "aros_hal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

static volatile int g_health_running = 0;
static void *g_health_thread = NULL;

/* Sonda rápida de conexão TCP com timeout */
static int probe_tcp_node(const char *host, int port, int timeout_ms) {
    if (!host || port <= 0) return 0;

    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(host, port_str, &hints, &res) != 0 || !res) {
        return 0;
    }

    int sock = (int)socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        freeaddrinfo(res);
        return 0;
    }

#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif

    int ret = connect(sock, res->ai_addr, (int)res->ai_addrlen);
    int alive = 0;

    if (ret == 0) {
        alive = 1;
    } else {
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(sock, &wfds);

        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        int sel = select(sock + 1, NULL, &wfds, NULL, &tv);
        if (sel > 0) {
            int err = 0;
            socklen_t len = sizeof(err);
            if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err, &len) == 0 && err == 0) {
                alive = 1;
            }
        }
    }

#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
    freeaddrinfo(res);
    return alive;
}

static void* health_check_worker(void *arg) {
    (void)arg;
    alri_print(CYN "[ARWS-HEALTH]" RST " Active health check prober started.\n");

    ArwsUpstreamPool pools[ARWS_MAX_POOLS];

    while (g_health_running) {
        int pool_count = arws_upstream_get_all_pools(pools, ARWS_MAX_POOLS);

        for (int i = 0; i < pool_count; i++) {
            ArwsUpstreamPool *p = &pools[i];
            ArwsUpstreamPool *live_pool = arws_upstream_get_pool(p->name);
            if (!live_pool) continue;

            for (int n = 0; n < p->node_count; n++) {
                if (!g_health_running) break;

                ArwsBackendNode *node = &p->nodes[n];
                if (node->is_draining) continue; /* Não sondar nós em drenagem voluntária */

                int alive = probe_tcp_node(node->host, node->port, p->check_timeout_ms);

                /* Atualizar estado com thread-safety */
                ar_mutex_lock(live_pool->mutex);
                for (int ln = 0; ln < live_pool->node_count; ln++) {
                    if (strcmp(live_pool->nodes[ln].host, node->host) == 0 &&
                        live_pool->nodes[ln].port == node->port) {
                        
                        ArwsBackendNode *tgt = &live_pool->nodes[ln];
                        tgt->last_check_ms = (uint64_t)ar_time_ms();

                        if (alive) {
                            tgt->pass_count++;
                            tgt->fail_count = 0;
                            if (tgt->pass_count >= live_pool->rise_threshold && !tgt->is_alive) {
                                tgt->is_alive = 1;
                                tgt->effective_weight = tgt->weight;
                                alri_print(GRN "[ARWS-HEALTH]" RST " Node %s:%d recovered -> UP\n", tgt->host, tgt->port);
                            }
                        } else {
                            tgt->fail_count++;
                            tgt->pass_count = 0;
                            if (tgt->fail_count >= live_pool->fall_threshold && tgt->is_alive) {
                                tgt->is_alive = 0;
                                alri_print(RED "[ARWS-HEALTH]" RST " Node %s:%d failed active probe -> DOWN\n", tgt->host, tgt->port);
                            }
                        }
                        break;
                    }
                }
                ar_mutex_unlock(live_pool->mutex);
            }
        }

        /* Dormir com checagens frequentes de flag para parada rápida */
        for (int s = 0; s < 50 && g_health_running; s++) {
            ar_sleep_ms(100);
        }
    }

    alri_print(CYN "[ARWS-HEALTH]" RST " Active health check prober stopped.\n");
    return NULL;
}

int arws_health_start(void) {
    if (g_health_running) return 0;
    g_health_running = 1;
    g_health_thread = ar_thread_create(health_check_worker, NULL);
    return g_health_thread != NULL ? 0 : -1;
}

void arws_health_stop(void) {
    if (!g_health_running) return;
    g_health_running = 0;
    if (g_health_thread) {
        ar_thread_join(g_health_thread);
        g_health_thread = NULL;
    }
}
