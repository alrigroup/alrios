/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "aros_hal.h"
#include "home_os.h"
#include "ar_ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define APP_NAME      "projetoliteratura-web"
#define GATEWAY_HOST  "127.0.0.1"
#define GATEWAY_PORT  9500
#define MAX_ATTEMPTS  30
#define MAX_REQ       65536
#define MAX_FILES     8

static int server_port = 3003;

typedef struct {
    const char *route_path;
    const char *file;
    const char *content_type;
    char       *data;
    int         len;
} StaticFile;

static StaticFile g_files[MAX_FILES];
static int g_file_count = 0;

static char *read_file(const char *path, int *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz <= 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *data = (char *)ar_mem_alloc((size_t)sz + 1);
    if (!data) { fclose(f); return NULL; }
    size_t got = fread(data, 1, (size_t)sz, f);
    fclose(f);
    data[got] = '\0';
    if (out_len) *out_len = (int)got;
    return data;
}

static int add_file(const char *dir, const char *route, const char *file, const char *ctype) {
    if (g_file_count >= MAX_FILES) return -1;
    StaticFile *sf = &g_files[g_file_count];
    char path[1400];
    snprintf(path, sizeof(path), "%s%c%s", dir,
#ifdef _WIN32
             '\\',
#else
             '/',
#endif
             file);
    char *data = read_file(path, &sf->len);
    if (!data) {
        printf("[%s] ERROR: cannot load %s\n", APP_NAME, path);
        return -1;
    }
    printf("[%s] loaded %s (%d bytes)\n", APP_NAME, path, sf->len);
    sf->route_path = route;
    sf->file = file;
    sf->content_type = ctype;
    sf->data = data;
    g_file_count++;
    return 0;
}

static int load_files(void) {
    char dir[1024];
    int loaded = 0;

    if (home_os_get_exe_dir(dir, sizeof(dir)) == 0) {
        if (add_file(dir, "/projetoliteratura", "projetoliteratura.html", "text/html; charset=utf-8") == 0) loaded++;
        if (add_file(dir, "/projetoliteratura/", "projetoliteratura.html", "text/html; charset=utf-8") == 0) loaded++;
        if (add_file(dir, "/projetoliteratura/style.css", "projetoliteratura/style.css", "text/css; charset=utf-8") == 0) loaded++;
        if (add_file(dir, "/projetoliteratura/images/machado-1905.png", "projetoliteratura/images/machado-1905.png", "image/png") == 0) loaded++;
        if (add_file(dir, "/projetoliteratura/images/machado-abl.jpg", "projetoliteratura/images/machado-abl.jpg", "image/jpeg") == 0) loaded++;
        if (loaded > 0) return 0;
    }

    if (add_file(".", "/projetoliteratura", "projetoliteratura.html", "text/html; charset=utf-8") == 0) loaded++;
    if (add_file(".", "/projetoliteratura/", "projetoliteratura.html", "text/html; charset=utf-8") == 0) loaded++;
    if (add_file(".", "/projetoliteratura/style.css", "projetoliteratura/style.css", "text/css; charset=utf-8") == 0) loaded++;
    if (add_file(".", "/projetoliteratura/images/machado-1905.png", "projetoliteratura/images/machado-1905.png", "image/png") == 0) loaded++;
    if (add_file(".", "/projetoliteratura/images/machado-abl.jpg", "projetoliteratura/images/machado-abl.jpg", "image/jpeg") == 0) loaded++;

    if (loaded == 0) {
        fprintf(stderr, "[%s] ERROR: files not found next to executable or in cwd\n", APP_NAME);
        return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* IPC frames (gateway protocol, port 9500)                            */
/* ------------------------------------------------------------------ */

static int ipc_send_frame(int fd, unsigned char type, const char *data, int len) {
    unsigned char header[5];
    header[0] = (unsigned char)((len >> 24) & 0xFF);
    header[1] = (unsigned char)((len >> 16) & 0xFF);
    header[2] = (unsigned char)((len >> 8) & 0xFF);
    header[3] = (unsigned char)(len & 0xFF);
    header[4] = type;

    if (ar_socket_send(fd, header, 5) != 5) return -1;
    if (len > 0 && data) {
        int written = 0;
        while (written < len) {
            int n = ar_socket_send(fd, data + written, (size_t)(len - written));
            if (n <= 0) return -1;
            written += n;
        }
    }
    return 0;
}

static int ipc_recv_frame(int fd, unsigned char *type, char *buf, int buflen) {
    unsigned char header[5];
    int got = 0;
    while (got < 5) {
        int n = ar_socket_recv(fd, header + got, (size_t)(5 - got));
        if (n <= 0) return -1;
        got += n;
    }

    int len = ((int)header[0] << 24) | ((int)header[1] << 16) |
              ((int)header[2] << 8) | (int)header[3];
    *type = header[4];

    if (len > 0) {
        if (len > buflen - 1) len = buflen - 1;
        got = 0;
        while (got < len) {
            int n = ar_socket_recv(fd, buf + got, (size_t)(len - got));
            if (n <= 0) return -1;
            got += n;
        }
        buf[got] = '\0';
    } else {
        buf[0] = '\0';
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Gateway route registration (proxy mode)                             */
/* ------------------------------------------------------------------ */

static int register_route(int fd, const char *prefix, const char *host, const char *mode) {
    char payload[512];
    int len = snprintf(payload, sizeof(payload),
                       "%s %s %s %s %s proxy=http://%s:%d type=proxy",
                       APP_NAME, prefix, "GET", host, mode,
                       GATEWAY_HOST, server_port);
    if (len <= 0 || len >= (int)sizeof(payload)) return -1;

    if (ipc_send_frame(fd, 1, payload, len + 1) != 0) return -1;

    unsigned char type = 0;
    char ack[128];
    if (ipc_recv_frame(fd, &type, ack, sizeof(ack)) != 0) return -1;

    printf("[%s] Registered GET %s host=%s mode=%s (%s)\n", APP_NAME, prefix, host, mode, ack);
    return 0;
}

/* Connect to gateway, register routes, keep the connection open as the
   control/query channel. Returns the fd, or -1 on failure. */
static int connect_and_register(void) {
    int fd = ar_socket_create(1);
    if (fd < 0) return -1;

    if (home_os_connect_timeout(fd, GATEWAY_HOST, GATEWAY_PORT, 1000) != 0) {
        ar_socket_close(fd);
        return -1;
    }

    const char *hosts[]  = { "alexsander.alrigroup.com", "alexsander.localhost" };
    const char *routes[] = { "/projetoliteratura", "/projetoliteratura/", "/projetoliteratura/style.css",
                             "/projetoliteratura/images/machado-1905.png", "/projetoliteratura/images/machado-abl.jpg" };
    const char *mode     = "production";

    int ok = 0;
    for (int h = 0; h < 2 && !ok; h++) {
        for (int r = 0; r < 5; r++) {
            if (register_route(fd, routes[r], hosts[h], mode) != 0) {
                ok = 1;
                break;
            }
        }
    }

    if (ok) {
        ar_socket_close(fd);
        return -1;
    }
    return fd;
}

static int build_routes_text(char *out, int size) {
    const char *hosts[]  = { "alexsander.alrigroup.com", "alexsander.localhost" };
    const char *routes[] = { "/projetoliteratura", "/projetoliteratura/", "/projetoliteratura/style.css",
                             "/projetoliteratura/images/machado-1905.png", "/projetoliteratura/images/machado-abl.jpg" };
    const char *mode     = "production";
    int used = 0;
    for (int h = 0; h < 2; h++) {
        for (int r = 0; r < 5; r++) {
            int n = snprintf(out + used, size - used,
                             "GET %-24s host=%-28s mode=%s proxy=http://%s:%d\n",
                             routes[r], hosts[h], mode, GATEWAY_HOST, server_port);
            if (n < 0 || used + n >= size) break;
            used += n;
        }
    }
    return used;
}

static void handle_query(int fd, const char *q, int len) {
    char cmd[128] = {0};
    int i = 0;
    while (i < len && i < 127 && q[i] != ' ' && q[i] != '\t' && q[i] != '\n') {
        cmd[i] = q[i];
        i++;
    }

    char resp[2048];
    int rlen = 0;
    if (strcmp(cmd, "ping") == 0) {
        rlen = snprintf(resp, sizeof(resp), "pong");
    } else if (strcmp(cmd, "status") == 0) {
        rlen = snprintf(resp, sizeof(resp), "%s RUNNING port=%d", APP_NAME, server_port);
    } else if (strcmp(cmd, "routes") == 0) {
        rlen = build_routes_text(resp, sizeof(resp));
    } else {
        rlen = snprintf(resp, sizeof(resp), "unknown command: %s", cmd);
    }
    if (rlen < 0) rlen = 0;
    if (rlen >= (int)sizeof(resp)) rlen = (int)sizeof(resp) - 1;

    ipc_send_frame(fd, IPC_QUERY_RESP, resp, rlen + 1);
}

static void query_loop(int fd) {
    home_os_set_recv_timeout(fd, 800);

    char buf[MAX_REQ];
    unsigned char type = 0;
    int idle = 0;

    while (1) {
        int r = ipc_recv_frame(fd, &type, buf, sizeof(buf));
        if (r == 0) {
            idle = 0;
            if (type == IPC_QUERY) {
                handle_query(fd, buf, (int)strlen(buf));
            }
            continue;
        }

        idle++;
        if (idle > 60) break;

        if (ipc_send_frame(fd, IPC_HEARTBEAT, NULL, 0) < 0) break;
    }

    ar_socket_close(fd);
    printf("[%s] control channel closed\n", APP_NAME);
}

/* ------------------------------------------------------------------ */
/* HTTP server (serves the SPA)                                        */
/* ------------------------------------------------------------------ */

static void send_all(int c, const char *data, int len) {
    int written = 0;
    while (written < len) {
        int n = ar_socket_send(c, data + written, (size_t)(len - written));
        if (n <= 0) return;
        written += n;
    }
}

static void send_response(int c, int status, const char *content_type, const char *body, int body_len) {
    char header[512];
    if (body_len < 0) body_len = body ? (int)strlen(body) : 0;
    const char *status_text = (status == 200) ? "OK" : (status == 500) ? "Internal Server Error" : "Not Found";

    int len = snprintf(header, sizeof(header),
                       "HTTP/1.1 %d %s\r\n"
                       "Content-Type: %s\r\n"
                       "Content-Length: %d\r\n"
                       "Cache-Control: no-store\r\n"
                       "Connection: close\r\n\r\n",
                       status, status_text, content_type, body_len);
    if (len > 0) send_all(c, header, len);
    if (body_len > 0) send_all(c, body, body_len);
}

static StaticFile *find_file(const char *path) {
    for (int i = 0; i < g_file_count; i++) {
        if (g_files[i].route_path && strcmp(g_files[i].route_path, path) == 0) {
            return &g_files[i];
        }
    }
    return NULL;
}

static void serve(int c, const char *path) {
    char clean[512];
    snprintf(clean, sizeof(clean), "%s", path);
    char *q = strchr(clean, '?');
    if (q) *q = '\0';
    char *h = strchr(clean, '#');
    if (h) *h = '\0';

    StaticFile *sf = find_file(clean);
    if (sf && sf->data) {
        send_response(c, 200, sf->content_type, sf->data, sf->len);
    } else {
        send_response(c, 404, "text/plain; charset=utf-8", "Not Found", -1);
    }
}

static void handle_client(int c) {
    home_os_set_recv_timeout(c, 5000);

    char buf[MAX_REQ];
    int total = 0;
    int header_end = -1;

    while (total < MAX_REQ - 1) {
        int n = ar_socket_recv(c, buf + total, (size_t)(MAX_REQ - 1 - total));
        if (n <= 0) break;
        total += n;
        buf[total] = '\0';

        char *end = strstr(buf, "\r\n\r\n");
        if (end) {
            header_end = (int)(end - buf) + 4;
            break;
        }
    }

    if (header_end < 0) {
        ar_socket_close(c);
        return;
    }

    char method[16] = {0};
    char path[512] = {0};
    sscanf(buf, "%15s %511s", method, path);

    serve(c, path);
    ar_socket_close(c);
}

static void *client_thread(void *arg) {
    int c = (int)(intptr_t)arg;
    handle_client(c);
    return NULL;
}

static int create_server(int port) {
    int s = ar_socket_create(1);
    if (s < 0) {
        printf("[%s] cannot create socket\n", APP_NAME);
        return -1;
    }

    ar_socket_reuseaddr(s, 1);

    if (ar_socket_bind(s, "127.0.0.1", (uint16_t)port) < 0) {
        printf("[%s] cannot bind 127.0.0.1:%d\n", APP_NAME, port);
        ar_socket_close(s);
        return -1;
    }

    if (ar_socket_listen(s, 64) < 0) {
        printf("[%s] listen failed\n", APP_NAME);
        ar_socket_close(s);
        return -1;
    }

    return s;
}

static void run_server(int srv) {
    while (1) {
        int c = ar_socket_accept(srv);
        if (c < 0) continue;

        void *worker = ar_thread_create(client_thread, (void *)(intptr_t)c);
        if (worker) {
            ar_thread_detach(worker);
        } else {
            ar_socket_close(c);
        }
    }
}

static void *register_thread(void *arg) {
    (void)arg;
    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        int fd = connect_and_register();
        if (fd >= 0) {
            printf("[%s] routes registered + control channel open (attempt %d)\n",
                   APP_NAME, attempt);
            query_loop(fd);
            printf("[%s] control channel lost, reconnecting...\n", APP_NAME);
            ar_sleep_ms(1000);
            continue;
        }
        printf("[%s] register retry %d/%d\n", APP_NAME, attempt, MAX_ATTEMPTS);
        ar_sleep_ms(1000);
    }
    fprintf(stderr, "[%s] failed to register routes after %d attempts\n",
            APP_NAME, MAX_ATTEMPTS);
    return NULL;
}

/* ------------------------------------------------------------------ */

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--port=", 7) == 0) {
            server_port = atoi(argv[i] + 7);
            if (server_port <= 0) server_port = 3003;
        }
    }

    if (load_files() != 0) return 1;

    int srv = create_server(server_port);
    if (srv < 0) return 1;

    printf("[%s] SPA server listening on 127.0.0.1:%d\n", APP_NAME, server_port);

    void *rt = ar_thread_create(register_thread, NULL);
    if (rt) ar_thread_detach(rt);

    run_server(srv);
    return 0;
}
