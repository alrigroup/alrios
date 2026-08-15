/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "ctl.h"
#include "loader.h"
#include "ar_ipc.h"
#include "ar_kernel.h"
#include "aros_hal.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

static int ctl_server_fd = -1;
static volatile int ctl_running = 0;

static void send_result(int fd, int ok, const char *msg) {
    ar_ipc_send_frame(fd, ok ? IPC_RESPONSE : IPC_ERROR, msg, (uint32_t)strlen(msg) + 1);
}

static void handle_frame(int fd, int type, const unsigned char *payload, uint32_t len) {
    char name[128];
    name[0] = '\0';
    if (payload && len > 0) {
        uint32_t n = len;
        if (n >= sizeof(name)) n = sizeof(name) - 1;
        memcpy(name, payload, n);
        name[n] = '\0';
    }

    switch (type) {
        case IPC_CTL_PING:
            send_result(fd, 1, "pong");
            break;

        case IPC_CTL_LIST: {
            loader_reap_apps();
            char list[AR_IPC_BUF_SIZE];
            list[0] = '\0';
            int n = loader_list_apps(list, sizeof(list));
            if (n <= 0) n = 1;
            ar_ipc_send_frame(fd, IPC_RESPONSE, list, (uint32_t)n);
            break;
        }

        case IPC_CTL_START:
            if (!name[0]) { send_result(fd, 0, "missing app name"); break; }
            if (loader_start_app(name) == 0) send_result(fd, 1, "ok");
            else send_result(fd, 0, "start failed");
            break;

        case IPC_CTL_STOP:
            if (!name[0]) { send_result(fd, 0, "missing app name"); break; }
            if (loader_stop_app(name) == 0) send_result(fd, 1, "ok");
            else send_result(fd, 0, "stop failed");
            break;

        case IPC_CTL_RESTART:
            if (!name[0]) { send_result(fd, 0, "missing app name"); break; }
            if (loader_restart_app(name) == 0) send_result(fd, 1, "ok");
            else send_result(fd, 0, "restart failed");
            break;

        case IPC_CTL_STATUS: {
            if (!name[0]) { send_result(fd, 0, "missing app name"); break; }
            char st[64];
            if (loader_status_app(name, st, sizeof(st)) == 0)
                ar_ipc_send_frame(fd, IPC_RESPONSE, st, (uint32_t)strlen(st) + 1);
            else
                send_result(fd, 0, "not found");
            break;
        }

        case IPC_CTL_POWER_RELOAD:
            loader_power_reload();
            send_result(fd, 1, "ok");
            break;

        case IPC_CTL_REFRESH:
            loader_refresh();
            send_result(fd, 1, "ok");
            break;

        case IPC_CTL_POWER_OFF:
            send_result(fd, 1, "bye");
            ar_sleep_ms(100);
            ar_shutdown();
            exit(0);
            break;

        default:
            send_result(fd, 0, "unknown");
            break;
    }
}

static void *client_handler_loop(void *arg) {
    int client_fd = (int)(intptr_t)arg;
    unsigned char buf[AR_IPC_BUF_SIZE];
    int type;

    while (ctl_running) {
        uint32_t len = sizeof(buf);
        if (ar_ipc_recv_frame(client_fd, &type, buf, &len) < 0)
            break;
        handle_frame(client_fd, type, buf, len);
    }

    ar_socket_close(client_fd);
    return NULL;
}

static void *accept_loop(void *arg) {
    (void)arg;
    while (ctl_running) {
        int client_fd = ar_socket_accept(ctl_server_fd);
        if (client_fd < 0) {
            if (!ctl_running) break;
            ar_sleep_ms(50);
            continue;
        }
        void *th = ar_thread_create(client_handler_loop, (void *)(intptr_t)client_fd);
        if (th) ar_thread_detach(th);
    }
    return NULL;
}

int ctl_start(void) {
    ctl_server_fd = ar_ipc_server_start(AR_CTL_PORT);
    if (ctl_server_fd < 0) {
        alri_printf("  " RED "x" RST " Control channel bind 127.0.0.1:%d failed\n", AR_CTL_PORT);
        return -1;
    }
    ctl_running = 1;
    void *th = ar_thread_create(accept_loop, NULL);
    if (th) ar_thread_detach(th);
    alri_printf("  " GRN "✓" RST " Control channel on " CYN "127.0.0.1:%d" RST "\n", AR_CTL_PORT);
    return 0;
}

void ctl_stop(void) {
    ctl_running = 0;
    if (ctl_server_fd >= 0)
        ar_ipc_server_stop(ctl_server_fd);
    ctl_server_fd = -1;
}
