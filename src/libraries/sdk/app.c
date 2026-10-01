/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "alrios/sdk/app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

const char *ar_app_get_storage_path(const ar_app_context_t *ctx, const char *relative_filename, char *out_buf, size_t buf_size) {
    if (!ctx || !relative_filename || !out_buf || buf_size == 0) {
        return NULL;
    }
    const char *base = ctx->persistent_storage_dir ? ctx->persistent_storage_dir : "/var/data";
    int n = snprintf(out_buf, buf_size, "%s/%s", base, relative_filename);
    if (n < 0 || (size_t)n >= buf_size) {
        return NULL;
    }
    return out_buf;
}

const char *ar_app_get_config_value(const ar_app_context_t *ctx, const char *key) {
    (void)ctx;
    if (!key) return NULL;
    return getenv(key);
}

int ar_net_connect_timeout(const char *host, uint16_t port, uint32_t timeout_ms, int *out_fd) {
    if (!host || !out_fd || timeout_ms == 0) {
        return AR_APP_ERR_INVALID_ARGUMENT;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return AR_APP_ERR_NETWORK;

    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    int rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (rc < 0 && errno == EINPROGRESS) {
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(fd, &wfds);

        struct timeval tv = {
            .tv_sec = (time_t)(timeout_ms / 1000U),
            .tv_usec = (long)(timeout_ms % 1000U) * 1000L
        };

        rc = select(fd + 1, NULL, &wfds, NULL, &tv);
        if (rc > 0) {
            int err = 0;
            socklen_t len = sizeof(err);
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
            if (err == 0) {
                *out_fd = fd;
                return AR_APP_OK;
            }
        } else if (rc == 0) {
            close(fd);
            return AR_APP_ERR_TIMEOUT;
        }
    } else if (rc == 0) {
        *out_fd = fd;
        return AR_APP_OK;
    }

    close(fd);
    return AR_APP_ERR_NETWORK;
}

int ar_net_send_timeout(int fd, const void *buf, size_t len, uint32_t timeout_ms, size_t *out_sent) {
    if (fd < 0 || !buf || timeout_ms == 0) return AR_APP_ERR_INVALID_ARGUMENT;

    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(fd, &wfds);

    struct timeval tv = {
        .tv_sec = (time_t)(timeout_ms / 1000U),
        .tv_usec = (long)(timeout_ms % 1000U) * 1000L
    };

    int s = select(fd + 1, NULL, &wfds, NULL, &tv);
    if (s < 0) return AR_APP_ERR_NETWORK;
    if (s == 0) return AR_APP_ERR_TIMEOUT;

    ssize_t sent = send(fd, buf, len, MSG_NOSIGNAL);
    if (sent < 0) return AR_APP_ERR_NETWORK;

    if (out_sent) *out_sent = (size_t)sent;
    return AR_APP_OK;
}

int ar_net_recv_timeout(int fd, void *buf, size_t len, uint32_t timeout_ms, size_t *out_recvd) {
    if (fd < 0 || !buf || timeout_ms == 0) return AR_APP_ERR_INVALID_ARGUMENT;

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    struct timeval tv = {
        .tv_sec = (time_t)(timeout_ms / 1000U),
        .tv_usec = (long)(timeout_ms % 1000U) * 1000L
    };

    int s = select(fd + 1, &rfds, NULL, NULL, &tv);
    if (s < 0) return AR_APP_ERR_NETWORK;
    if (s == 0) return AR_APP_ERR_TIMEOUT;

    ssize_t recvd = recv(fd, buf, len, 0);
    if (recvd < 0) return AR_APP_ERR_NETWORK;

    if (out_recvd) *out_recvd = (size_t)recvd;
    return AR_APP_OK;
}

int ar_app_execute(const ar_app_spec_t *spec, const char *app_id, const char *storage_dir) {
    if (!spec) return AR_APP_ERR_INVALID_ARGUMENT;

    ar_app_context_t ctx = {
        .app_id = app_id ? app_id : "sovereign.app",
        .persistent_storage_dir = storage_dir ? storage_dir : "/var/data",
        .user_data = NULL
    };

    if (spec->init) {
        int init_res = spec->init(&ctx);
        if (init_res != AR_APP_OK) return init_res;
    }

    int run_res = AR_APP_OK;
    if (spec->run) {
        run_res = spec->run(&ctx);
    }

    if (spec->cleanup) {
        spec->cleanup(&ctx);
    }

    return run_res;
}
