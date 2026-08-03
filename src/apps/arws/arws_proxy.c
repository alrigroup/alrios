/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arws_proxy.h"
#include "log.h"
#include "aros_hal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/select.h>
#endif

static int parse_url(const char *url, char *host, int host_size,
                     int *port) {
    if (!url || !host || host_size < 1) return -1;

    *port = 80;
    const char *p = url;

    if (strncmp(p, "http://", 7) == 0) p += 7;
    else if (strncmp(p, "https://", 8) == 0) { p += 8; *port = 443; }
    else return -1;

    int hi = 0;
    while (*p && *p != ':' && *p != '/' && *p != '?' && hi < host_size - 1)
        host[hi++] = *p++;
    host[hi] = '\0';

    if (*p == ':') {
        p++;
        *port = 0;
        while (*p >= '0' && *p <= '9') {
            *port = (*port * 10) + (*p - '0');
            p++;
        }
    }

    return 0;
}

static int build_502_response(unsigned char *buf, int maxlen) {
    const char *body = "<html><body><h1>502 Bad Gateway</h1></body></html>";
    int body_len = strlen(body);
    return snprintf((char *)buf, maxlen,
        "HTTP/1.1 502 Bad Gateway\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n\r\n%s",
        body_len, body);
}

int arws_proxy_init(void) {
    return 0;
}

int arws_proxy_forward(const char *target_url,
                       const unsigned char *raw_req, int raw_len,
                       unsigned char *out_resp, int max_resp_len) {
    char host[256];
    int port;

    if (parse_url(target_url, host, sizeof(host), &port) < 0) {
        return build_502_response(out_resp, max_resp_len);
    }

    int fd = ar_socket_create(1);
    if (fd < 0) {
        alri_print(RED "[ARWS]" RST " proxy: socket create failed\n");
        return build_502_response(out_resp, max_resp_len);
    }

    if (ar_socket_connect(fd, host, (uint16_t)port) < 0) {
        alri_print(RED "[ARWS]" RST " proxy: connect to %s:%d failed\n", host, port);
        ar_socket_close(fd);
        return build_502_response(out_resp, max_resp_len);
    }

    int sent = 0;
    while (sent < raw_len) {
        int n = ar_socket_send(fd, (const char*)raw_req + sent, raw_len - sent);
        if (n <= 0) {
            alri_print(RED "[ARWS]" RST " proxy: send failed\n");
            ar_socket_close(fd);
            return build_502_response(out_resp, max_resp_len);
        }
        sent += n;
    }

    int total = 0;
    int header_end = -1;

    while (total < max_resp_len) {
        int r;
        if (total > 0) {
            fd_set rfds;
            struct timeval tv;
            FD_ZERO(&rfds);
            FD_SET(fd, &rfds);
            tv.tv_sec = ARWS_PROXY_TIMEOUT_MS / 1000;
            tv.tv_usec = (ARWS_PROXY_TIMEOUT_MS % 1000) * 1000;
            r = select(fd + 1, &rfds, NULL, NULL, &tv);
            if (r <= 0) break;
        }

        int n = ar_socket_recv(fd, out_resp + total, max_resp_len - total);
        if (n <= 0) break;
        total += n;

        if (header_end < 0) {
            for (int i = (total > 4 ? total - 4 : 0); i < total - 3; i++) {
                if (out_resp[i] == '\r' && out_resp[i+1] == '\n' &&
                    out_resp[i+2] == '\r' && out_resp[i+3] == '\n') {
                    header_end = i + 4;
                    break;
                }
            }
        }

        if (header_end >= 0) {
            int body_so_far = total - header_end;
            const char *cl = strstr((const char *)out_resp, "Content-Length: ");
            if (!cl) cl = strstr((const char *)out_resp, "content-length: ");
            if (!cl) cl = strstr((const char *)out_resp, "Content-length: ");

            if (cl && cl < (const char *)out_resp + header_end) {
                int content_length = atoi(cl + 16);
                if (body_so_far >= content_length) break;
            } else {
                break;
            }
        }
    }

    ar_socket_close(fd);
    return total;
}
