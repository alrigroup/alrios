/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "server.h"
#include "cJSON.h"
#include "log.h"
#include "aros_hal.h"
#include "arws_config.h"
#include "arws_utils.h"
#include "arws_ratelimit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#ifndef _WIN32
#include <strings.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef __linux__
#include <sys/sendfile.h>
#endif
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <BaseTsd.h>
#ifdef _MSC_VER
typedef SSIZE_T ssize_t;
#endif
#endif
#include <time.h>
#include <stdarg.h>
#include <errno.h>
#include <openssl/rand.h>

#define MAX_CONN 65536
#define MAX_POLL_EVENTS 1024
#define ARWS_READ_TIMEOUT_MS 10000

typedef struct {
    int fd;
    int in_use;
    int mode;
    void *ctx;
    char client_ip[64];
} EConn;

static EConn *econn_pool = NULL;
static void *econn_mutex = NULL;
static ArPoll *ar_poll = NULL;

static EConn* econn_alloc(void) {
    if (!econn_mutex) econn_mutex = ar_mutex_create();
    ar_mutex_lock(econn_mutex);
    if (!econn_pool) {
        econn_pool = (EConn *)calloc(MAX_CONN, sizeof(EConn));
    }
    if (econn_pool) {
        for (int i = 0; i < MAX_CONN; i++) {
            if (!econn_pool[i].in_use) {
                econn_pool[i].in_use = 1;
                econn_pool[i].fd = -1;
                ar_mutex_unlock(econn_mutex);
                return &econn_pool[i];
            }
        }
    }
    ar_mutex_unlock(econn_mutex);
    return NULL;
}

static void econn_free(EConn *ec) {
    if (!ec) return;
    ar_mutex_lock(econn_mutex);
    ec->in_use = 0;
    ar_mutex_unlock(econn_mutex);
}

static volatile int http_server_running = 0;
static int http_server_sock = -1;

#define MAX_CONNECTIONS 65536
static int active_connections = 0;
static void *conn_limit_mutex = NULL;

struct ClientConnection {
    int socket_fd;
    void *ssl;
    int mode;
    char client_ip[64];
    char current_path[256];
    char response_headers[1024];
    char anon_id[65];
};

static RequestHandler global_api_handler = NULL;
static LoggerCallback global_logger = NULL;
static const char *global_bind_address = "0.0.0.0";

void server_set_bind_address(const char *addr) {
    if (addr) global_bind_address = addr;
}

static void url_decode(char *dst, const char *src) {
    if (!dst || !src) return;
    char a, b;
    while (*src) {
        if ((*src == '%') && ((a = src[1]) && (b = src[2])) && (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a') a -= 32;
            if (b >= 'a') b -= 32;
            char val = ((a >= 'A' ? a - 'A' + 10 : a - '0') << 4) | (b >= 'A' ? b - 'A' + 10 : b - '0');
            if (val == '\0') {
                /* Replace null byte with unprintable character 0x01 to trigger Bad Request */
                *dst++ = '\x01';
            } else {
                *dst++ = val;
            }
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

/* Trusted proxy validation (IPv4). Headers like X-Forwarded-For and
   CF-Connecting-IP are only honored when the real peer (accept) is inside
   the CIDR list from ARWS_TRUSTED_PROXY. Default: trust nothing. */
#define ARWS_MAX_TRUSTED_PROXIES 32

typedef struct {
    struct in_addr net;
    int prefix;
} TrustedNet;

static TrustedNet g_trusted_proxies[ARWS_MAX_TRUSTED_PROXIES];
static int g_trusted_proxy_count = -1;

static void trusted_proxies_parse(void) {
    g_trusted_proxy_count = 0;
    const char *env = getenv("ARWS_TRUSTED_PROXY");
    if (!env || !env[0]) return;

    char list[2048];
    strncpy(list, env, sizeof(list) - 1);
    list[sizeof(list) - 1] = '\0';

    char *tok = strtok(list, ",");
    while (tok && g_trusted_proxy_count < ARWS_MAX_TRUSTED_PROXIES) {
        char *t = tok;
        while (*t == ' ' || *t == '\t') t++;
        char *e = t + strlen(t);
        while (e > t && (e[-1] == ' ' || e[-1] == '\t')) *--e = '\0';

        if (t[0]) {
            int prefix = 32;
            char host[128];
            strncpy(host, t, sizeof(host) - 1);
            host[sizeof(host) - 1] = '\0';
            char *slash = strchr(host, '/');
            if (slash) {
                *slash = '\0';
                prefix = atoi(slash + 1);
            }
            struct in_addr net;
            if (inet_pton(AF_INET, host, &net) == 1 && prefix >= 0 && prefix <= 32) {
                g_trusted_proxies[g_trusted_proxy_count].net = net;
                g_trusted_proxies[g_trusted_proxy_count].prefix = prefix;
                g_trusted_proxy_count++;
            }
        }
        tok = strtok(NULL, ",");
    }
}

static int ip_is_trusted_proxy(const char *peer_ip) {
    if (!peer_ip || !peer_ip[0]) return 0;
    if (g_trusted_proxy_count < 0) trusted_proxies_parse();

    struct in_addr a;
    if (inet_pton(AF_INET, peer_ip, &a) != 1) return 0;
    uint32_t ip = ntohl(a.s_addr);

    for (int i = 0; i < g_trusted_proxy_count; i++) {
        uint32_t mask = (g_trusted_proxies[i].prefix == 0)
                            ? 0
                            : (0xFFFFFFFFu << (32 - g_trusted_proxies[i].prefix));
        uint32_t net = ntohl(g_trusted_proxies[i].net.s_addr);
        if ((ip & mask) == (net & mask)) return 1;
    }
    return 0;
}

static int ip_looks_valid(const char *ip) {
    if (!ip || !ip[0]) return 0;
    struct in_addr a;
    return inet_pton(AF_INET, ip, &a) == 1;
}

static void track_access(const char *ip, const char *path, int status, const char *anon_id) {
    if (global_logger) {
        global_logger(ip, path, status, anon_id);
    }
}

int server_conn_write(ClientConnection *conn, const void *buf, int num) {
    if (conn->mode == MODE_SECURE && conn->ssl != NULL) {
        return ar_ssl_write(conn->ssl, buf, num);
    } else {
        return ar_socket_send(conn->socket_fd, buf, num);
    }
}

int server_conn_read(ClientConnection *conn, void *buf, int num) {
    if (conn->mode == MODE_SECURE && conn->ssl != NULL) {
        return ar_ssl_read(conn->ssl, buf, num);
    } else {
        return ar_socket_recv(conn->socket_fd, buf, num);
    }
}

int server_conn_get_fd(ClientConnection *conn) {
    if (!conn) return -1;
    return conn->socket_fd;
}

void server_conn_close(ClientConnection *conn) {
    if (!conn) return;
    ar_socket_close(conn->socket_fd);
}

void server_send_response(ClientConnection *conn, int status, const char *content_type, const char *body) {
    char headers[2048];
    int body_len = body ? strlen(body) : 0;

    const char *status_text = "OK";
    if (status == 404) status_text = "Not Found";
    else if (status == 500) status_text = "Internal Server Error";

    snprintf(headers, sizeof(headers),
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %d\r\n"
             "%s"
             "Connection: close\r\n\r\n",
             status, status_text, content_type, body_len, conn->response_headers);

    server_conn_write(conn, headers, strlen(headers));
    if (body) {
        server_conn_write(conn, body, body_len);
    }
    track_access(conn->client_ip, conn->current_path, status, conn->anon_id);
}

void server_add_header(ClientConnection *conn, const char *header_line) {
    if (!conn || !header_line) return;
    char sanitized[1024];
    int si = 0;
    for (int i = 0; header_line[i] != '\0' && si < (int)sizeof(sanitized) - 2; i++) {
        if (header_line[i] != '\r' && header_line[i] != '\n') {
            sanitized[si++] = header_line[i];
        }
    }
    sanitized[si] = '\0';
    int current_len = strlen(conn->response_headers);
    int line_len = strlen(sanitized);
    if (current_len + line_len + 2 < (int)sizeof(conn->response_headers) - 1) {
        strcat(conn->response_headers, sanitized);
        strcat(conn->response_headers, "\r\n");
    }
}

static int server_path_safe(const char *path) {
    if (!path || path[0] == '\0') return 0;
    if (strstr(path, "..")) return 0;
    if (strstr(path, "%2e") || strstr(path, "%2E")) return 0;
    if (strstr(path, "%2f") || strstr(path, "%2F")) return 0;
    if (strstr(path, "%5c") || strstr(path, "%5C")) return 0;
    if (strchr(path, '\\')) return 0;
    for (int i = 0; path[i] != '\0'; i++) {
        unsigned char c = (unsigned char)path[i];
        if (c < 0x20 || c == 0x7F) return 0; /* Control chars & null byte injection */
    }
    return 1;
}

void server_redirect(ClientConnection *conn, const char *url) {
    if (!conn || !url) return;
    char safe_url[1024];
    int si = 0;
    for (int i = 0; url[i] != '\0' && si < (int)sizeof(safe_url) - 1; i++) {
        if (url[i] != '\r' && url[i] != '\n') {
            safe_url[si++] = url[i];
        }
    }
    safe_url[si] = '\0';

    char headers[1024];
    snprintf(headers, sizeof(headers),
             "HTTP/1.1 302 Found\r\n"
             "Location: %s\r\n"
             "Content-Length: 0\r\n"
             "%s"
             "Connection: close\r\n\r\n",
             safe_url, conn->response_headers);
    server_conn_write(conn, headers, strlen(headers));
}

void server_send_404(ClientConnection *conn) {
    server_send_response(conn, 404, "text/html", "<h1>404 Not Found</h1>");
}

int server_serve_file(ClientConnection *conn, const char *filepath, const char *content_type) {
    if (!server_path_safe(filepath)) return 0;

    int fd = open(filepath, O_RDONLY);
    if (fd < 0) return 0;

    off_t fsize = lseek(fd, 0, SEEK_END);
    if (fsize < 0) { close(fd); return 0; }
    lseek(fd, 0, SEEK_SET);

    server_add_header(conn, "Cache-Control: public, max-age=3600\r\n");

    char headers[1024];
    int hlen = snprintf(headers, sizeof(headers),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n"
             "%s"
             "Connection: close\r\n\r\n",
             content_type, (long)fsize, conn->response_headers);

    if (server_conn_write(conn, headers, hlen) <= 0) { close(fd); return 0; }

#ifdef __linux__
    off_t offset = 0;
    while (offset < fsize) {
        ssize_t n = sendfile(conn->socket_fd, fd, &offset, (size_t)(fsize - offset));
        if (n <= 0) break;
    }
#else
    char chunk[8192];
    ssize_t nread;
    lseek(fd, 0, SEEK_SET);
    while ((nread = read(fd, chunk, sizeof(chunk))) > 0) {
        if (ar_socket_send(conn->socket_fd, chunk, (size_t)nread) <= 0) break;
    }
#endif

    close(fd);
    track_access(conn->client_ip, conn->current_path, 200, conn->anon_id);
    return 1;
}

const char* get_header(HttpRequest *req, const char *header_name) {
    if (!req || !header_name) return NULL;
    for (int i = 0; i < req->header_count; i++) {
        if (strcasecmp(req->headers[i].name, header_name) == 0) {
            return req->headers[i].value;
        }
    }
    return NULL;
}

const char* get_path_param(HttpRequest *req, const char *key) {
    if (!req || !key) return NULL;
    for (int i = 0; i < req->path_param_count; i++) {
        if (strcmp(req->path_params[i].key, key) == 0) {
            return req->path_params[i].value;
        }
    }
    return NULL;
}

const char* get_query_param(HttpRequest *req, const char *key) {
    if (!req || !key) return NULL;
    for (int i = 0; i < req->query_count; i++) {
        if (strcmp(req->parsed_query[i].key, key) == 0) {
            return req->parsed_query[i].value;
        }
    }
    return NULL;
}

cJSON* parse_json_body(HttpRequest *req) {
    if (!req || !req->body) return NULL;
    if (!req->json_doc) {
        req->json_doc = cJSON_Parse(req->body);
    }
    return req->json_doc;
}

void server_send_json(ClientConnection *conn, int status, cJSON *json_obj) {
    if (!conn || !json_obj) return;
    char *json_str = cJSON_PrintUnformatted(json_obj);
    if (json_str) {
        server_send_response(conn, status, "application/json", json_str);
        cJSON_free(json_str);
    }
    cJSON_Delete(json_obj);
}

const char* server_get_client_ip(ClientConnection *conn) {
    if (!conn) return "0.0.0.0";
    return conn->client_ip;
}

static void *http_worker_thread(void *arg) {
    int client_socket = *(int*)arg;
    ar_mem_free(arg);

    char buffer[1024];
    int bytes_read = ar_socket_recv(client_socket, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';

        char host[256] = "localhost";
        char *host_start = strstr(buffer, "Host: ");
        if (!host_start) host_start = strstr(buffer, "host: ");
        if (host_start) {
            host_start += 6;
            char *host_end = strstr(host_start, "\r\n");
            if (host_end && (host_end - host_start) < (int)sizeof(host)) {
                int len = host_end - host_start;
                memcpy(host, host_start, len);
                host[len] = '\0';
            }
        }

        if (host[0] == '\0') {
            const char *trusted_host = getenv("TRUSTED_DOMAIN");
            if (!trusted_host || trusted_host[0] == '\0') trusted_host = "localhost";
            size_t tlen = strlen(trusted_host);
            if (tlen >= sizeof(host)) tlen = sizeof(host) - 1;
            memcpy(host, trusted_host, tlen);
            host[tlen] = '\0';
        }

        char response[1024];
        snprintf(response, sizeof(response),
                 "HTTP/1.1 301 Moved Permanently\r\n"
                 "Location: https://%s/\r\n"
                 "Content-Length: 0\r\n"
                 "Connection: close\r\n\r\n", host);
        ar_socket_send(client_socket, response, strlen(response));
    }
    ar_socket_close(client_socket);
    return NULL;
}

static void *redirector_thread(void *arg) {
    (void)arg;
    int server_fd = ar_socket_create(1);
    if (server_fd < 0) {
        alri_print("Failed redirector socket\n");
        return NULL;
    }

    ar_socket_reuseaddr(server_fd, 1);

    if (ar_socket_bind(server_fd, "0.0.0.0", 80) < 0) {
        alri_print("Error opening port 80 for HTTPS redirect (try: sudo ARWS_STAY_ROOT=1)\n");
        ar_socket_close(server_fd);
        return NULL;
    }

    if (ar_socket_listen(server_fd, 100) < 0) {
        alri_print("Error on port 80 Listen\n");
        ar_socket_close(server_fd);
        return NULL;
    }

    alri_print("Running on port 80, redirecting to HTTPS...\n");

    while (1) {
        int client_socket = ar_socket_accept(server_fd);
        if (client_socket < 0) { ar_sleep_ms(1000); continue; }

        ar_socket_set_recv_timeout(client_socket, ARWS_READ_TIMEOUT_MS);

        int *sock_ptr = (int *)ar_mem_alloc(sizeof(int));
        if (sock_ptr) {
            *sock_ptr = client_socket;
            void *worker = ar_thread_create(http_worker_thread, sock_ptr);
            if (worker) {
                ar_thread_detach(worker);
            } else {
                ar_mem_free(sock_ptr);
                ar_socket_close(client_socket);
            }
        }
    }
    return NULL;
}

static void handle_client(ClientConnection *conn) {
    long buffer_size = 65536;
    char *buffer = (char *)ar_mem_alloc(buffer_size);
    if (!buffer) return;
    buffer[0] = '\0';
    long total_read = 0;
    char *body_start = NULL;
    long content_length = 0;
    int headers_parsed = 0;

    while (1) {
        if (total_read >= buffer_size - 1) {
            long new_size = buffer_size * 2;
            if (new_size > 10485760) break;
            char *new_buf = (char *)ar_mem_alloc(new_size);
            if (!new_buf) break;
            memcpy(new_buf, buffer, total_read);
            ar_mem_free(buffer);
            buffer = new_buf;
            buffer_size = new_size;
        }

        int bytes_read = server_conn_read(conn, buffer + total_read, (int)(buffer_size - 1 - total_read));
        if (bytes_read <= 0) break;
        total_read += bytes_read;
        buffer[total_read] = '\0';

        if (!headers_parsed && (body_start = strstr(buffer, "\r\n\r\n")) != NULL) {
            headers_parsed = 1;
            char *cl_str = strstr(buffer, "Content-Length:");
            if (!cl_str) cl_str = strstr(buffer, "content-length:");
            if (cl_str && cl_str < body_start) {
                content_length = strtol(cl_str + 15, NULL, 10);
                long header_len = (body_start + 4) - buffer;
                long required_size = header_len + content_length + 1;
                if (required_size > 10485760) {
                    server_send_response(conn, 413, "text/plain", "Payload Too Large");
                    ar_mem_free(buffer);
                    return;
                }
                if (required_size > buffer_size) {
                    char *new_buf = (char *)ar_mem_alloc(required_size);
                    if (!new_buf) {
                        server_send_response(conn, 500, "text/plain", "Internal Server Error");
                        ar_mem_free(buffer);
                        return;
                    }
                    memset(new_buf, 0, required_size);
                    memcpy(new_buf, buffer, buffer_size);
                    ar_mem_free(buffer);
                    buffer = new_buf;
                    buffer_size = required_size;
                    body_start = strstr(buffer, "\r\n\r\n");
                }
            }
        }

        if (headers_parsed && body_start) {
            int header_len = (int)((body_start + 4) - buffer);
            if (total_read - header_len >= content_length) break;
        }
    }

    if (!body_start) body_start = strstr(buffer, "\r\n\r\n");
    if (!body_start) { ar_mem_free(buffer); return; }

    HttpRequest req;
    memset(&req, 0, sizeof(HttpRequest));
    *body_start = '\0';
    req.body = body_start + 4;
    req.body_length_in_buffer = (int)(total_read - (body_start + 4 - buffer));

    if (content_length > 0 && content_length < (buffer_size - (body_start + 4 - buffer))) {
        req.body[content_length] = '\0';
    }

    char *headers_start = strstr(buffer, "\r\n");
    if (headers_start) {
        *headers_start = '\0';
        headers_start += 2;
    }

    char *saveptr = NULL;
    char *method = strtok_r(buffer, " ", &saveptr);
    char *full_path = strtok_r(NULL, " ", &saveptr);

    if (!method || !full_path) {
        server_send_response(conn, 400, "text/plain", "Bad Request");
        ar_mem_free(buffer);
        return;
    }

    if (strstr(full_path, "%00") || !server_path_safe(full_path)) {
        server_send_response(conn, 400, "text/plain", "Bad Request (Null Byte / Malicious Path)");
        ar_mem_free(buffer);
        return;
    }

    url_decode(full_path, full_path);

    if (strchr(full_path, '%')) {
        char *second_pass = (char *)ar_mem_alloc(strlen(full_path) + 1);
        if (second_pass) {
            strcpy(second_pass, full_path);
            url_decode(full_path, second_pass);
            if (strchr(full_path, '%')) {
                req.method = NULL;
                server_send_response(conn, 400, "text/plain", "Bad Request (Invalid Encoding)");
                ar_mem_free(second_pass);
                ar_mem_free(buffer);
                return;
            }
            ar_mem_free(second_pass);
        }
    }

    {
        char *pc = full_path;
        while (*pc) {
            if ((unsigned char)*pc < 0x20) {
                server_send_response(conn, 400, "text/plain", "Bad Request (Invalid Character)");
                ar_mem_free(buffer);
                return;
            }
            pc++;
        }
    }

    if (!server_path_safe(full_path)) {
        server_send_response(conn, 400, "text/plain", "Bad Request (Path Traversal / Malicious Characters)");
        ar_mem_free(buffer);
        return;
    }

    req.method = method;
    req.path = full_path;
    strncpy(conn->current_path, full_path, sizeof(conn->current_path) - 1);

    char *query = strchr(full_path, '?');
    if (query) {
        *query = '\0';
        req.query_params = query + 1;

        req.query_count = 0;
        char *q = req.query_params;
        while (q && *q && req.query_count < 50) {
            char *amp = strchr(q, '&');
            if (amp) *amp = '\0';
            char *eq = strchr(q, '=');
            if (eq) {
                *eq = '\0';
                req.parsed_query[req.query_count].key = q;
                req.parsed_query[req.query_count].value = eq + 1;
            } else {
                req.parsed_query[req.query_count].key = q;
                req.parsed_query[req.query_count].value = "";
            }
            req.query_count++;
            if (amp) q = amp + 1;
            else break;
        }
    }

    req.header_count = 0;
    if (headers_start) {
        char *line = headers_start;
        char *next_line;
        while (line && *line != '\r' && *line != '\n' && *line != '\0' && req.header_count < 100) {
            next_line = strstr(line, "\r\n");
            if (next_line) {
                *next_line = '\0';
                next_line += 2;
            }

            char *colon = strchr(line, ':');
            if (colon) {
                *colon = '\0';
                req.headers[req.header_count].name = line;
                char *val = colon + 1;
                while (*val == ' ') val++;
                req.headers[req.header_count].value = val;
                req.header_count++;
            }
            line = next_line;
        }
    }

    req.host[0] = '\0';
    const char *host_header = get_header(&req, "Host");
    if (host_header) {
        const char *start = host_header;
        while (*start == ' ') start++;
        const char *end = start;
        while (*end && *end != ':') end++;
        int len = (int)(end - start);
        if (len > 255) len = 255;
        strncpy(req.host, start, len);
        req.host[len] = '\0';
        while (len > 0 && req.host[len - 1] == '.') {
            req.host[--len] = '\0';
        }
    }

    static int behind_cf = -1;
    if (behind_cf == -1) {
        const char *env = getenv("BEHIND_CLOUDFLARE");
        behind_cf = (env && strcmp(env, "true") == 0);
    }
    if (ip_is_trusted_proxy(conn->client_ip)) {
        if (behind_cf) {
            const char *cf_ip = get_header(&req, "CF-Connecting-IP");
            if (cf_ip && ip_looks_valid(cf_ip)) {
                strncpy(conn->client_ip, cf_ip, sizeof(conn->client_ip) - 1);
                conn->client_ip[sizeof(conn->client_ip) - 1] = '\0';
            }
        } else {
            const char *xff = get_header(&req, "X-Forwarded-For");
            if (xff && xff[0]) {
                const char *s = xff;
                while (*s == ' ' || *s == '\t') s++;
                char xff_buf[64];
                strncpy(xff_buf, s, sizeof(xff_buf) - 1);
                xff_buf[sizeof(xff_buf) - 1] = '\0';
                char *comma = strchr(xff_buf, ',');
                if (comma) *comma = '\0';
                char *sp = xff_buf;
                while (*sp) {
                    if ((unsigned char)*sp < 0x20) { sp[0] = '\0'; break; }
                    sp++;
                }
                if (xff_buf[0] && ip_looks_valid(xff_buf)) {
                    strncpy(conn->client_ip, xff_buf, sizeof(conn->client_ip) - 1);
                    conn->client_ip[sizeof(conn->client_ip) - 1] = '\0';
                }
            }
        }
    }

    const char *cookie_header = get_header(&req, "Cookie");
    if (cookie_header) {
        const char *anon_ptr = strstr(cookie_header, "ARC_ANON_ID=");
        if (anon_ptr) {
            strncpy(conn->anon_id, anon_ptr + 12, 64);
            conn->anon_id[64] = '\0';
            char *semi = strchr(conn->anon_id, ';');
            if (semi) *semi = '\0';
        }
    }
    if (conn->anon_id[0] == '\0') {
        unsigned char anon_raw[16];
        int got_random = 0;
        if (RAND_bytes(anon_raw, sizeof(anon_raw)) == 1) {
            got_random = 1;
        } else {
#ifndef _WIN32
            int ufd = open("/dev/urandom", O_RDONLY);
            if (ufd >= 0) {
                ssize_t nr = read(ufd, anon_raw, sizeof(anon_raw));
                close(ufd);
                got_random = (nr == (ssize_t)sizeof(anon_raw));
            }
#endif
        }
        if (got_random) {
            for (int i = 0; i < (int)sizeof(anon_raw); i++)
                snprintf(conn->anon_id + (i * 2), 3, "%02x", anon_raw[i]);
            snprintf(conn->response_headers + strlen(conn->response_headers),
                     sizeof(conn->response_headers) - strlen(conn->response_headers),
                     "Set-Cookie: ARC_ANON_ID=%s; Path=/; HttpOnly; SameSite=Lax; Secure\r\n", conn->anon_id);
        }
    }

    /* Enforce Rate Limiting por IP real, Host e Rota */
    if (!arws_ratelimit_check(conn->client_ip, req.host, req.path)) {
        server_send_response(conn, 429, "text/html",
                             "<!DOCTYPE html><html><head><title>429 Too Many Requests</title></head>"
                             "<body style='font-family:sans-serif;text-align:center;padding:50px;'>"
                             "<h1>429 Too Many Requests</h1>"
                             "<p>Voce atingiu o limite de requisicoes. Por favor, aguarde alguns segundos.</p>"
                             "</body></html>");
        if (req.json_doc) cJSON_Delete(req.json_doc);
        ar_mem_free(buffer);
        return;
    }

    if (global_api_handler) {
        global_api_handler(conn, &req);
    } else {
        server_send_response(conn, 404, "text/plain", "Not Found");
    }

    if (req.json_doc) cJSON_Delete(req.json_doc);
    ar_mem_free(buffer);
}

static void resolve_client_ip(int sock, char *ip_buf, int ip_buf_len) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    if (getpeername(sock, (struct sockaddr*)&addr, &addr_len) == 0) {
        const char *res = inet_ntop(AF_INET, &addr.sin_addr, ip_buf, ip_buf_len);
        if (res) {
            ip_buf[ip_buf_len - 1] = '\0';
            return;
        }
    }
    strncpy(ip_buf, "0.0.0.0", ip_buf_len - 1);
    ip_buf[ip_buf_len - 1] = '\0';
}

#define POOL_SIZE 512
#define QUEUE_SIZE 16384

typedef struct {
    EConn *ec;
} PoolJob;

static PoolJob job_queue[QUEUE_SIZE];
static volatile int job_head = 0;
static volatile int job_tail = 0;
static volatile int job_count = 0;
static void *job_mutex = NULL;
static void *job_cond = NULL;
static volatile int pool_shutdown = 0;
static void *pool_threads[POOL_SIZE];

static void pool_push(EConn *ec) {
    ar_mutex_lock(job_mutex);
    while (job_count >= QUEUE_SIZE && !pool_shutdown) {
        ar_mutex_unlock(job_mutex);
        ar_sleep_ms(1);
        ar_mutex_lock(job_mutex);
    }
    if (pool_shutdown) { ar_mutex_unlock(job_mutex); return; }
    job_queue[job_tail].ec = ec;
    job_tail = (job_tail + 1) % QUEUE_SIZE;
    job_count++;
    ar_cond_signal(job_cond);
    ar_mutex_unlock(job_mutex);
}

static int pool_pop(PoolJob *job) {
    ar_mutex_lock(job_mutex);
    while (job_count == 0 && !pool_shutdown) {
        ar_cond_wait(job_cond, job_mutex);
    }
    if (pool_shutdown && job_count == 0) {
        ar_mutex_unlock(job_mutex);
        return -1;
    }
    *job = job_queue[job_head];
    job_head = (job_head + 1) % QUEUE_SIZE;
    job_count--;
    ar_mutex_unlock(job_mutex);
    return 0;
}

static void* pool_worker(void *arg) {
    (void)arg;
    while (1) {
        PoolJob job;
        if (pool_pop(&job) < 0) break;

        EConn *ec = job.ec;
        if (!ec) continue;

        ClientConnection conn;
        conn.socket_fd = ec->fd;
        conn.mode = ec->mode;
        conn.ssl = NULL;
        strncpy(conn.client_ip, ec->client_ip, sizeof(conn.client_ip) - 1);
        conn.client_ip[sizeof(conn.client_ip) - 1] = '\0';
        memset(conn.current_path, 0, sizeof(conn.current_path));

        ar_socket_set_recv_timeout(conn.socket_fd, ARWS_READ_TIMEOUT_MS);

        if (conn.mode == MODE_SECURE) {
            conn.ssl = ar_ssl_new(ec->ctx, ec->fd);
            if (conn.ssl && ar_ssl_handshake(conn.ssl) == 0) {
                handle_client(&conn);
            }
            if (conn.ssl) ar_ssl_free(conn.ssl);
        } else {
            handle_client(&conn);
        }

        ar_socket_close(ec->fd);
        ar_mutex_lock(conn_limit_mutex);
        active_connections--;
        ar_mutex_unlock(conn_limit_mutex);
        econn_free(ec);
    }
    return NULL;
}

void server_stop(void) {
    http_server_running = 0;
    int fd = http_server_sock;
    http_server_sock = -1;
    if (fd >= 0) {
        ar_socket_close(fd);
    }
}

int server_start(int port, int mode, RequestHandler handler) {
    global_api_handler = handler;
    http_server_running = 1;
    if (!conn_limit_mutex)
        conn_limit_mutex = ar_mutex_create();
    if (econn_pool)
        memset(econn_pool, 0, MAX_CONN * sizeof(EConn));

    if (!job_mutex) {
        job_mutex = ar_mutex_create();
        job_cond = ar_cond_create();
        for (int i = 0; i < POOL_SIZE; i++) {
            pool_threads[i] = ar_thread_create(pool_worker, NULL);
            if (pool_threads[i]) ar_thread_detach(pool_threads[i]);
        }
    }
    pool_shutdown = 0;
    job_head = job_tail = job_count = 0;

    void *ctx = NULL;
    int using_ssl = 0;

    if (mode == MODE_SECURE) {
        ctx = ar_ssl_ctx_create(1);
        if (!ctx) {
            alri_print("Failed to create SSL context\n");
            return -1;
        }

        if (ar_ssl_ctx_use_certificate(ctx, "storage/arws/certs/cert.pem", "storage/arws/certs/key.pem") != 0) {
            alri_print("Failed to load cert.pem / key.pem\n");
            return -1;
        }

        void *redir = ar_thread_create(redirector_thread, NULL);
        if (redir) ar_thread_detach(redir);
        using_ssl = 1;
    }

    while (http_server_running) {
        int server_sock = ar_socket_create(1);
        if (server_sock < 0) {
            alri_print("Failed to create server socket\n");
            http_server_running = 0;
            return -1;
        }

        ar_socket_reuseaddr(server_sock, 1);
        if (ar_socket_bind(server_sock, global_bind_address, (uint16_t)port) < 0) {
            if (port == 443 || port == 80) {
                alri_print("Port %d needs root/cap_net_bind_service. Falling back to HTTP on port 8080...\n", port);
                ar_socket_close(server_sock);
                http_server_sock = -1;
                ctx = NULL;
                using_ssl = 0;
                mode = 0;
                port = 8080;
                arws_config_set_port(8080);
                ar_sleep_ms(200);
                continue;
            }
            alri_print("Error binding to port %d (try: sudo ARWS_STAY_ROOT=1)\n", port);
            ar_socket_close(server_sock);
            http_server_sock = -1;
            return -1;
        }

        if (ar_socket_listen(server_sock, 4096) < 0) {
            alri_print("Error on main server listen\n");
            ar_socket_close(server_sock);
            http_server_sock = -1;
            return -1;
        }

        http_server_sock = server_sock;

        if (using_ssl) {
            alri_print("HTTPS Server started on port %d!\n", port);
        } else {
            alri_print("HTTP Server started on port %d!\n", port);
        }

        ar_socket_set_nonblock(server_sock);

        ar_poll = ar_poll_create();
        if (!ar_poll) {
            alri_print("Failed to create event poll\n");
            ar_socket_close(server_sock);
            http_server_sock = -1;
            return -1;
        }

        ar_poll_add(ar_poll, server_sock, AR_EVENT_READ, NULL);

        ArPollEvent *events = (ArPollEvent *)calloc(MAX_POLL_EVENTS, sizeof(ArPollEvent));
        if (!events) {
            alri_print("Failed to allocate poll events\n");
            ar_poll_remove(ar_poll, server_sock);
            ar_poll_destroy(ar_poll);
            ar_poll = NULL;
            ar_socket_close(server_sock);
            http_server_sock = -1;
            return -1;
        }

        while (http_server_running) {
            int nfds = ar_poll_wait(ar_poll, events, MAX_POLL_EVENTS, 1000);
            if (nfds < 0) {
                if (errno == EINTR) continue;
                alri_print(RED "[ARWS-SERVER]" RST " ar_poll_wait returned %d, errno=%d (%s)\n",
                           nfds, errno, strerror(errno));
                break;
            }
            if (nfds == 0) continue;

            for (int i = 0; i < nfds; i++) {
                if (events[i].fd == server_sock) {
                    while (1) {
                        struct sockaddr_in addr;
                        socklen_t addr_len = sizeof(addr);
                        int csock = accept(server_sock, (struct sockaddr*)&addr, &addr_len);
                        if (csock < 0) break;

                        ar_mutex_lock(conn_limit_mutex);
                        if (active_connections >= MAX_CONNECTIONS) {
                            ar_mutex_unlock(conn_limit_mutex);
                            const char *busy = "HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                            ar_socket_send(csock, busy, strlen(busy));
                            ar_socket_close(csock);
                            continue;
                        }
                        active_connections++;
                        ar_mutex_unlock(conn_limit_mutex);

                        EConn *ec = econn_alloc();
                        if (!ec) {
                            ar_mutex_lock(conn_limit_mutex);
                            active_connections--;
                            ar_mutex_unlock(conn_limit_mutex);
                            ar_socket_close(csock);
                            continue;
                        }
                        ec->fd = csock;
                        ec->mode = mode;
                        ec->ctx = ctx;

                        const char *ip = inet_ntop(AF_INET, &addr.sin_addr, ec->client_ip, sizeof(ec->client_ip));
                        if (!ip) strcpy(ec->client_ip, "0.0.0.0");

                        pool_push(ec);
                    }
                }
            }
        }

        free(events);
        ar_poll_remove(ar_poll, server_sock);
        ar_poll_destroy(ar_poll);
        ar_poll = NULL;

        ar_socket_close(server_sock);
        http_server_sock = -1;
    }

    if (ctx) ar_ssl_ctx_free(ctx);
    return 0;
}

void server_set_logger(LoggerCallback callback) {
    global_logger = callback;
}
