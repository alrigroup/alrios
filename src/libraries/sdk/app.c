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
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

static int valid_text(const char *value, size_t max_length) {
    size_t length;

    if (!value || value[0] == '\0') {
        return 0;
    }
    length = strnlen(value, max_length);
    return length > 0U && length < max_length;
}

static int valid_config_key(const char *key) {
    size_t index;

    if (!valid_text(key, 128U) || strchr(key, '/') || strchr(key, '\\')) {
        return 0;
    }
    for (index = 0U; key[index] != '\0'; ++index) {
        char character = key[index];
        if (!((character >= 'A' && character <= 'Z') ||
              (character >= '0' && character <= '9') ||
              character == '_')) {
            return 0;
        }
    }
    return 1;
}

static int valid_relative_path(const char *path) {
    const char *component;
    const char *cursor;

    if (!valid_text(path, 256U) || path[0] == '/' || strchr(path, '\\')) {
        return 0;
    }
    component = path;
    cursor = path;
    for (;;) {
        size_t component_length;

        while (*cursor != '\0' && *cursor != '/') {
            unsigned char character = (unsigned char)*cursor;

            if (character < 0x20U || character == 0x7fU) {
                return 0;
            }
            ++cursor;
        }
        component_length = (size_t)(cursor - component);
        if (component_length == 0U ||
            (component_length == 1U && component[0] == '.') ||
            (component_length == 2U && component[0] == '.' && component[1] == '.')) {
            return 0;
        }
        if (*cursor == '\0') {
            return 1;
        }
        component = ++cursor;
    }
}

static int wait_for_fd(int fd, int write_ready, uint32_t timeout_ms) {
    fd_set fds;
    struct timeval timeout;
    int result;

    if (fd < 0 || fd >= FD_SETSIZE || timeout_ms == 0U) {
        return AR_APP_ERR_INVALID_ARGUMENT;
    }
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    timeout.tv_sec = (time_t)(timeout_ms / 1000U);
    timeout.tv_usec = (suseconds_t)((timeout_ms % 1000U) * 1000U);
    do {
        result = select(fd + 1,
                        write_ready ? NULL : &fds,
                        write_ready ? &fds : NULL,
                        NULL,
                        &timeout);
    } while (result < 0 && errno == EINTR);
    if (result == 0) {
        return AR_APP_ERR_TIMEOUT;
    }
    if (result < 0) {
        return AR_APP_ERR_NETWORK;
    }
    return AR_APP_OK;
}

const char *ar_app_get_storage_path(const ar_app_context_t *ctx,
                                    const char *relative_filename,
                                    char *out_buf,
                                    size_t buf_size) {
    const char *base;
    int written;

    if (!ctx || !out_buf || buf_size == 0U ||
        !valid_relative_path(relative_filename) ||
        !valid_text(ctx->persistent_storage_dir, 512U) ||
        ctx->persistent_storage_dir[0] != '/') {
        return NULL;
    }
    base = ctx->persistent_storage_dir;
    written = snprintf(out_buf, buf_size, "%s/%s", base, relative_filename);
    if (written < 0 || (size_t)written >= buf_size) {
        out_buf[0] = '\0';
        return NULL;
    }
    return out_buf;
}

const char *ar_app_get_config_value(const ar_app_context_t *ctx, const char *key) {
    if (!ctx || !ctx->config_get || !valid_config_key(key)) {
        return NULL;
    }
    return ctx->config_get(ctx->config_backend_context, key);
}

int ar_app_log(const ar_app_context_t *ctx,
               ar_app_log_level_t level,
               const char *event,
               const char *message,
               const ar_app_log_field_t *fields,
               size_t field_count) {
    size_t index;

    if (!ctx || !ctx->log_sink || !valid_text(ctx->app_id, 128U) ||
        !valid_text(event, AR_APP_MAX_LOG_EVENT_LENGTH + 1U) ||
        !valid_text(message, AR_APP_MAX_LOG_MESSAGE_LENGTH + 1U) ||
        level < AR_APP_LOG_DEBUG || level > AR_APP_LOG_ERROR ||
        field_count > AR_APP_MAX_LOG_FIELDS || (field_count > 0U && !fields)) {
        return AR_APP_ERR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < field_count; ++index) {
        if (!valid_text(fields[index].key, 128U) || !fields[index].value) {
            return AR_APP_ERR_INVALID_ARGUMENT;
        }
    }
    return ctx->log_sink(ctx->log_backend_context, ctx->app_id, level, event,
                         message, fields, field_count);
}

int ar_net_connect_timeout(const char *host, uint16_t port, uint32_t timeout_ms, int *out_fd) {
    struct sockaddr_in address;
    socklen_t error_length = sizeof(int);
    int fd;
    int flags;
    int socket_error = 0;
    int result;

    if (out_fd) {
        *out_fd = -1;
    }
    if (!host || !valid_text(host, 256U) || !out_fd ||
        port == 0U || timeout_ms == 0U) {
        return AR_APP_ERR_INVALID_ARGUMENT;
    }
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return AR_APP_ERR_NETWORK;
    }
    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        (void)close(fd);
        return AR_APP_ERR_NETWORK;
    }
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &address.sin_addr) != 1) {
        (void)close(fd);
        return AR_APP_ERR_INVALID_ARGUMENT;
    }
    result = connect(fd, (const struct sockaddr *)&address, sizeof(address));
    if (result < 0 && errno != EINPROGRESS) {
        (void)close(fd);
        return AR_APP_ERR_NETWORK;
    }
    if (result < 0) {
        result = wait_for_fd(fd, 1, timeout_ms);
        if (result != AR_APP_OK ||
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_length) != 0 ||
            socket_error != 0) {
            (void)close(fd);
            return result == AR_APP_OK ? AR_APP_ERR_NETWORK : result;
        }
    }
    *out_fd = fd;
    return AR_APP_OK;
}

int ar_net_send_timeout(int fd, const void *buf, size_t len, uint32_t timeout_ms, size_t *out_sent) {
    ssize_t sent;
    int result;

    if (out_sent) {
        *out_sent = 0U;
    }
    if (fd < 0 || !buf || len == 0U || len > (size_t)SSIZE_MAX ||
        timeout_ms == 0U || !out_sent) {
        return AR_APP_ERR_INVALID_ARGUMENT;
    }
    result = wait_for_fd(fd, 1, timeout_ms);
    if (result != AR_APP_OK) {
        return result;
    }
    do {
        sent = send(fd, buf, len, MSG_NOSIGNAL);
    } while (sent < 0 && errno == EINTR);
    if (sent < 0) {
        return errno == EAGAIN || errno == EWOULDBLOCK ? AR_APP_ERR_TIMEOUT : AR_APP_ERR_NETWORK;
    }
    *out_sent = (size_t)sent;
    return AR_APP_OK;
}

int ar_net_recv_timeout(int fd, void *buf, size_t len, uint32_t timeout_ms, size_t *out_recvd) {
    ssize_t received;
    int result;

    if (out_recvd) {
        *out_recvd = 0U;
    }
    if (fd < 0 || !buf || len == 0U || len > (size_t)SSIZE_MAX ||
        timeout_ms == 0U || !out_recvd) {
        return AR_APP_ERR_INVALID_ARGUMENT;
    }
    result = wait_for_fd(fd, 0, timeout_ms);
    if (result != AR_APP_OK) {
        return result;
    }
    do {
        received = recv(fd, buf, len, 0);
    } while (received < 0 && errno == EINTR);
    if (received < 0) {
        return errno == EAGAIN || errno == EWOULDBLOCK ? AR_APP_ERR_TIMEOUT : AR_APP_ERR_NETWORK;
    }
    *out_recvd = (size_t)received;
    return AR_APP_OK;
}

int ar_app_execute_with_context(const ar_app_spec_t *spec, ar_app_context_t *context) {
    int result;

    if (!spec || !context || !spec->init || !spec->run || !spec->cleanup ||
        !valid_text(spec->name, 128U) || !valid_text(spec->version, 64U) ||
        !valid_text(context->app_id, 128U) ||
        !valid_text(context->persistent_storage_dir, 512U) ||
        context->persistent_storage_dir[0] != '/') {
        return AR_APP_ERR_INVALID_ARGUMENT;
    }
    result = spec->init(context);
    if (result != AR_APP_OK) {
        return result;
    }
    result = spec->run(context);
    spec->cleanup(context);
    return result;
}

int ar_app_execute(const ar_app_spec_t *spec, const char *app_id, const char *storage_dir) {
    ar_app_context_t context;

    if (!spec || !app_id || !storage_dir) {
        return AR_APP_ERR_INVALID_ARGUMENT;
    }
    memset(&context, 0, sizeof(context));
    context.app_id = app_id;
    context.persistent_storage_dir = storage_dir;
    return ar_app_execute_with_context(spec, &context);
}
