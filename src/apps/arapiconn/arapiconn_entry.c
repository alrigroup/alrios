/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arapiconn_config.h"
#include "arapiconn_db.h"
#include "arapiconn_http.h"
#include "aros_hal.h"
#include "ar_ipc.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

static volatile int g_app_running = 1;
static int g_ipc_fd = -1;

static void handle_sig(int s) {
    (void)s;
    g_app_running = 0;
}

static void handle_ipc_query(int fd, const char *payload, int payload_len) {
    char q[512] = {0};
    int qlen = payload_len < (int)sizeof(q) - 1 ? payload_len : (int)sizeof(q) - 1;
    memcpy(q, payload, qlen);
    q[qlen] = '\0';

    ArapiconnConfig *cfg = arapiconn_config_get();
    char resp[AR_IPC_BUF_SIZE];
    int rlen = 0;

    if (strcmp(q, "status") == 0) {
        rlen = snprintf(resp, sizeof(resp),
            "[ARAPICONN] Communication & Kanban API Backend Status:\n"
            "  State: RUNNING\n"
            "  HTTP Port: %d (Active)\n"
            "  Route Prefix: %s\n"
            "  Storage: %s\n"
            "  Privacy Shield: ACTIVE (Zero-DM Telemetry Filter)\n",
            cfg->server_port, cfg->route_prefix, cfg->data_dir);
    } else {
        rlen = snprintf(resp, sizeof(resp),
            "ARAPICONN — Sovereign Communication & Kanban API Backend v1.0.0\n"
            "  status - View backend status\n");
    }

    ar_ipc_send_frame(fd, IPC_QUERY_RESP, resp, (uint32_t)rlen + 1);
}

static void *arapiconn_ipc_thread(void *arg) {
    (void)arg;
    ArapiconnConfig *cfg = arapiconn_config_get();

    while (g_app_running) {
        g_ipc_fd = ar_ipc_client_connect("127.0.0.1", 9500);
        if (g_ipc_fd < 0) {
            ar_sleep_ms(1000);
            continue;
        }

        /* Auto-register proxy route in ARWS */
        char reg_frame[512];
        snprintf(reg_frame, sizeof(reg_frame),
                 "arapiconn %s/* * * production proxy=http://127.0.0.1:%d",
                 cfg->route_prefix, cfg->server_port);
        ar_ipc_send_frame(g_ipc_fd, IPC_REGISTER, reg_frame, (uint32_t)strlen(reg_frame) + 1);

        char reg_frame_exact[512];
        snprintf(reg_frame_exact, sizeof(reg_frame_exact),
                 "arapiconn %s * * production proxy=http://127.0.0.1:%d",
                 cfg->route_prefix, cfg->server_port);
        ar_ipc_send_frame(g_ipc_fd, IPC_REGISTER, reg_frame_exact, (uint32_t)strlen(reg_frame_exact) + 1);

        char buf[AR_IPC_BUF_SIZE];
        while (g_app_running) {
            int type = 0;
            uint32_t len = sizeof(buf);
            if (ar_ipc_recv_frame(g_ipc_fd, &type, buf, &len) < 0) {
                break;
            }

            if (type == IPC_QUERY) {
                handle_ipc_query(g_ipc_fd, buf, (int)len);
            } else if (type == IPC_HEARTBEAT) {
                ar_ipc_send_frame(g_ipc_fd, IPC_ACK, "ACK", 4);
            }
        }

        ar_socket_close(g_ipc_fd);
        g_ipc_fd = -1;
        ar_sleep_ms(1000);
    }
    return NULL;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    signal(SIGINT, handle_sig);
    signal(SIGTERM, handle_sig);

    alri_print(BLD GRN "== [ARAPICONN] Starting Sovereign Communication Backend ==\n" RST);

    arapiconn_config_load("arapiconn.cfg");
    ArapiconnConfig *cfg = arapiconn_config_get();

    if (arapiconn_db_init(cfg->data_dir) != 0) {
        alri_print(RED "[ARAPICONN] Failed to initialize communication database\n" RST);
        return 1;
    }

    if (arapiconn_http_server_start(cfg->server_bind, cfg->server_port) != 0) {
        alri_print(RED "[ARAPICONN] Failed to start HTTP server on port %d\n" RST, cfg->server_port);
        arapiconn_db_close();
        return 1;
    }

    void *ipc_t = ar_thread_create(arapiconn_ipc_thread, NULL);

    while (g_app_running) {
        ar_sleep_ms(250);
    }

    alri_print(YLW "[ARAPICONN] Shutting down service...\n" RST);
    arapiconn_http_server_stop();
    arapiconn_db_close();

    if (ipc_t) ar_thread_join(ipc_t);

    alri_print(GRN "[ARAPICONN] Clean shutdown completed.\n" RST);
    return 0;
}
