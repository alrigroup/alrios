/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arws_gateway.h"
#include "arws_utils.h"
#include "arws_cache.h"
#include "arws_config.h"
#include "arws_proxy.h"
#include "arws_stream_proxy.h"
#include "log.h"
#include "aros_hal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static int is_static_path(const char *path) {
    if (!path) return 0;
    const char *dot = strrchr(path, '.');
    if (!dot) return 0;
    const char *ext = dot + 1;
    if (strcasecmp(ext, "css") == 0) return 1;
    if (strcasecmp(ext, "js") == 0) return 1;
    if (strcasecmp(ext, "png") == 0) return 1;
    if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0) return 1;
    if (strcasecmp(ext, "gif") == 0) return 1;
    if (strcasecmp(ext, "svg") == 0) return 1;
    if (strcasecmp(ext, "ico") == 0) return 1;
    if (strcasecmp(ext, "woff") == 0 || strcasecmp(ext, "woff2") == 0) return 1;
    if (strcasecmp(ext, "ttf") == 0) return 1;
    if (strcasecmp(ext, "webp") == 0) return 1;
    if (strcasecmp(ext, "pdf") == 0) return 1;
    return 0;
}

static int header_name_eq(const char *p, const char *name, int nlen) {
    for (int i = 0; i < nlen; i++) {
        char c = p[i], a = name[i];
        if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
        if (a >= 'a' && a <= 'z') a -= 'a' - 'A';
        if (c != a) return 0;
    }
    return 1;
}

/* True when the response carries a Set-Cookie header. Responses that vary
   per user (or set cookies) must not be cached/replayed to other clients. */
static int response_has_set_cookie(const unsigned char *buf, int len) {
    if (!buf || len <= 0) return 0;
    int hlen = len;
    for (int i = 0; i + 3 < len; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n' &&
            buf[i + 2] == '\r' && buf[i + 3] == '\n') {
            hlen = i;
            break;
        }
    }
    for (int i = 0; i + 10 < hlen; i++) {
        if (header_name_eq((const char *)buf + i, "Set-Cookie", 10) == 0)
            return 1;
    }
    return 0;
}

int arws_dispatch(ClientConnection *conn, HttpRequest *req, const char *effective_mode) {
    ArwsRoute route;
    alri_print(CYN "[ARWS]" RST " dispatch: effective_mode=%s\n", effective_mode);
    int result = arws_route_match(conn, req, effective_mode, &route);
    alri_print(CYN "[ARWS]" RST " dispatch: route_match result=%d\n", result);

    if (result == 0) {
        alri_print(CYN "[ARWS]" RST " dispatch: 404\n");
        arws_send_404(conn);
        return 0;
    }

    if (result == 2 && route.use_handler) {
        alri_print(CYN "[ARWS]" RST " dispatch: handler\n");
        route.handler(conn, req);
        return 0;
    }

    if (result == 1 && route.use_redirect && route.redirect_target[0] != '\0') {
        alri_print(CYN "[ARWS]" RST " dispatch: redirect -> %s\n", route.redirect_target);
        arws_send_302(conn, route.redirect_target);
        return 0;
    }

    if (result == 1 && route.backend_id >= 0) {
        alri_print(CYN "[ARWS]" RST " dispatch: backend_id=%d\n", route.backend_id);
        unsigned char raw_buf[32768];
        int raw_len = arws_build_http_request(conn, req, raw_buf, sizeof(raw_buf));
        alri_print(CYN "[ARWS]" RST " dispatch: raw_len=%d\n", raw_len);
        if (raw_len <= 0) {
            arws_send_502(conn, "Bad Gateway");
            return -1;
        }

        int is_get = (strcmp(req->method, "GET") == 0);
        int is_static = is_get && is_static_path(req->path);
        int has_no_cache = arws_config_is_no_cache(req->host, req->path);
        int use_cache = 0;
        if (is_get && !has_no_cache) {
            if (is_static) {
                use_cache = 1;
            } else if (arws_config_get_cache_ttl() > 0) {
                use_cache = 1;
            }
        }

        if (use_cache) {
            char cache_key[512];
            arws_cache_make_key(cache_key, sizeof(cache_key),
                                req->method, req->host, req->path,
                                req->query_params);
            unsigned char *cached_data = NULL;
            int cached_len = 0;
            if (arws_cache_get(cache_key, &cached_data, &cached_len) == 0) {
                alri_print(CYN "[ARWS]" RST " dispatch: cache HIT %s\n", cache_key);
                server_conn_write(conn, cached_data, cached_len);
                ar_mem_free(cached_data);
                return 0;
            }
        }

        arws_dispatch_lock(route.backend_id);

        alri_print(CYN "[ARWS]" RST " dispatch: sending to backend...\n");
        if (arws_backend_send_request(route.backend_id, raw_buf, raw_len) < 0) {
            arws_dispatch_unlock(route.backend_id);
            arws_send_502(conn, "Backend unavailable");
            return -1;
        }

        alri_print(CYN "[ARWS]" RST " dispatch: reading response...\n");
        unsigned char resp_buf[65536];
        int resp_len = arws_backend_read_response(route.backend_id,
                                                   resp_buf, sizeof(resp_buf));
        arws_dispatch_unlock(route.backend_id);

        alri_print(CYN "[ARWS]" RST " dispatch: resp_len=%d\n", resp_len);
        if (resp_len <= 0) {
            alri_print(CYN "[ARWS]" RST " dispatch: backend error\n");
            arws_send_502(conn, "Backend error");
            return -1;
        }

        if (use_cache) {
            char cache_key[512];
            arws_cache_make_key(cache_key, sizeof(cache_key),
                                req->method, req->host, req->path,
                                req->query_params);
            if (resp_len >= 9 && strncmp((const char *)resp_buf, "HTTP/1.1 ", 9) == 0 &&
                resp_buf[9] == '2' && !response_has_set_cookie(resp_buf, resp_len)) {
                arws_cache_set(cache_key, resp_buf, resp_len);
            }
        }

        server_conn_write(conn, resp_buf, resp_len);
        alri_print(CYN "[ARWS]" RST " dispatch: done\n");
        return 0;
    }

    if (result == 1 && route.use_stream && route.proxy_target[0] != '\0') {
        alri_print(CYN "[ARWS]" RST " dispatch: stream proxy -> %s\n", route.proxy_target);
        arws_stream_proxy_forward(conn, req, route.proxy_target);
        return 0;
    }

    if (result == 1 && !route.use_stream && route.proxy_target[0] != '\0') {
        alri_print(CYN "[ARWS]" RST " dispatch: proxy (stream) -> %s\n", route.proxy_target);
        arws_stream_proxy_forward(conn, req, route.proxy_target);
        return 0;
    }

    alri_print(CYN "[ARWS]" RST " dispatch: fallback 404\n");
    arws_send_404(conn);
    return 0;
}
