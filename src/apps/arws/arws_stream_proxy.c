/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arws_stream_proxy.h"
#include "arws_utils.h"
#include "log.h"
#include "aros_hal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>
#endif

static int parse_target(const char *url, char *host, int host_size,
                        int *port) {
    if (!url || !host || host_size < 1) return -1;
    *port = 80;
    const char *p = url;
    int scheme_len = 0;

    if (strncmp(p, "ws://", 5) == 0)       scheme_len = 5;
    else if (strncmp(p, "wss://", 6) == 0)  { scheme_len = 6; *port = 443; }
    else if (strncmp(p, "http://", 7) == 0) scheme_len = 7;
    else if (strncmp(p, "https://", 8) == 0) { scheme_len = 8; *port = 443; }
    else return -1;
    p += scheme_len;

    int hi = 0;
    while (*p && *p != ':' && *p != '/' && *p != '?' && hi < host_size - 1)
        host[hi++] = *p++;
    host[hi] = '\0';

    if (*p == ':') {
        p++;
        *port = 0;
        int digits = 0;
        while (*p >= '0' && *p <= '9') {
            if (digits >= 5 || *port > (65535 - (*p - '0')) / 10)
                return -1;
            *port = (*port * 10) + (*p++ - '0');
            digits++;
        }
        if (*port <= 0) return -1;
    }
    return 0;
}

static int append_str(unsigned char *buf, int bufsize, int pos, const char *fmt, ...) {
    int rem = bufsize - pos;
    if (rem <= 0) return -1;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf((char *)buf + pos, rem, fmt, ap);
    va_end(ap);
    if (n < 0 || n >= rem) return -1;
    return pos + n;
}

int arws_build_http_request(ClientConnection *conn, HttpRequest *req,
                            unsigned char *buf, int bufsize) {
    const char *ip = server_get_client_ip(conn);
    int pos = 0;
    int has_host = 0, has_connection = 0;

    pos = append_str(buf, bufsize, pos, "%s %s", req->method, req->path);
    if (pos < 0) return -1;
    if (req->query_params && req->query_params[0]) {
        pos = append_str(buf, bufsize, pos, "?%s", req->query_params);
        if (pos < 0) return -1;
    }
    pos = append_str(buf, bufsize, pos, " HTTP/1.1\r\n");
    if (pos < 0) return -1;

    for (int i = 0; i < req->header_count; i++) {
        const char *n = req->headers[i].name;
        if (!has_host && strcasecmp(n, "Host") == 0) has_host = 1;
        if (!has_connection && (strcasecmp(n, "Connection") == 0
            || strcasecmp(n, "Proxy-Connection") == 0)) has_connection = 1;
        pos = append_str(buf, bufsize, pos, "%s: %s\r\n", n, req->headers[i].value);
        if (pos < 0) return -1;
    }

    if (!has_host && req->host[0]) {
        pos = append_str(buf, bufsize, pos, "Host: %s\r\n", req->host);
        if (pos < 0) return -1;
    }
    pos = append_str(buf, bufsize, pos, "X-Forwarded-For: %s\r\n", ip ? ip : "unknown");
    if (pos < 0) return -1;
    if (!has_connection) {
        pos = append_str(buf, bufsize, pos, "Connection: keep-alive\r\n");
        if (pos < 0) return -1;
    }
    pos = append_str(buf, bufsize, pos, "\r\n");
    if (pos < 0) return -1;

    if (req->body && req->body[0]) {
        int rem = bufsize - pos;
        if (rem > 0) {
            int body_len = strlen(req->body);
            int to_copy = body_len < rem ? body_len : rem;
            memcpy(buf + pos, req->body, to_copy);
            pos += to_copy;
        }
    }
    return pos > 0 ? pos : -1;
}

static int eintr_select(int maxfd, fd_set *rfds, struct timeval *tv) {
    int r;
    do r = select(maxfd, rfds, NULL, NULL, tv);
    while (r < 0
#ifdef EINTR
           && errno == EINTR
#endif
          );
    return r;
}

static int eintr_recv(int fd, void *buf, size_t len) {
    int n;
    do n = ar_socket_recv(fd, buf, len);
    while (n < 0
#ifdef EINTR
           && n == -EINTR
#endif
          );
    return n;
}

static int pipe_data(int src_fd, int dst_fd, int *closes) {
    char buf[ARWS_STREAM_BUF_SIZE];
    int n = eintr_recv(src_fd, buf, sizeof(buf));
    if (n <= 0) { *closes = 1; return n; }
    int sent = 0;
    while (sent < n) {
        int w = ar_socket_send(dst_fd, buf + sent, n - sent);
        if (w <= 0) { *closes = 1; return -1; }
        sent += w;
    }
    return n;
}

static void parse_response_headers(unsigned char *buf, int header_end,
                                   int *is_websocket, int *is_chunked,
                                   int *content_length) {
    char *hdr = (char *)buf;
    int pos = 0;
    *is_websocket = 0;
    *is_chunked = 0;
    *content_length = -1;

    if (header_end >= 13 && strncmp(hdr, "HTTP/1.1 101 ", 13) == 0)
        *is_websocket = 1;

    while (pos < header_end) {
        char *line_end = (char *)memchr(hdr + pos, '\r', header_end - pos);
        if (!line_end) break;
        int line_len = line_end - (hdr + pos);
        if (line_len == 0) break;

        char *val = (char *)memchr(hdr + pos, ':', line_len);
        if (val) {
            int hn_len = val - (hdr + pos);
            val++;
            while (val < hdr + header_end && *val == ' ') val++;
            int hv_len = line_end - val;

            if (hn_len == 14 && strncasecmp(hdr + pos, "Content-Length", 14) == 0) {
                char cl_buf[32];
                int cln = hv_len < 31 ? hv_len : 31;
                memcpy(cl_buf, val, cln);
                cl_buf[cln] = '\0';
                *content_length = atoi(cl_buf);
            } else if (hn_len == 17 && strncasecmp(hdr + pos, "Transfer-Encoding", 17) == 0) {
                if (hv_len >= 7 && strncasecmp(val, "chunked", 7) == 0)
                    *is_chunked = 1;
            }
        }
        pos = line_end - hdr + 2;
        if (pos >= header_end) break;
    }
}

#define UPSTREAM_POOL_MAX 512

typedef struct {
    char host[128];
    int port;
    int fd;
} UpstreamConn;

static UpstreamConn upstream_pool[UPSTREAM_POOL_MAX];
static int upstream_pool_count = 0;
static void *upstream_pool_mutex = NULL;

static int socket_is_alive(int fd) {
    if (fd < 0) return 0;
    char buf[1];
#ifdef MSG_PEEK
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(fd, FIONBIO, &mode);
    int r = recv(fd, buf, 1, MSG_PEEK);
    int err = WSAGetLastError();
    if (r == 0 || (r < 0 && err != WSAEWOULDBLOCK)) return 0;
#else
    int r = recv(fd, buf, 1, MSG_PEEK | MSG_DONTWAIT);
    if (r == 0 || (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) return 0;
#endif
#endif
    return 1;
}

static int upstream_get_conn(const char *host, int port) {
    if (!upstream_pool_mutex) upstream_pool_mutex = ar_mutex_create();
    ar_mutex_lock(upstream_pool_mutex);
    for (int i = upstream_pool_count - 1; i >= 0; i--) {
        if (upstream_pool[i].port == port && strcmp(upstream_pool[i].host, host) == 0) {
            int fd = upstream_pool[i].fd;
            upstream_pool[i] = upstream_pool[--upstream_pool_count];
            ar_mutex_unlock(upstream_pool_mutex);
            if (socket_is_alive(fd)) return fd;
            ar_socket_close(fd);
            ar_mutex_lock(upstream_pool_mutex);
        }
    }
    ar_mutex_unlock(upstream_pool_mutex);

    int fd = ar_socket_create(1);
    if (fd < 0) return -1;
    if (ar_socket_connect(fd, host, (uint16_t)port) < 0) {
        ar_socket_close(fd);
        return -1;
    }
    return fd;
}

static void upstream_release_conn(const char *host, int port, int fd, int keep_alive) {
    if (!keep_alive || fd < 0) {
        if (fd >= 0) ar_socket_close(fd);
        return;
    }
    if (!upstream_pool_mutex) upstream_pool_mutex = ar_mutex_create();
    ar_mutex_lock(upstream_pool_mutex);
    if (upstream_pool_count < UPSTREAM_POOL_MAX) {
        strncpy(upstream_pool[upstream_pool_count].host, host, sizeof(upstream_pool[0].host) - 1);
        upstream_pool[upstream_pool_count].host[sizeof(upstream_pool[0].host) - 1] = '\0';
        upstream_pool[upstream_pool_count].port = port;
        upstream_pool[upstream_pool_count].fd = fd;
        upstream_pool_count++;
        ar_mutex_unlock(upstream_pool_mutex);
        return;
    }
    ar_mutex_unlock(upstream_pool_mutex);
    ar_socket_close(fd);
}

int arws_stream_proxy_forward(ClientConnection *conn, HttpRequest *req,
                              const char *target_url) {
    char host[256];
    int port;

    if (parse_target(target_url, host, sizeof(host), &port) < 0) {
        arws_send_502(conn, "Bad Gateway");
        return -1;
    }

    int target_fd = upstream_get_conn(host, port);
    if (target_fd < 0) { arws_send_502(conn, "Bad Gateway"); return -1; }

    unsigned char *raw_buf = (unsigned char *)ar_mem_alloc(ARWS_STREAM_BUF_SIZE);
    if (!raw_buf) {
        ar_socket_close(target_fd);
        arws_send_502(conn, "Bad Gateway");
        return -1;
    }
    int raw_len = arws_build_http_request(conn, req, raw_buf, ARWS_STREAM_BUF_SIZE);
    if (raw_len <= 0) {
        ar_mem_free(raw_buf);
        ar_socket_close(target_fd);
        arws_send_502(conn, "Bad Gateway");
        return -1;
    }

    int sent = 0;
    while (sent < raw_len) {
        int n = ar_socket_send(target_fd, (const char*)raw_buf + sent, raw_len - sent);
        if (n <= 0) {
            ar_mem_free(raw_buf);
            ar_socket_close(target_fd);
            arws_send_502(conn, "Bad Gateway");
            return -1;
        }
        sent += n;
    }
    ar_mem_free(raw_buf);

    int client_fd = server_conn_get_fd(conn);
    if (client_fd < 0) {
        ar_socket_close(target_fd);
        arws_send_502(conn, "Bad Gateway");
        return -1;
    }

    unsigned char *resp_buf = (unsigned char *)ar_mem_alloc(ARWS_STREAM_BUF_SIZE);
    if (!resp_buf) {
        ar_socket_close(target_fd);
        arws_send_502(conn, "Bad Gateway");
        return -1;
    }

    int total = 0;
    int header_end = -1;
    fd_set rfds;
    struct timeval tv;

    while (total < ARWS_STREAM_BUF_SIZE) {
        FD_ZERO(&rfds);
        FD_SET(target_fd, &rfds);
        tv.tv_sec = ARWS_STREAM_TIMEOUT_MS / 1000;
        tv.tv_usec = (ARWS_STREAM_TIMEOUT_MS % 1000) * 1000;
        int r = eintr_select(target_fd + 1, &rfds, &tv);
        if (r <= 0) break;

        int n = eintr_recv(target_fd, resp_buf + total, ARWS_STREAM_BUF_SIZE - total);
        if (n <= 0) break;
        int newly = total;
        total += n;

        for (int i = newly > 3 ? newly - 3 : 0; i < total - 3; i++) {
            if (resp_buf[i] == '\r' && resp_buf[i+1] == '\n' &&
                resp_buf[i+2] == '\r' && resp_buf[i+3] == '\n') {
                header_end = i + 4;
                break;
            }
        }
        if (header_end >= 0) break;
    }

    if (header_end < 0 || total <= 0) {
        ar_mem_free(resp_buf);
        ar_socket_close(target_fd);
        arws_send_502(conn, "Upstream error");
        return -1;
    }

    server_conn_write(conn, resp_buf, total);

    int is_websocket, is_chunked, content_length;
    parse_response_headers(resp_buf, header_end,
                           &is_websocket, &is_chunked, &content_length);

    if (is_websocket) {
        int closes = 0;
        while (!closes) {
            FD_ZERO(&rfds);
            FD_SET(target_fd, &rfds);
            FD_SET(client_fd, &rfds);
            int maxfd = (target_fd > client_fd) ? target_fd : client_fd;
            tv.tv_sec = ARWS_STREAM_TIMEOUT_MS / 1000;
            tv.tv_usec = 0;

            int r = eintr_select(maxfd + 1, &rfds, &tv);
            if (r <= 0) break;

            if (FD_ISSET(target_fd, &rfds)) {
                int n = eintr_recv(target_fd, resp_buf, ARWS_STREAM_BUF_SIZE);
                if (n <= 0) break;
                int ws = 0;
                while (ws < n) {
                    int w = server_conn_write(conn, resp_buf + ws, n - ws);
                    if (w <= 0) { closes = 1; break; }
                    ws += w;
                }
            }
            if (closes) break;

            if (FD_ISSET(client_fd, &rfds)) {
                if (pipe_data(client_fd, target_fd, &closes) <= 0) break;
            }
        }
    } else if (content_length >= 0) {
        int body_done = total - header_end;
        while (body_done < content_length) {
            FD_ZERO(&rfds);
            FD_SET(target_fd, &rfds);
            tv.tv_sec = ARWS_STREAM_TIMEOUT_MS / 1000;
            tv.tv_usec = 0;
            int r = eintr_select(target_fd + 1, &rfds, &tv);
            if (r <= 0) break;

            int n = eintr_recv(target_fd, resp_buf, ARWS_STREAM_BUF_SIZE);
            if (n <= 0) break;

            int remain = content_length - body_done;
            int to_write = n < remain ? n : remain;
            if (to_write > 0) {
                int ws = 0;
                while (ws < to_write) {
                    int w = server_conn_write(conn, resp_buf + ws, to_write - ws);
                    if (w <= 0) break;
                    ws += w;
                }
                body_done += ws;
                if (ws < to_write) break;
            }
            if (body_done >= content_length) break;
        }
    } else {
        while (1) {
            FD_ZERO(&rfds);
            FD_SET(target_fd, &rfds);
            tv.tv_sec = ARWS_STREAM_TIMEOUT_MS / 1000;
            tv.tv_usec = 0;
            int r = eintr_select(target_fd + 1, &rfds, &tv);
            if (r <= 0) break;

            int n = eintr_recv(target_fd, resp_buf, ARWS_STREAM_BUF_SIZE);
            if (n <= 0) break;

            int ws = 0;
            while (ws < n) {
                int w = server_conn_write(conn, resp_buf + ws, n - ws);
                if (w <= 0) break;
                ws += w;
            }
        }
    }

    ar_mem_free(resp_buf);
    upstream_release_conn(host, port, target_fd, 0);
    return 0;
}
