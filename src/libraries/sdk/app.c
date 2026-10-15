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
#include "alrios/crypto_verify.h"
#include <alrios/platform/fs_compat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>

#if defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <io.h>
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#define poll(p, n, t) WSAPoll((p), (n), (t))
#define close(s) closesocket(s)
#define SHUT_RDWR SD_BOTH
#else
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#ifndef F_GET_SEALS
#define F_GET_SEALS 1034
#endif
#ifndef F_SEAL_SEAL
#define F_SEAL_SEAL 0x0001
#endif
#ifndef F_SEAL_WRITE
#define F_SEAL_WRITE 0x0008
#endif

typedef struct ar_app_secret_entry {
    char key[AR_APP_SECRET_KEY_MAX];
    char val[AR_APP_SECRET_VAL_MAX];
} ar_app_secret_entry_t;

struct ar_app_context {
    ar_app_descriptor_t desc;
    ar_app_state_t      state;
    char                storage_path[AR_APP_STORAGE_PATH_MAX];
    void               *user_data;

    /* Vault & Secrets */
    ar_app_secret_entry_t secrets[AR_APP_MAX_SECRETS];
    size_t                secrets_count;
    char                  authorized_keys[AR_APP_MAX_SECRETS][AR_APP_SECRET_KEY_MAX];
    size_t                authorized_keys_count;
    int                   vault_memfd;

    /* Structured Logging */
    ar_app_log_sink_cb    log_sink;
    void                 *log_sink_user_data;
    char                  last_log[AR_APP_LOG_LINE_MAX];
};

__attribute__((weak)) const ar_app_descriptor_t *alrios_get_app_descriptor(void) {
    return NULL;
}

__attribute__((weak)) const ar_app_descriptor_t *ar_app_get_descriptor(void) {
    return NULL;
}

static int is_valid_app_id(const char *app_id) {
    if (!app_id || app_id[0] == '\0') {
        return 0;
    }
    size_t len = strlen(app_id);
    if (len >= AR_APP_ID_MAX_LEN) {
        return 0;
    }
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)app_id[i];
        if (!isalnum(c) && c != '.' && c != '-' && c != '_') {
            return 0;
        }
    }
    return 1;
}

static int is_valid_secret_key(const char *key) {
    if (!key || key[0] == '\0') {
        return 0;
    }
    size_t len = strlen(key);
    if (len >= AR_APP_SECRET_KEY_MAX) {
        return 0;
    }
    if (strchr(key, '/') != NULL || strchr(key, '\\') != NULL || strstr(key, "..") != NULL) {
        return 0;
    }
    return 1;
}

static int verify_memfd_sealed(int fd) {
#if defined(_WIN32)
    (void)fd;
    return 1;
#else
    if (fd < 0) {
        return 0;
    }
    int seals = fcntl(fd, F_GET_SEALS);
    if (seals < 0) {
        return 0;
    }
    if ((seals & (F_SEAL_SEAL | F_SEAL_WRITE)) != (F_SEAL_SEAL | F_SEAL_WRITE)) {
        return 0;
    }
    return 1;
#endif
}

ar_app_context_t *ar_app_context_create(const ar_app_descriptor_t *desc) {
    const ar_app_descriptor_t *d = desc;
    if (!d) {
        d = alrios_get_app_descriptor();
    }
    if (!d) {
        d = ar_app_get_descriptor();
    }
    if (!d || !d->app_id || !is_valid_app_id(d->app_id)) {
        return NULL;
    }

    ar_app_context_t *ctx = (ar_app_context_t *)calloc(1, sizeof(ar_app_context_t));
    if (!ctx) {
        return NULL;
    }

    ctx->desc = *d;
    ctx->state = AR_APP_STATE_UNINITIALIZED;
    ctx->vault_memfd = -1;

    int written = snprintf(ctx->storage_path, sizeof(ctx->storage_path), "/data/%s", d->app_id);
    if (written < 0 || (size_t)written >= sizeof(ctx->storage_path)) {
        free(ctx);
        return NULL;
    }

    return ctx;
}

void ar_app_context_destroy(ar_app_context_t *ctx) {
    if (!ctx) {
        return;
    }

    if (ctx->state != AR_APP_STATE_DESTROYED && ctx->desc.on_destroy) {
        ctx->desc.on_destroy(ctx);
    }

    if (ctx->vault_memfd >= 0) {
        close(ctx->vault_memfd);
        ctx->vault_memfd = -1;
    }

    alrios_explicit_zeroize(ctx->secrets, sizeof(ctx->secrets));
    alrios_explicit_zeroize(ctx->authorized_keys, sizeof(ctx->authorized_keys));
    alrios_explicit_zeroize(ctx->last_log, sizeof(ctx->last_log));
    alrios_explicit_zeroize(ctx, sizeof(ar_app_context_t));
    free(ctx);
}

int ar_app_init(ar_app_context_t *ctx) {
    if (!ctx) {
        return AR_APP_ERR_INVALID_ARG;
    }
    if (ctx->state != AR_APP_STATE_UNINITIALIZED) {
        return AR_APP_ERR_STATE;
    }

    if (ctx->desc.on_init) {
        int rc = ctx->desc.on_init(ctx);
        if (rc != AR_APP_OK) {
            ctx->state = AR_APP_STATE_ERROR;
            return rc;
        }
    }

    ctx->state = AR_APP_STATE_INITIALIZED;
    return AR_APP_OK;
}

int ar_app_start(ar_app_context_t *ctx) {
    if (!ctx) {
        return AR_APP_ERR_INVALID_ARG;
    }
    if (ctx->state != AR_APP_STATE_INITIALIZED && ctx->state != AR_APP_STATE_STOPPED) {
        return AR_APP_ERR_STATE;
    }

    if (ctx->desc.on_start) {
        int rc = ctx->desc.on_start(ctx);
        if (rc != AR_APP_OK) {
            ctx->state = AR_APP_STATE_ERROR;
            return rc;
        }
    }

    ctx->state = AR_APP_STATE_RUNNING;
    return AR_APP_OK;
}

int ar_app_tick(ar_app_context_t *ctx) {
    if (!ctx) {
        return AR_APP_ERR_INVALID_ARG;
    }
    if (ctx->state != AR_APP_STATE_RUNNING) {
        return AR_APP_ERR_STATE;
    }

    if (ctx->desc.on_event) {
        int rc = ctx->desc.on_event(ctx, AR_APP_EVENT_TICK, NULL);
        if (rc != AR_APP_OK) {
            ctx->state = AR_APP_STATE_ERROR;
            return rc;
        }
    }

    return AR_APP_OK;
}

int ar_app_event(ar_app_context_t *ctx, uint32_t event_type, void *event_data) {
    if (!ctx) {
        return AR_APP_ERR_INVALID_ARG;
    }
    if (ctx->state != AR_APP_STATE_RUNNING) {
        return AR_APP_ERR_STATE;
    }

    if (ctx->desc.on_event) {
        int rc = ctx->desc.on_event(ctx, event_type, event_data);
        if (rc != AR_APP_OK) {
            ctx->state = AR_APP_STATE_ERROR;
            return rc;
        }
    }

    return AR_APP_OK;
}

int ar_app_stop(ar_app_context_t *ctx) {
    if (!ctx) {
        return AR_APP_ERR_INVALID_ARG;
    }
    if (ctx->state != AR_APP_STATE_RUNNING) {
        return AR_APP_ERR_STATE;
    }

    if (ctx->desc.on_stop) {
        int rc = ctx->desc.on_stop(ctx);
        if (rc != AR_APP_OK) {
            ctx->state = AR_APP_STATE_ERROR;
            return rc;
        }
    }

    ctx->state = AR_APP_STATE_STOPPED;
    return AR_APP_OK;
}

int ar_app_destroy(ar_app_context_t *ctx) {
    if (!ctx) {
        return AR_APP_ERR_INVALID_ARG;
    }
    if (ctx->state == AR_APP_STATE_DESTROYED) {
        return AR_APP_OK;
    }

    if (ctx->desc.on_destroy) {
        ctx->desc.on_destroy(ctx);
    }

    ctx->state = AR_APP_STATE_DESTROYED;
    return AR_APP_OK;
}

ar_app_state_t ar_app_get_state(const ar_app_context_t *ctx) {
    if (!ctx) {
        return AR_APP_STATE_UNINITIALIZED;
    }
    return ctx->state;
}

const char *ar_app_state_to_string(ar_app_state_t state) {
    switch (state) {
        case AR_APP_STATE_UNINITIALIZED: return "UNINITIALIZED";
        case AR_APP_STATE_INITIALIZED:   return "INITIALIZED";
        case AR_APP_STATE_RUNNING:       return "RUNNING";
        case AR_APP_STATE_STOPPED:       return "STOPPED";
        case AR_APP_STATE_DESTROYED:     return "DESTROYED";
        case AR_APP_STATE_ERROR:         return "ERROR";
        default:                         return "UNKNOWN";
    }
}

const char *ar_app_get_id(const ar_app_context_t *ctx) {
    if (!ctx) {
        return NULL;
    }
    return ctx->desc.app_id;
}

const char *ar_app_get_version(const ar_app_context_t *ctx) {
    if (!ctx) {
        return NULL;
    }
    return ctx->desc.version;
}

void ar_app_set_user_data(ar_app_context_t *ctx, void *user_data) {
    if (ctx) {
        ctx->user_data = user_data;
    }
}

void *ar_app_get_user_data(const ar_app_context_t *ctx) {
    if (!ctx) {
        return NULL;
    }
    return ctx->user_data;
}

const char *ar_app_get_storage_path(const ar_app_context_t *ctx) {
    if (!ctx) {
        return NULL;
    }
    return ctx->storage_path;
}

int ar_app_get_storage_path_buf(const ar_app_context_t *ctx, char *out_buf, size_t buf_size) {
    if (!ctx || !out_buf || buf_size == 0) {
        return AR_APP_ERR_INVALID_ARG;
    }
    size_t len = strlen(ctx->storage_path);
    if (len >= buf_size) {
        return AR_APP_ERR_BUFFER_TOO_SMALL;
    }
    snprintf(out_buf, buf_size, "%s", ctx->storage_path);
    return AR_APP_OK;
}

int ar_app_context_set_vault_memfd(ar_app_context_t *ctx, int memfd) {
    if (!ctx) {
        return AR_APP_ERR_INVALID_ARG;
    }
    if (memfd < 0) {
        if (ctx->vault_memfd >= 0) {
            close(ctx->vault_memfd);
            ctx->vault_memfd = -1;
        }
        return AR_APP_OK;
    }

    if (!verify_memfd_sealed(memfd)) {
        return AR_APP_ERR_VAULT;
    }

    if (ctx->vault_memfd >= 0) {
        close(ctx->vault_memfd);
    }
    ctx->vault_memfd = memfd;
    return AR_APP_OK;
}

int ar_app_context_set_secret(ar_app_context_t *ctx, const char *key, const char *value) {
    if (!ctx || !key || !value || !is_valid_secret_key(key)) {
        return AR_APP_ERR_INVALID_ARG;
    }
    if (strlen(value) >= AR_APP_SECRET_VAL_MAX) {
        return AR_APP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < ctx->secrets_count; ++i) {
        if (strcmp(ctx->secrets[i].key, key) == 0) {
            snprintf(ctx->secrets[i].val, sizeof(ctx->secrets[i].val), "%s", value);
            return AR_APP_OK;
        }
    }

    if (ctx->secrets_count >= AR_APP_MAX_SECRETS) {
        return AR_APP_ERR_OOM;
    }

    snprintf(ctx->secrets[ctx->secrets_count].key, sizeof(ctx->secrets[ctx->secrets_count].key), "%s", key);
    snprintf(ctx->secrets[ctx->secrets_count].val, sizeof(ctx->secrets[ctx->secrets_count].val), "%s", value);
    ctx->secrets_count++;
    return AR_APP_OK;
}

int ar_app_context_set_authorized_keys(ar_app_context_t *ctx, const char *const *authorized_keys, size_t count) {
    if (!ctx) {
        return AR_APP_ERR_INVALID_ARG;
    }
    if (count > AR_APP_MAX_SECRETS) {
        return AR_APP_ERR_INVALID_ARG;
    }

    ctx->authorized_keys_count = 0;
    for (size_t i = 0; i < count; ++i) {
        if (!authorized_keys[i] || !is_valid_secret_key(authorized_keys[i])) {
            return AR_APP_ERR_INVALID_ARG;
        }
        snprintf(ctx->authorized_keys[i], sizeof(ctx->authorized_keys[i]), "%s", authorized_keys[i]);
    }
    ctx->authorized_keys_count = count;
    return AR_APP_OK;
}

static int parse_secret_from_memfd(
    int memfd,
    const char *app_id,
    const char *key,
    char *out_buf,
    size_t buf_size
) {
    char chunk[4096];
    ssize_t bytes_read = pread(memfd, chunk, sizeof(chunk) - 1, 0);
    if (bytes_read <= 0) {
        return AR_APP_ERR_NOT_FOUND;
    }
    chunk[bytes_read] = '\0';

    char *saveptr = NULL;
    char *line = strtok_r(chunk, "\n\r", &saveptr);
    while (line != NULL) {
        while (*line == ' ' || *line == '\t') {
            line++;
        }
        if (*line != '\0' && *line != '#') {
            char *eq = strchr(line, '=');
            if (eq != NULL) {
                *eq = '\0';
                char *line_key = line;
                char *line_val = eq + 1;

                char *colon = strchr(line_key, ':');
                if (colon != NULL) {
                    *colon = '\0';
                    const char *owner_app = line_key;
                    line_key = colon + 1;
                    if (strcmp(owner_app, app_id) != 0) {
                        line = strtok_r(NULL, "\n\r", &saveptr);
                        continue;
                    }
                }

                if (strcmp(line_key, key) == 0) {
                    size_t vlen = strlen(line_val);
                    if (vlen >= buf_size) {
                        return AR_APP_ERR_BUFFER_TOO_SMALL;
                    }
                    snprintf(out_buf, buf_size, "%s", line_val);
                    return AR_APP_OK;
                }
            }
        }
        line = strtok_r(NULL, "\n\r", &saveptr);
    }

    return AR_APP_ERR_NOT_FOUND;
}

int ar_app_get_secret(
    const ar_app_context_t *ctx,
    const char *key,
    char *out_buf,
    size_t buf_size
) {
    if (!ctx || !key || !out_buf || buf_size == 0 || !is_valid_secret_key(key)) {
        return AR_APP_ERR_INVALID_ARG;
    }

    if (ctx->authorized_keys_count > 0) {
        int authorized = 0;
        for (size_t i = 0; i < ctx->authorized_keys_count; ++i) {
            if (strcmp(ctx->authorized_keys[i], key) == 0) {
                authorized = 1;
                break;
            }
        }
        if (!authorized) {
            return AR_APP_ERR_UNAUTHORIZED;
        }
    }

    for (size_t i = 0; i < ctx->secrets_count; ++i) {
        if (strcmp(ctx->secrets[i].key, key) == 0) {
            size_t vlen = strlen(ctx->secrets[i].val);
            if (vlen >= buf_size) {
                return AR_APP_ERR_BUFFER_TOO_SMALL;
            }
            snprintf(out_buf, buf_size, "%s", ctx->secrets[i].val);
            return AR_APP_OK;
        }
    }

    if (ctx->vault_memfd >= 0) {
        return parse_secret_from_memfd(ctx->vault_memfd, ctx->desc.app_id, key, out_buf, buf_size);
    }

    return AR_APP_ERR_NOT_FOUND;
}

int ar_app_net_connect(
    ar_app_context_t *ctx,
    const char *host,
    uint16_t port,
    uint32_t timeout_ms
) {
    if (!ctx || !host || host[0] == '\0' || port == 0 || timeout_ms == 0) {
        return AR_APP_ERR_INVALID_ARG;
    }

#if defined(_WIN32)
    int fd = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return AR_APP_ERR_NETWORK;
    }
    u_long mode = 1;
    if (ioctlsocket((SOCKET)fd, FIONBIO, &mode) != 0) {
        closesocket((SOCKET)fd);
        return AR_APP_ERR_NETWORK;
    }
#else
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        return AR_APP_ERR_NETWORK;
    }

    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(fd);
        return AR_APP_ERR_NETWORK;
    }
#endif

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &sa.sin_addr) <= 0) {
        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%u", (unsigned int)port);
        struct addrinfo hints;
        struct addrinfo *result = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo(host, port_str, &hints, &result) != 0 || !result) {
            close(fd);
            return AR_APP_ERR_NETWORK;
        }
        memcpy(&sa, result->ai_addr, sizeof(sa));
        freeaddrinfo(result);
    }

    int rc = connect(fd, (struct sockaddr *)&sa, sizeof(sa));
    if (rc == 0) {
        return fd;
    }

#if defined(_WIN32)
    int werr = WSAGetLastError();
    if (werr != WSAEWOULDBLOCK && werr != WSAEINPROGRESS) {
        closesocket((SOCKET)fd);
        return AR_APP_ERR_NETWORK;
    }
#else
    if (errno != EINPROGRESS) {
        close(fd);
        return AR_APP_ERR_NETWORK;
    }
#endif

    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLOUT;
    pfd.revents = 0;

    int poll_rc = poll(&pfd, 1, (int)timeout_ms);
    if (poll_rc == 0) {
        close(fd);
        return AR_APP_ERR_TIMEOUT;
    }
    if (poll_rc < 0) {
        close(fd);
        return AR_APP_ERR_NETWORK;
    }

    if (pfd.revents & (POLLOUT | POLLERR | POLLHUP)) {
        int so_error = 0;
#if defined(_WIN32)
        int len = sizeof(so_error);
        if (getsockopt((SOCKET)fd, SOL_SOCKET, SO_ERROR, (char *)&so_error, &len) < 0 || so_error != 0) {
#else
        socklen_t len = sizeof(so_error);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &len) < 0 || so_error != 0) {
#endif
            close(fd);
            if (so_error == ETIMEDOUT || so_error == EINPROGRESS || so_error == EHOSTUNREACH || so_error == ENETUNREACH) {
                return AR_APP_ERR_TIMEOUT;
            }
            return AR_APP_ERR_NETWORK;
        }
        return fd;
    }

    close(fd);
    return AR_APP_ERR_NETWORK;
}

int ar_app_net_send(
    ar_app_context_t *ctx,
    int fd,
    const void *buf,
    size_t len,
    uint32_t timeout_ms
) {
    if (!ctx || fd < 0 || !buf || timeout_ms == 0) {
        return AR_APP_ERR_INVALID_ARG;
    }

    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLOUT;
    pfd.revents = 0;

    int poll_rc = poll(&pfd, 1, (int)timeout_ms);
    if (poll_rc == 0) {
        return AR_APP_ERR_TIMEOUT;
    }
    if (poll_rc < 0 || !(pfd.revents & POLLOUT)) {
        return AR_APP_ERR_NETWORK;
    }

    ssize_t sent = send(fd, buf, len, MSG_NOSIGNAL);
    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return AR_APP_ERR_TIMEOUT;
        }
        return AR_APP_ERR_NETWORK;
    }

    return (int)sent;
}

int ar_app_net_recv(
    ar_app_context_t *ctx,
    int fd,
    void *buf,
    size_t len,
    uint32_t timeout_ms
) {
    if (!ctx || fd < 0 || !buf || timeout_ms == 0) {
        return AR_APP_ERR_INVALID_ARG;
    }

    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    int poll_rc = poll(&pfd, 1, (int)timeout_ms);
    if (poll_rc == 0) {
        return AR_APP_ERR_TIMEOUT;
    }
    if (poll_rc < 0 || !(pfd.revents & POLLIN)) {
        return AR_APP_ERR_NETWORK;
    }

    ssize_t received = recv(fd, buf, len, 0);
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return AR_APP_ERR_TIMEOUT;
        }
        return AR_APP_ERR_NETWORK;
    }

    return (int)received;
}

void ar_app_net_close(ar_app_context_t *ctx, int fd) {
    (void)ctx;
    if (fd >= 0) {
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }
}

void ar_app_set_log_sink(
    ar_app_context_t *ctx,
    ar_app_log_sink_cb sink,
    void *user_data
) {
    if (ctx) {
        ctx->log_sink = sink;
        ctx->log_sink_user_data = user_data;
    }
}

const char *ar_app_context_get_last_log(const ar_app_context_t *ctx) {
    if (!ctx) {
        return NULL;
    }
    return ctx->last_log;
}

static const char *level_to_string(ar_app_log_level_t level) {
    switch (level) {
        case AR_LOG_LEVEL_DEBUG: return "DEBUG";
        case AR_LOG_LEVEL_INFO:  return "INFO";
        case AR_LOG_LEVEL_WARN:  return "WARN";
        case AR_LOG_LEVEL_ERROR: return "ERROR";
        case AR_LOG_LEVEL_FATAL: return "FATAL";
        default:                 return "INFO";
    }
}

static void escape_json_string(const char *src, char *dst, size_t dst_size) {
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j + 2 < dst_size; ++i) {
        if (src[i] == '"' || src[i] == '\\') {
            dst[j++] = '\\';
            dst[j++] = src[i];
        } else if (src[i] == '\n') {
            dst[j++] = '\\';
            dst[j++] = 'n';
        } else if (src[i] == '\r') {
            dst[j++] = '\\';
            dst[j++] = 'r';
        } else if (src[i] == '\t') {
            dst[j++] = '\\';
            dst[j++] = 't';
        } else {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
}

int ar_app_vlog(
    ar_app_context_t *ctx,
    ar_app_log_level_t level,
    const char *fmt,
    va_list args
) {
    if (!ctx || !fmt) {
        return AR_APP_ERR_INVALID_ARG;
    }

    char raw_msg[1024];
    vsnprintf(raw_msg, sizeof(raw_msg), fmt, args);

    char escaped_msg[1536];
    escape_json_string(raw_msg, escaped_msg, sizeof(escaped_msg));

#if defined(_WIN32)
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    /* Convert 100-ns intervals since Jan 1 1601 to ms since Jan 1 1970 */
    uint64_t ts_ms = (uli.QuadPart - 116444736000000000ULL) / 10000ULL;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t ts_ms = ((uint64_t)ts.tv_sec * 1000ULL) + ((uint64_t)ts.tv_nsec / 1000000ULL);
#endif

    int n = snprintf(
        ctx->last_log,
        sizeof(ctx->last_log),
        "{\"timestamp\":%llu,\"level\":\"%s\",\"app_id\":\"%s\",\"message\":\"%s\"}",
        (unsigned long long)ts_ms,
        level_to_string(level),
        ctx->desc.app_id ? ctx->desc.app_id : "unknown",
        escaped_msg
    );

    if (n < 0 || (size_t)n >= sizeof(ctx->last_log)) {
        return AR_APP_ERR_BUFFER_TOO_SMALL;
    }

    if (ctx->log_sink) {
        ctx->log_sink(level, ctx->desc.app_id, ctx->last_log, ctx->log_sink_user_data);
    }

    return AR_APP_OK;
}

int ar_app_log(
    ar_app_context_t *ctx,
    ar_app_log_level_t level,
    const char *fmt,
    ...
) {
    va_list args;
    va_start(args, fmt);
    int rc = ar_app_vlog(ctx, level, fmt, args);
    va_end(args);
    return rc;
}
