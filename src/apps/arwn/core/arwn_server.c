/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arwn_server.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arwn_http.h"
#include "arwn_pack.h"
#include "aros_hal.h"

#define ARWN_HTTP_MAX_REQUEST  (64 * 1024)

/* Estados de uma conexão. */
typedef enum {
    CONN_READING,   /* lendo request (headers+body) */
    CONN_RESPONDING /* enviando resposta */
} arwn_conn_state_t;

typedef struct arwn_conn {
    int   fd;
    arwn_conn_state_t state;
    unsigned char rbuf[ARWN_SERVER_RECV_BUF];
    size_t rlen;
    uint64_t last_active_ms;
    int keep_alive;
    /* estado da resposta em curso */
    const unsigned char *resp_body;
    size_t resp_body_len;
    size_t resp_body_sent;
    char resp_head[1024];
    size_t resp_head_len;
    size_t resp_head_sent;
    struct arwn_conn *next;
} arwn_conn_t;

#define ARWN_SERVER_WORKERS 28
#define ARWN_SERVER_QUEUE_SIZE 16384

typedef struct {
    int fd;
} arwn_work_item_t;

struct arwn_server {
    int listening_fd;
    volatile int running;
    uint16_t port;
    arwn_route_t routes[ARWN_SERVER_MAX_ROUTES];
    int route_count;
    arwn_route_t notfound_route;
    int has_notfound_route;
    void *queue_mutex;
    void *queue_cond;
    arwn_work_item_t queue[ARWN_SERVER_QUEUE_SIZE];
    int queue_head;
    int queue_tail;
    int queue_count;
    void *workers[ARWN_SERVER_WORKERS];
};

static const char *cache_header(arwn_cache_t c) {
    switch (c) {
        case ARWN_CACHE_IMMUTABLE:
            return "public, max-age=31536000, immutable";
        case ARWN_CACHE_NO_CACHE:
            return "no-cache";
        default:
            return "no-store";
    }
}

arwn_server_t *arwn_server_create(void) {
    arwn_server_t *s = (arwn_server_t *)calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->listening_fd = -1;
    s->queue_mutex = ar_mutex_create();
    s->queue_cond = ar_cond_create();
    return s;
}

int arwn_server_set_notfound_route(arwn_server_t *s, const arwn_route_t *route) {
    if (!s || !route) return -1;
    s->notfound_route = *route;
    s->has_notfound_route = 1;
    return 0;
}

int arwn_server_add_route(arwn_server_t *s, const char *path,
                          const uint8_t *data, size_t size,
                          const char *content_type, arwn_cache_t cache) {
    if (!s || !path || !data || size == 0) return -1;
    /* Evita registrar rotas duplicadas com mesmo path exato */
    for (int i = 0; i < s->route_count; i++) {
        if (s->routes[i].path && strcmp(s->routes[i].path, path) == 0) {
            s->routes[i].data = data;
            s->routes[i].size = size;
            s->routes[i].content_type = content_type;
            s->routes[i].cache = cache;
            return 0;
        }
    }
    if (s->route_count >= ARWN_SERVER_MAX_ROUTES) return -1;
    arwn_route_t *r = &s->routes[s->route_count];
    r->path = path;
    r->data = data;
    r->size = size;
    r->content_type = content_type;
    r->cache = cache;
    s->route_count++;
    return 0;
}

int arwn_server_load_arweb(arwn_server_t *s, const arwn_unit_t *unit,
                           const uint8_t *arweb, size_t arweb_len) {
    if (!s || !unit || !arweb) return -1;
    arwn_pack_section_t views[ARWN_ARWEB_MAX_SECTIONS];
    int n = arwn_pack_index(arweb, arweb_len, views, ARWN_ARWEB_MAX_SECTIONS);
    if (n <= 0) return -1;

    char wasm_route[80];
    const char *route = unit->route[0] ? unit->route : "/";
    int is_404_unit = (strcmp(unit->name, "notfound") == 0 ||
                       strcmp(route, "/notfound") == 0 ||
                       strcmp(route, "/404") == 0 ||
                       strcmp(route, "404") == 0);

    for (int i = 0; i < n; i++) {
        if (strcmp(views[i].name, "app.html") == 0) {
            if (is_404_unit) {
                arwn_route_t nf;
                nf.path = "/404";
                nf.data = views[i].data;
                nf.size = views[i].size;
                nf.content_type = "text/html; charset=utf-8";
                nf.cache = ARWN_CACHE_NO_CACHE;
                arwn_server_set_notfound_route(s, &nf);
                arwn_server_add_route(s, "/notfound", views[i].data, views[i].size,
                                      "text/html; charset=utf-8",
                                      ARWN_CACHE_NO_CACHE);
                arwn_server_add_route(s, "/404", views[i].data, views[i].size,
                                      "text/html; charset=utf-8",
                                      ARWN_CACHE_NO_CACHE);
            } else {
                arwn_server_add_route(s, strdup(route), views[i].data, views[i].size,
                                      "text/html; charset=utf-8",
                                      ARWN_CACHE_NO_CACHE);
                if (strcmp(unit->name, "index") == 0 || strcmp(unit->name, "main") == 0) {
                    arwn_server_add_route(s, "/", views[i].data, views[i].size,
                                          "text/html; charset=utf-8",
                                          ARWN_CACHE_NO_CACHE);
                }
            }
        } else if (strcmp(views[i].name, "mod/main.wasm") == 0) {
            snprintf(wasm_route, sizeof(wasm_route), "/mod/%s.wasm", unit->name);
            arwn_server_add_route(s, "/mod/main.wasm", views[i].data,
                                  views[i].size, "application/wasm",
                                  ARWN_CACHE_IMMUTABLE);
            arwn_server_add_route(s, strdup(wasm_route), views[i].data, views[i].size,
                                  "application/wasm", ARWN_CACHE_IMMUTABLE);
        } else if (strcmp(views[i].name, "bundle.js") == 0 || strcmp(views[i].name, "main.js") == 0) {
            char unit_js[128];
            snprintf(unit_js, sizeof(unit_js), "/%s.js", unit->name);
            arwn_server_add_route(s, strdup(unit_js), views[i].data, views[i].size,
                                  "application/javascript; charset=utf-8", ARWN_CACHE_IMMUTABLE);
            arwn_server_add_route(s, "/bundle.js", views[i].data, views[i].size,
                                  "application/javascript; charset=utf-8", ARWN_CACHE_IMMUTABLE);
            arwn_server_add_route(s, "/main.js", views[i].data, views[i].size,
                                  "application/javascript; charset=utf-8", ARWN_CACHE_IMMUTABLE);
        } else if (strcmp(views[i].name, "main.css") == 0 || strcmp(views[i].name, "style.css") == 0) {
            char unit_css[128];
            snprintf(unit_css, sizeof(unit_css), "/%s.css", unit->name);
            arwn_server_add_route(s, strdup(unit_css), views[i].data, views[i].size,
                                  "text/css; charset=utf-8", ARWN_CACHE_IMMUTABLE);
            arwn_server_add_route(s, "/main.css", views[i].data, views[i].size,
                                  "text/css; charset=utf-8", ARWN_CACHE_IMMUTABLE);
            arwn_server_add_route(s, "/style.css", views[i].data, views[i].size,
                                  "text/css; charset=utf-8", ARWN_CACHE_IMMUTABLE);
        } else if (strcmp(views[i].name, "arwn-bridge.js") == 0) {
            /* bridge (F3): servida com hash estável p/ V8 code cache */
            arwn_server_add_route(s, "/arwn-bridge.js", views[i].data,
                                  views[i].size,
                                  "application/javascript; charset=utf-8",
                                  ARWN_CACHE_IMMUTABLE);
        } else if (strcmp(views[i].name, "config.arwn") != 0) {
            /* Demais arquivos estáticos empacotados (ex: robots.txt, sitemap.xml, favicon.ico) */
            char static_route[128];
            snprintf(static_route, sizeof(static_route), "/%s", views[i].name);
            const char *content_type = "application/octet-stream";
            size_t nlen = strlen(views[i].name);
            if (nlen > 4 && strcmp(views[i].name + nlen - 4, ".txt") == 0) {
                content_type = "text/plain; charset=utf-8";
            } else if (nlen > 4 && strcmp(views[i].name + nlen - 4, ".xml") == 0) {
                content_type = "application/xml; charset=utf-8";
            } else if (nlen > 4 && strcmp(views[i].name + nlen - 4, ".ico") == 0) {
                content_type = "image/x-icon";
            } else if (nlen > 4 && strcmp(views[i].name + nlen - 4, ".svg") == 0) {
                content_type = "image/svg+xml";
            } else if (nlen > 5 && strcmp(views[i].name + nlen - 5, ".json") == 0) {
                content_type = "application/json; charset=utf-8";
            } else if (nlen > 9 && strcmp(views[i].name + nlen - 9, ".manifest") == 0) {
                content_type = "application/manifest+json; charset=utf-8";
            }
            arwn_server_add_route(s, strdup(static_route), views[i].data,
                                  views[i].size, content_type,
                                  ARWN_CACHE_NO_CACHE);
        }
    }

    /* .arweb cru (F3): o browser busca via ARWN.load + verifica CRC */
    char arweb_route[ARWN_UNIT_MAX_NAME + 8];
    snprintf(arweb_route, sizeof(arweb_route), "/%s.arweb", unit->name);
    arwn_server_add_route(s, strdup(arweb_route), arweb, arweb_len,
                          "application/octet-stream", ARWN_CACHE_NO_CACHE);
    return 0;
}

int arwn_server_route_count(const arwn_server_t *s) {
    return s ? s->route_count : 0;
}

const arwn_route_t *arwn_server_route(const arwn_server_t *s, int idx) {
    if (!s || idx < 0 || idx >= s->route_count) return NULL;
    return &s->routes[idx];
}

int arwn_server_has_notfound_route(const arwn_server_t *s) {
    return (s && s->has_notfound_route);
}

static arwn_route_t *find_route(arwn_server_t *s, const char *path,
                                size_t path_len) {
    for (int i = 0; i < s->route_count; i++) {
        const char *rp = s->routes[i].path;
        size_t rpl = strlen(rp);
        if (rpl == path_len && memcmp(rp, path, path_len) == 0) {
            return &s->routes[i];
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Response primitiva                                                   */
/* ------------------------------------------------------------------ */

static size_t build_head(char *out, size_t cap, int status,
                         const char *status_text, const char *content_type,
                         size_t body_len, arwn_cache_t cache,
                         int keep_alive, const char *etag) {
    const char *ct = content_type ? content_type : "text/plain";
    char et_line[64] = "";
    if (etag && etag[0] != '\0') {
        snprintf(et_line, sizeof(et_line), "ETag: %s\r\n", etag);
    }
    int n = snprintf(out, cap,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Cache-Control: %s\r\n"
        "X-Content-Type-Options: nosniff\r\n"
        "X-Frame-Options: DENY\r\n"
        "Content-Security-Policy: default-src 'self' https: data: blob: 'unsafe-inline' 'unsafe-eval' 'wasm-unsafe-eval'; "
        "script-src 'self' 'unsafe-inline' 'unsafe-eval' 'wasm-unsafe-eval' https://unpkg.com https://cdn.jsdelivr.net https://cdnjs.cloudflare.com; "
        "style-src 'self' 'unsafe-inline' https://fonts.googleapis.com https://cdnjs.cloudflare.com; "
        "font-src 'self' https://fonts.gstatic.com https://cdnjs.cloudflare.com https://ka-f.fontawesome.com https://use.fontawesome.com data:; "
        "img-src 'self' data: https:; "
        "connect-src 'self' https:\r\n"
        "%s"
        "Connection: %s\r\n"
        "\r\n",
        status, status_text, ct,
        body_len, cache_header(cache),
        et_line,
        keep_alive ? "keep-alive" : "close");
    if (n < 0) n = 0;
    if ((size_t)n >= cap) n = (int)cap - 1;
    out[n] = '\0';
    return (size_t)n;
}

static int send_all(int fd, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    size_t sent = 0;
    while (sent < len) {
        int n = ar_socket_send(fd, p + sent, len - sent);
        if (n > 0) {
            sent += (size_t)n;
            continue;
        }
        if (n < 0 && (-n == EAGAIN || -n == EWOULDBLOCK || -n == EINTR)) {
            ar_sleep_ms(1);
            continue;
        }
        return -1;
    }
    return 0;
}

static int etag_of(const uint8_t *data, size_t len, char *out, size_t cap) {
    uint32_t crc = arwn_crc32(data, len);
    int n = snprintf(out, cap, "\"%08x\"", crc);
    if (n < 0 || (size_t)n >= cap) return -1;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Worker Connection Handler: Non-blocking I/O com Multi-Thread Pool    */
/* ------------------------------------------------------------------ */

static void handle_client_conn(arwn_server_t *s, int fd) {
    ar_socket_set_recv_timeout(fd, ARWN_SERVER_HEADER_TIMEOUT_MS);
    char buf[ARWN_HTTP_MAX_REQUEST];
    size_t total = 0;

    while (s->running) {
        int header_complete = 0;
        arwn_http_req_t req;

        while (total < sizeof(buf) - 1) {
            int n = ar_socket_recv(fd, buf + total, sizeof(buf) - 1 - total);
            if (n > 0) {
                total += (size_t)n;
                buf[total] = '\0';
                int rc = arwn_http_parse(&req, buf, total);
                if (rc == ARWN_HTTP_COMPLETE) {
                    header_complete = 1;
                    break;
                }
                if (rc == ARWN_HTTP_BAD) {
                    break;
                }
            } else if (n == 0) {
                /* Client closed connection */
                ar_socket_close(fd);
                return;
            } else {
                if (-n == EAGAIN || -n == EWOULDBLOCK) {
                    /* Timeout */
                    ar_socket_close(fd);
                    return;
                }
                ar_socket_close(fd);
                return;
            }
        }

        if (!header_complete) {
            char hbuf[1024];
            size_t hlen = build_head(hbuf, sizeof(hbuf), 400, "Bad Request",
                                     "text/plain", 11, ARWN_CACHE_NO_STORE, 0, NULL);
            send_all(fd, hbuf, hlen);
            send_all(fd, "bad request", 11);
            ar_socket_close(fd);
            return;
        }

        /* Check Keep-Alive: close by default on micro-services unless explicitly keep-alive */
        int keep_alive = 0;
        size_t cn_len = 0;
        const char *cn = arwn_http_header(&req, ARWN_HTTP_H_CONNECTION, &cn_len);
        if (cn && cn_len == 10 && memcmp(cn, "keep-alive", 10) == 0) {
            keep_alive = 1;
        }

        /* Method check */
        int is_head = arwn_http_method_is(&req, "HEAD");
        if (!arwn_http_method_is(&req, "GET") && !is_head) {
            char hbuf[1024];
            size_t hlen = build_head(hbuf, sizeof(hbuf), 405, "Method Not Allowed",
                                     "text/plain", 18, ARWN_CACHE_NO_STORE, keep_alive, NULL);
            send_all(fd, hbuf, hlen);
            send_all(fd, "method not allowed", 18);
            if (!keep_alive) { ar_socket_close(fd); return; }
            total = 0;
            continue;
        }

        /* Match Route */
        arwn_route_t *r = find_route(s, req.path, req.path_len);
        if (!r) {
            /* SPA Fallback: If route has no file extension (e.g. /restrict-area), serve root index.html */
            if (!memchr(req.path, '.', req.path_len)) {
                r = find_route(s, "/", 1);
                if (!r) r = find_route(s, "", 0);
                if (!r) r = find_route(s, "/index.html", 11);
                if (!r) {
                    for (int i = 0; i < s->route_count; i++) {
                        if (s->routes[i].content_type && strstr(s->routes[i].content_type, "text/html") != NULL) {
                            r = &s->routes[i];
                            break;
                        }
                    }
                }
            }
        }
        if (!r) {
            if (s->has_notfound_route && s->notfound_route.data) {
                char hbuf[1024];
                size_t hlen = build_head(hbuf, sizeof(hbuf), 404, "Not Found",
                                         s->notfound_route.content_type,
                                         s->notfound_route.size,
                                         s->notfound_route.cache, keep_alive, NULL);
                if (send_all(fd, hbuf, hlen) < 0) { ar_socket_close(fd); return; }
                if (!is_head && s->notfound_route.size > 0) {
                    if (send_all(fd, s->notfound_route.data, s->notfound_route.size) < 0) {
                        ar_socket_close(fd);
                        return;
                    }
                }
            } else {
                char hbuf[1024];
                size_t hlen = build_head(hbuf, sizeof(hbuf), 404, "Not Found",
                                         "text/plain", 9, ARWN_CACHE_NO_STORE, keep_alive, NULL);
                send_all(fd, hbuf, hlen);
                send_all(fd, "not found", 9);
            }
            if (!keep_alive) { ar_socket_close(fd); return; }
            total = 0;
            continue;
        }

        /* ETag match for 304 */
        char etag[16];
        int has_etag = (r->cache == ARWN_CACHE_NO_CACHE && etag_of(r->data, r->size, etag, sizeof(etag)) == 0);
        int not_modified = 0;

        if (has_etag) {
            for (int k = 0; k < req.header_count; k++) {
                if (req.headers[k].name_len == 13 &&
                    memcmp(req.headers[k].name, "If-None-Match", 13) == 0) {
                    if (req.headers[k].value_len == strlen(etag) &&
                        memcmp(req.headers[k].value, etag, strlen(etag)) == 0) {
                        not_modified = 1;
                        break;
                    }
                }
            }
        }

        char hbuf[1024];
        if (not_modified) {
            size_t hlen = build_head(hbuf, sizeof(hbuf), 304, "Not Modified",
                                     "text/plain", 0, ARWN_CACHE_NO_CACHE,
                                     keep_alive, etag);
            if (send_all(fd, hbuf, hlen) < 0) { ar_socket_close(fd); return; }
        } else {
            size_t hlen = build_head(hbuf, sizeof(hbuf), 200, "OK",
                                     r->content_type, r->size,
                                     r->cache, keep_alive, has_etag ? etag : NULL);
            if (send_all(fd, hbuf, hlen) < 0) { ar_socket_close(fd); return; }
            if (!is_head && r->size > 0) {
                if (send_all(fd, r->data, r->size) < 0) { ar_socket_close(fd); return; }
            }
        }

        if (!keep_alive) {
            ar_socket_close(fd);
            return;
        }

        /* Reset buffer for next request over Keep-Alive */
        total = 0;
        ar_socket_set_recv_timeout(fd, 2000);
    }

    ar_socket_close(fd);
}

static void *worker_thread_main(void *arg) {
    arwn_server_t *s = (arwn_server_t *)arg;

    while (s->running) {
        ar_mutex_lock(s->queue_mutex);

        while (s->running && s->queue_count == 0) {
            ar_cond_wait(s->queue_cond, s->queue_mutex);
        }

        if (!s->running) {
            ar_mutex_unlock(s->queue_mutex);
            break;
        }

        int cfd = s->queue[s->queue_head].fd;
        s->queue_head = (s->queue_head + 1) % ARWN_SERVER_QUEUE_SIZE;
        s->queue_count--;

        ar_mutex_unlock(s->queue_mutex);

        handle_client_conn(s, cfd);
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Event loop & Dispatcher                                            */
/* ------------------------------------------------------------------ */

void arwn_server_stop(arwn_server_t *s) {
    if (!s) return;
    s->running = 0;
    ar_mutex_lock(s->queue_mutex);
    ar_cond_signal(s->queue_cond);
    ar_mutex_unlock(s->queue_mutex);
}

int arwn_server_run(arwn_server_t *s, const char *bind, uint16_t port) {
    if (!s) return -1;
    s->port = port;

    int fd = ar_socket_create(1);
    if (fd < 0) {
        fprintf(stderr, "[arwn] ar_socket_create failed (code %d)\n", fd);
        return -1;
    }
    ar_socket_reuseaddr(fd, 1);
    int brc = ar_socket_bind(fd, bind, port);
    if (brc < 0) {
        fprintf(stderr, "[arwn] ar_socket_bind to %s:%u failed (code %d)\n", bind, port, brc);
        ar_socket_close(fd);
        return -1;
    }
    int lrc = ar_socket_listen(fd, ARWN_SERVER_LISTEN_BACKLOG);
    if (lrc < 0) {
        fprintf(stderr, "[arwn] ar_socket_listen on port %u failed (code %d)\n", port, lrc);
        ar_socket_close(fd);
        return -1;
    }
    s->listening_fd = fd;
    s->running = 1;

    /* Inicia a Thread Pool de Alta Concorrência (Multi-Worker Pool) */
    for (int i = 0; i < ARWN_SERVER_WORKERS; i++) {
        s->workers[i] = ar_thread_create(worker_thread_main, s);
        if (s->workers[i]) {
            ar_thread_detach(s->workers[i]);
        }
    }

    printf("[arwn] multi-threaded server listening on %s:%u (%d routes, %d worker threads)\n",
           bind, port, s->route_count, ARWN_SERVER_WORKERS);

    while (s->running) {
        int cfd = ar_socket_accept(fd);
        if (cfd < 0) {
            ar_sleep_ms(1);
            continue;
        }

        ar_mutex_lock(s->queue_mutex);
        if (s->queue_count < ARWN_SERVER_QUEUE_SIZE) {
            s->queue[s->queue_tail].fd = cfd;
            s->queue_tail = (s->queue_tail + 1) % ARWN_SERVER_QUEUE_SIZE;
            s->queue_count++;
            ar_cond_signal(s->queue_cond);
            ar_mutex_unlock(s->queue_mutex);
        } else {
            /* Fila cheia (Anti-DoS / Load Shedding) */
            ar_mutex_unlock(s->queue_mutex);
            char hbuf[256];
            size_t hlen = build_head(hbuf, sizeof(hbuf), 503, "Service Unavailable",
                                     "text/plain", 19, ARWN_CACHE_NO_STORE, 0, NULL);
            ar_socket_send(cfd, hbuf, hlen);
            ar_socket_send(cfd, "service unavailable", 19);
            ar_socket_close(cfd);
        }
    }

    /* Shutdown */
    if (s->listening_fd >= 0) {
        ar_socket_close(s->listening_fd);
        s->listening_fd = -1;
    }
    return 0;
}

void arwn_server_free(arwn_server_t *s) {
    if (!s) return;
    arwn_server_stop(s);
    if (s->queue_mutex) ar_mutex_destroy(s->queue_mutex);
    if (s->queue_cond) ar_cond_destroy(s->queue_cond);
    free(s);
}