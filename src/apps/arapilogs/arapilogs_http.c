/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arapilogs_http.h"
#include "arapilogs_config.h"
#include "arapilogs_db.h"
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
    char *auth = strstr(buf, "Authorization: Bearer ");
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

    ArapilogsConfig *cfg = arapilogs_config_get();
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

    char resp[2048] = {0};
    int recvd = read(fd, resp, sizeof(resp) - 1);
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
        out_sess->is_master = (strcmp(m, "true") == 0 || strcmp(m, "1") == 0);
        free(m);
    }
    out_sess->valid = 1;
    return 0;
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

    char client_ip[64] = "127.0.0.1";
    char *xff = strstr(buf, "X-Forwarded-For:");
    if (xff) {
        sscanf(xff + 16, "%63s", client_ip);
        char *comma = strchr(client_ip, ',');
        if (comma) *comma = '\0';
    }

    char *body = strstr(buf, "\r\n\r\n");
    if (body) body += 4;

    /* 1. GET /arapi/logs/status or /health */
    if (strcmp(method, "GET") == 0 &&
        (strcmp(path, "/arapi/logs/status") == 0 || strcmp(path, "/arapi/logs/health") == 0 || strcmp(path, "/arapi/logs/") == 0)) {
        char json[512];
        snprintf(json, sizeof(json),
            "{\"status\":\"online\",\"backend\":\"arapilogs\",\"prefix\":\"/arapi/logs\","
            "\"guarantee\":\"Sovereign Audit Log with Strict DM Privacy Isolation\"}\n");
        send_http_response(client_fd, 200, "OK", "application/json", NULL, json);
        ar_socket_close(client_fd);
        return NULL;
    }

    /* 2. POST /arapi/logs/ingest (Event Ingestion from any internal ALRI service) */
    if (strcmp(method, "POST") == 0 && (strcmp(path, "/arapi/logs/ingest") == 0 || strcmp(path, "/arapi/logs") == 0)) {
        if (!body) {
            send_http_response(client_fd, 400, "Bad Request", "application/json", NULL, "{\"error\":\"Missing body\"}\n");
            ar_socket_close(client_fd);
            return NULL;
        }

        char *user = extract_json_string(body, "user");
        char *tenant = extract_json_string(body, "tenant");
        char *service = extract_json_string(body, "service");
        char *action = extract_json_string(body, "action");
        char *severity = extract_json_string(body, "severity");
        char *status_str = extract_json_string(body, "status_code");
        char *details = extract_json_string(body, "details");
        char *req_ip = extract_json_string(body, "ip");

        int status_code = status_str ? atoi(status_str) : 200;
        const char *effective_ip = (req_ip && req_ip[0]) ? req_ip : client_ip;

        arapilogs_db_append(user, tenant, service, action, severity ? severity : "INFO",
                            status_code, effective_ip, details);

        if (user) free(user);
        if (tenant) free(tenant);
        if (service) free(service);
        if (action) free(action);
        if (severity) free(severity);
        if (status_str) free(status_str);
        if (details) free(details);
        if (req_ip) free(req_ip);

        send_http_response(client_fd, 201, "Created", "application/json", NULL, "{\"status\":\"success\",\"message\":\"Event ingested\"}\n");
        ar_socket_close(client_fd);
        return NULL;
    }

    /* 3. GET /arapi/logs or /arapi/logs/query (Protected with Zero-Trust ARAUTH Session) */
    if (strcmp(method, "GET") == 0 && (strncmp(path, "/arapi/logs/query", 17) == 0 || strcmp(path, "/arapi/logs") == 0)) {
        char *token = extract_session_token_from_request(buf);
        AuthSession sess;
        int auth_ok = (token && verify_auth_token(token, &sess) == 0 && sess.valid);
        if (token) free(token);

        if (!auth_ok) {
            send_http_response(client_fd, 401, "Unauthorized", "application/json", NULL, "{\"error\":\"Authentication required to access audit telemetry\"}\n");
            ar_socket_close(client_fd);
            return NULL;
        }

        // Parse query params: ?tenant=...&service=...&severity=...&q=...&limit=...
        char tenant_param[64] = {0}, service_param[32] = {0}, severity_param[16] = {0}, q_param[128] = {0};
        int limit = 150;

        char *qmark = strchr(path, '?');
        if (qmark) {
            char *p = qmark + 1;
            char *tok = strtok(p, "&");
            while (tok) {
                if (strncmp(tok, "tenant=", 7) == 0) strncpy(tenant_param, tok + 7, sizeof(tenant_param) - 1);
                else if (strncmp(tok, "service=", 8) == 0) strncpy(service_param, tok + 8, sizeof(service_param) - 1);
                else if (strncmp(tok, "severity=", 9) == 0) strncpy(severity_param, tok + 9, sizeof(severity_param) - 1);
                else if (strncmp(tok, "q=", 2) == 0) strncpy(q_param, tok + 2, sizeof(q_param) - 1);
                else if (strncmp(tok, "limit=", 6) == 0) limit = atoi(tok + 6);
                tok = strtok(NULL, "&");
            }
        }

        // If caller is not master, force tenant filter to caller's own tenant
        const char *effective_tenant = sess.is_master ? (tenant_param[0] ? tenant_param : "all") : sess.tenant;

        char *json = arapilogs_db_query_json(effective_tenant, sess.is_master,
                                             service_param[0] ? service_param : NULL,
                                             severity_param[0] ? severity_param : NULL,
                                             q_param[0] ? q_param : NULL,
                                             limit);

        send_http_response(client_fd, 200, "OK", "application/json", NULL, json);
        free(json);
        ar_socket_close(client_fd);
        return NULL;
    }

    /* 4. GET /arapi/logs/metrics (Protected Dashboard Metrics) */
    if (strcmp(method, "GET") == 0 && strcmp(path, "/arapi/logs/metrics") == 0) {
        char *token = extract_session_token_from_request(buf);
        AuthSession sess;
        int auth_ok = (token && verify_auth_token(token, &sess) == 0 && sess.valid);
        if (token) free(token);

        if (!auth_ok) {
            send_http_response(client_fd, 401, "Unauthorized", "application/json", NULL, "{\"error\":\"Authentication required\"}\n");
            ar_socket_close(client_fd);
            return NULL;
        }

        const char *effective_tenant = sess.is_master ? "all" : sess.tenant;
        char *metrics_json = arapilogs_db_metrics_json(effective_tenant, sess.is_master);

        send_http_response(client_fd, 200, "OK", "application/json", NULL, metrics_json);
        free(metrics_json);
        ar_socket_close(client_fd);
        return NULL;
    }

    /* Fallback 404 */
    send_http_response(client_fd, 404, "Not Found", "application/json", NULL, "{\"error\":\"Endpoint not found in /arapi/logs\"}\n");
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

int arapilogs_http_server_start(const char *bind_ip, int port) {
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
    alri_print(GRN "[ARAPILOGS]" RST " Audit & Telemetry API Backend listening on %s:%d (prefix: /arapi/logs)\n", bind_ip ? bind_ip : "127.0.0.1", port);
    return 0;
}

void arapilogs_http_server_stop(void) {
    if (!g_http_running) return;
    g_http_running = 0;
    if (g_http_listen_fd >= 0) {
        ar_socket_close(g_http_listen_fd);
        g_http_listen_fd = -1;
    }
    alri_print(CYN "[ARAPILOGS]" RST " Audit & Telemetry API Backend stopped.\n");
}

int arapilogs_http_is_running(void) {
    return g_http_running;
}
