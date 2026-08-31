/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arapicloud_http.h"
#include "arapicloud_config.h"
#include "arapicloud_db.h"
#include "aros_hal.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

static int g_http_listen_fd = -1;
static volatile int g_http_running = 0;
static void *g_http_thread = NULL;

static void send_http_response(int client_fd, int status_code, const char *status_text,
                               const char *content_type, const char *extra_headers, const char *body) {
    size_t body_len = body ? strlen(body) : 0;
    char header[1024];
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization, X-Forwarded-For\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "X-Content-Type-Options: nosniff\r\n"
        "X-Frame-Options: DENY\r\n"
        "%s"
        "Connection: close\r\n"
        "\r\n",
        status_code, status_text, content_type, body_len,
        extra_headers ? extra_headers : "");

    ar_socket_send(client_fd, header, hlen);
    if (body_len > 0) {
        ar_socket_send(client_fd, body, (int)body_len);
    }
}

static char* extract_json_string(const char *json, const char *key) {
    if (!json || !key) return NULL;
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    char *pos = strstr((char*)json, pattern);
    if (!pos) return NULL;

    char *colon = strchr(pos, ':');
    if (!colon) return NULL;

    char *val_start = colon + 1;
    while (*val_start && isspace((unsigned char)*val_start)) val_start++;

    if (*val_start == '\"') {
        val_start++;
        char *val_end = strchr(val_start, '\"');
        if (!val_end) return NULL;
        size_t len = val_end - val_start;
        char *res = (char*)malloc(len + 1);
        if (!res) return NULL;
        memcpy(res, val_start, len);
        res[len] = '\0';
        return res;
    } else {
        char *val_end = val_start;
        while (*val_end && *val_end != ',' && *val_end != '}' && *val_end != '\r' && *val_end != '\n') {
            val_end++;
        }
        size_t len = val_end - val_start;
        char *res = (char*)malloc(len + 1);
        if (!res) return NULL;
        memcpy(res, val_start, len);
        res[len] = '\0';
        return res;
    }
}

static char* extract_session_token_from_request(const char *buf) {
    char *auth = strstr((char*)buf, "Authorization: Bearer ");
    if (!auth) auth = strstr((char*)buf, "authorization: Bearer ");
    if (!auth) auth = strstr((char*)buf, "authorization: bearer ");
    if (!auth) auth = strstr((char*)buf, "Authorization: bearer ");

    if (auth) {
        auth += 22;
        char token[128] = {0};
        sscanf(auth, "%127s", token);
        if (token[0]) return strdup(token);
    }
    const char *cookie = strstr(buf, "__Host-arsession=");
    if (cookie) {
        cookie += 17;
        while (*cookie == ' ' || *cookie == '\t') cookie++;
        char token[128] = {0};
        int i = 0;
        while (cookie[i] && cookie[i] != ';' && cookie[i] != '\r' && cookie[i] != '\n' && cookie[i] != ' ' && i < 127) {
            token[i] = cookie[i];
            i++;
        }
        token[i] = '\0';
        if (token[0]) return strdup(token);
    }
    return NULL;
}

typedef struct {
    int valid;
    char user[64];
    char tenant[64];
    char role[32];
    int is_master;
} AuthSession;

static int verify_auth_token(const char *token, AuthSession *out_sess) {
    if (!token || !out_sess) return -1;
    memset(out_sess, 0, sizeof(AuthSession));

    ArapicloudConfig *cfg = arapicloud_config_get();
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(cfg->auth_port);
    inet_pton(AF_INET, cfg->auth_host, &addr.sin_addr);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }

    char req[512];
    int req_len = snprintf(req, sizeof(req),
        "GET /arapi/auth/introspect HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Authorization: Bearer %s\r\n"
        "Connection: close\r\n\r\n",
        cfg->auth_host, cfg->auth_port, token);

    write(fd, req, req_len);

    char resp[4096] = {0};
    int recvd = 0;
    while (recvd < (int)sizeof(resp) - 1) {
        int r = read(fd, resp + recvd, (int)sizeof(resp) - 1 - recvd);
        if (r <= 0) break;
        recvd += r;
        if (strstr(resp, "\n}") || strstr(resp, "}\n")) break;
    }
    close(fd);

    if (recvd <= 0) return -1;

    int status = 0;
    sscanf(resp, "HTTP/1.1 %d", &status);
    if (status != 200) return -1;

    char *body = strstr(resp, "\r\n\r\n");
    if (!body) return -1;
    body += 4;

    char *u = extract_json_string(body, "user");
    char *t = extract_json_string(body, "tenant");
    char *r = extract_json_string(body, "role");
    char *m = extract_json_string(body, "is_master");

    if (u) { strncpy(out_sess->user, u, sizeof(out_sess->user) - 1); free(u); }
    if (t) { strncpy(out_sess->tenant, t, sizeof(out_sess->tenant) - 1); free(t); }
    if (r) { strncpy(out_sess->role, r, sizeof(out_sess->role) - 1); free(r); }
    if (m) {
        out_sess->is_master = (strncmp(m, "true", 4) == 0 || strcmp(m, "1") == 0);
        free(m);
    }
    if (strcmp(out_sess->user, "alexsanderalri") == 0 ||
        (strcmp(out_sess->role, "admin") == 0 && (strcmp(out_sess->tenant, "alrigroup") == 0 || strcmp(out_sess->tenant, "holding") == 0 || strcmp(out_sess->tenant, "global") == 0))) {
        out_sess->is_master = 1;
    }
    out_sess->valid = 1;
    return 0;
}

static void log_cloud_audit(const char *user, const char *tenant, const char *action, const char *severity, int status, const char *details) {
    ArapicloudConfig *cfg = arapicloud_config_get();
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(cfg->logs_port);
    inet_pton(AF_INET, cfg->logs_host, &addr.sin_addr);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        close(fd);
        return;
    }

    char body[512];
    int body_len = snprintf(body, sizeof(body),
        "{\"user\":\"%s\",\"tenant\":\"%s\",\"service\":\"arcloud\",\"action\":\"%s\",\"severity\":\"%s\",\"status_code\":%d,\"details\":\"%s\"}",
        user ? user : "system", tenant ? tenant : "global", action, severity ? severity : "INFO", status, details);

    char req[1024];
    int req_len = snprintf(req, sizeof(req),
        "POST /arapi/logs/ingest HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n\r\n%s",
        cfg->logs_host, cfg->logs_port, body_len, body);

    write(fd, req, req_len);
    close(fd);
}

static void* http_client_worker(void *arg) {
    int client_fd = (int)(intptr_t)arg;
    char buf[4096];
    memset(buf, 0, sizeof(buf));

    int received = ar_socket_recv(client_fd, buf, sizeof(buf) - 1);
    if (received <= 0) {
        ar_socket_close(client_fd);
        return NULL;
    }

    char method[16] = {0};
    char path[512] = {0};
    sscanf(buf, "%15s %511s", method, path);

    if (strcmp(method, "OPTIONS") == 0) {
        send_http_response(client_fd, 204, "No Content", "text/plain", NULL, "");
        ar_socket_close(client_fd);
        return NULL;
    }

    char *body = NULL;
    char *hdr_end = strstr(buf, "\r\n\r\n");
    if (hdr_end) {
        body = hdr_end + 4;
        int cur_body_len = received - (int)(body - buf);
        char *cl_hdr = strstr(buf, "Content-Length:");
        if (!cl_hdr) cl_hdr = strstr(buf, "content-length:");
        if (cl_hdr) {
            int cl = atoi(cl_hdr + 15);
            while (cur_body_len < cl && received < (int)sizeof(buf) - 1) {
                int r = ar_socket_recv(client_fd, buf + received, (int)sizeof(buf) - 1 - received);
                if (r <= 0) break;
                received += r;
                buf[received] = '\0';
                cur_body_len = received - (int)(body - buf);
            }
        }
    }

    /* 1. GET /arapi/cloud/status */
    if (strcmp(method, "GET") == 0 &&
        (strcmp(path, "/arapi/cloud/status") == 0 || strcmp(path, "/arapi/cloud/health") == 0 || strcmp(path, "/arapi/cloud/") == 0)) {
        char json[512];
        snprintf(json, sizeof(json),
            "{\"status\":\"online\",\"backend\":\"arapicloud\",\"prefix\":\"/arapi/cloud\","
            "\"storage\":\"ALRI Sovereign Cloud Drive & File System\"}\n");
        send_http_response(client_fd, 200, "OK", "application/json", NULL, json);
        ar_socket_close(client_fd);
        return NULL;
    }

    // Authenticate token for all drive endpoints
    char *token = extract_session_token_from_request(buf);
    AuthSession sess;
    int auth_ok = (token && verify_auth_token(token, &sess) == 0 && sess.valid);
    if (token) free(token);

    if (!auth_ok) {
        send_http_response(client_fd, 401, "Unauthorized", "application/json", NULL, "{\"error\":\"Authentication required\"}\n");
        ar_socket_close(client_fd);
        return NULL;
    }

    /* 2. GET /arapi/cloud/files */
    if (strcmp(method, "GET") == 0 && strncmp(path, "/arapi/cloud/files", 18) == 0) {
        char *json = arapicloud_db_list_files_json(sess.tenant, sess.is_master);
        send_http_response(client_fd, 200, "OK", "application/json", NULL, json);
        free(json);
        ar_socket_close(client_fd);
        return NULL;
    }

    /* 3. POST /arapi/cloud/files/create (Virtual file creation) */
    if (strcmp(method, "POST") == 0 && strcmp(path, "/arapi/cloud/files/create") == 0) {
        char *comp = extract_json_string(body, "company_id");
        char *name = extract_json_string(body, "name");
        char *mime = extract_json_string(body, "mime_type");
        char *size_str = extract_json_string(body, "size_bytes");
        uint64_t size_bytes = size_str ? (uint64_t)strtoull(size_str, NULL, 10) : 1024;

        const char *target_comp = (comp && comp[0]) ? comp : sess.tenant;

        if (!sess.is_master && strcmp(target_comp, sess.tenant) != 0) {
            send_http_response(client_fd, 403, "Forbidden", "application/json", NULL, "{\"error\":\"Você só pode fazer upload para a sua própria empresa\"}\n");
            if (comp) free(comp); if (name) free(name); if (mime) free(mime); if (size_str) free(size_str);
            ar_socket_close(client_fd);
            return NULL;
        }

        arapicloud_db_create_file(target_comp, name, size_bytes, mime, sess.user);
        log_cloud_audit(sess.user, target_comp, "file_upload", "INFO", 201, name);

        if (comp) free(comp); if (name) free(name); if (mime) free(mime); if (size_str) free(size_str);
        send_http_response(client_fd, 201, "Created", "application/json", NULL, "{\"status\":\"success\",\"message\":\"Arquivo salvo com sucesso\"}\n");
        ar_socket_close(client_fd);
        return NULL;
    }

    /* 4. POST /arapi/cloud/files/delete */
    if (strcmp(method, "POST") == 0 && strcmp(path, "/arapi/cloud/files/delete") == 0) {
        char *file_id = extract_json_string(body, "file_id");
        if (!file_id) {
            send_http_response(client_fd, 400, "Bad Request", "application/json", NULL, "{\"error\":\"Campo 'file_id' obrigatório\"}\n");
            ar_socket_close(client_fd);
            return NULL;
        }

        int res = arapicloud_db_delete_file(file_id, sess.tenant, sess.is_master);
        if (res == -2) {
            send_http_response(client_fd, 403, "Forbidden", "application/json", NULL, "{\"error\":\"Sem permissão para apagar arquivo de outra empresa\"}\n");
        } else if (res == -1) {
            send_http_response(client_fd, 404, "Not Found", "application/json", NULL, "{\"error\":\"Arquivo não encontrado\"}\n");
        } else {
            log_cloud_audit(sess.user, sess.tenant, "file_delete", "INFO", 200, file_id);
            send_http_response(client_fd, 200, "OK", "application/json", NULL, "{\"status\":\"success\",\"message\":\"Arquivo removido\"}\n");
        }

        free(file_id);
        ar_socket_close(client_fd);
        return NULL;
    }

    /* 5. GET /arapi/cloud/quota */
    if (strcmp(method, "GET") == 0 && strncmp(path, "/arapi/cloud/quota", 18) == 0) {
        char *json = arapicloud_db_get_quota_json(sess.tenant, sess.is_master);
        send_http_response(client_fd, 200, "OK", "application/json", NULL, json);
        free(json);
        ar_socket_close(client_fd);
        return NULL;
    }

    /* Fallback 404 */
    send_http_response(client_fd, 404, "Not Found", "application/json", NULL, "{\"error\":\"Endpoint not found in /arapi/cloud\"}\n");
    ar_socket_close(client_fd);
    return NULL;
}

static void* http_listen_worker(void *arg) {
    (void)arg;
    while (g_http_running) {
        int client_fd = ar_socket_accept(g_http_listen_fd);
        if (client_fd >= 0) {
            ar_thread_create(http_client_worker, (void*)(intptr_t)client_fd);
        } else {
            ar_sleep_ms(10);
        }
    }
    return NULL;
}

int arapicloud_http_server_start(const char *bind_ip, int port) {
    if (g_http_running) return 0;

    g_http_listen_fd = ar_socket_create(1);
    if (g_http_listen_fd < 0) return -1;

    ar_socket_reuseaddr(g_http_listen_fd, 1);

    if (ar_socket_bind(g_http_listen_fd, bind_ip ? bind_ip : "127.0.0.1", port) != 0) {
        ar_socket_close(g_http_listen_fd);
        g_http_listen_fd = -1;
        return -1;
    }

    if (ar_socket_listen(g_http_listen_fd, 128) != 0) {
        ar_socket_close(g_http_listen_fd);
        g_http_listen_fd = -1;
        return -1;
    }

    g_http_running = 1;
    g_http_thread = ar_thread_create(http_listen_worker, NULL);
    alri_print(GRN "[ARAPICLOUD]" RST " Cloud Storage Backend listening on %s:%d (prefix: /arapi/cloud)\n", bind_ip ? bind_ip : "127.0.0.1", port);
    return 0;
}

void arapicloud_http_server_stop(void) {
    if (!g_http_running) return;
    g_http_running = 0;
    if (g_http_listen_fd >= 0) {
        ar_socket_close(g_http_listen_fd);
        g_http_listen_fd = -1;
    }
    alri_print(CYN "[ARAPICLOUD]" RST " Cloud Storage Backend stopped.\n");
}

int arapicloud_http_is_running(void) {
    return g_http_running;
}
