/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arapiwork_http.h"
#include "arapiwork_config.h"
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

    ArapiworkConfig *cfg = arapiwork_config_get();
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

static void log_audit_event(const char *user, const char *tenant, const char *action, const char *details) {
    ArapiworkConfig *cfg = arapiwork_config_get();
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
        "{\"user\":\"%s\",\"tenant\":\"%s\",\"service\":\"arwork\",\"action\":\"%s\",\"severity\":\"INFO\",\"status_code\":200,\"details\":\"%s\"}",
        user ? user : "system", tenant ? tenant : "global", action, details);

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

    /* 1. GET /arapi/work/status or /health */
    if (strcmp(method, "GET") == 0 &&
        (strcmp(path, "/arapi/work/status") == 0 || strcmp(path, "/arapi/work/health") == 0 || strcmp(path, "/arapi/work/") == 0)) {
        char json[512];
        snprintf(json, sizeof(json),
            "{\"status\":\"online\",\"backend\":\"arapiwork\",\"prefix\":\"/arapi/work\","
            "\"hub\":\"ALRIGROUP Sovereign Workspace Hub\"}\n");
        send_http_response(client_fd, 200, "OK", "application/json", NULL, json);
        ar_socket_close(client_fd);
        return NULL;
    }

    /* 2. GET /arapi/work/catalog (Protected App Directory) */
    if (strcmp(method, "GET") == 0 && (strcmp(path, "/arapi/work/catalog") == 0 || strcmp(path, "/arapi/work/apps") == 0)) {
        char *token = extract_session_token_from_request(buf);
        AuthSession sess;
        int auth_ok = (token && verify_auth_token(token, &sess) == 0 && sess.valid);
        if (token) free(token);

        if (!auth_ok) {
            send_http_response(client_fd, 401, "Unauthorized", "application/json", NULL, "{\"error\":\"Authentication required\"}\n");
            ar_socket_close(client_fd);
            return NULL;
        }

        log_audit_event(sess.user, sess.tenant, "workspace_catalog_view", "Accessed application hub catalog");

        char json[4096];
        snprintf(json, sizeof(json),
            "{\n"
            "  \"user\": \"%s\",\n"
            "  \"tenant\": \"%s\",\n"
            "  \"role\": \"%s\",\n"
            "  \"is_master\": %s,\n"
            "  \"apps\": [\n"
            "    {\n"
            "      \"id\": \"arbus\",\n"
            "      \"name\": \"ALRI-Business\",\n"
            "      \"sigla\": \"ARBUS\",\n"
            "      \"category\": \"Gestão Corporativa\",\n"
            "      \"description\": \"RH, Organograma, Empresas, Departamentos e Cargos\",\n"
            "      \"icon\": \"🏢\",\n"
            "      \"badge\": \"Core\",\n"
            "      \"url\": \"https://arbus.alrigroup.com\",\n"
            "      \"local_url\": \"http://localhost:3013\"\n"
            "    },\n"
            "    {\n"
            "      \"id\": \"arconn\",\n"
            "      \"name\": \"ALRI-Connect\",\n"
            "      \"sigla\": \"ARCONN\",\n"
            "      \"category\": \"Comunicação & Demandas\",\n"
            "      \"description\": \"Canais por equipe, tarefas Kanban e conversas privadas sigilosas\",\n"
            "      \"icon\": \"💬\",\n"
            "      \"badge\": \"Colaboração\",\n"
            "      \"url\": \"https://arconn.alrigroup.com\",\n"
            "      \"local_url\": \"http://localhost:3017\"\n"
            "    },\n"
            "    {\n"
            "      \"id\": \"ardash\",\n"
            "      \"name\": \"ALRI-Dashboard\",\n"
            "      \"sigla\": \"ARDASH\",\n"
            "      \"category\": \"Business Intelligence\",\n"
            "      \"description\": \"Gráficos analíticos, faturamento executivo e KPIs de performance\",\n"
            "      \"icon\": \"📊\",\n"
            "      \"badge\": \"Analytics\",\n"
            "      \"url\": \"https://ardash.alrigroup.com\",\n"
            "      \"local_url\": \"http://localhost:3015\"\n"
            "    },\n"
            "    {\n"
            "      \"id\": \"archat\",\n"
            "      \"name\": \"ALRI-Chat\",\n"
            "      \"sigla\": \"ARCHAT\",\n"
            "      \"category\": \"Atendimento & Suporte\",\n"
            "      \"description\": \"Painel de atendimento multicanal e widget institucional\",\n"
            "      \"icon\": \"🎧\",\n"
            "      \"badge\": \"Tickets\",\n"
            "      \"url\": \"https://archat.alrigroup.com\",\n"
            "      \"local_url\": \"http://localhost:3014\"\n"
            "    },\n"
            "    {\n"
            "      \"id\": \"arcloud\",\n"
            "      \"name\": \"ALRI-Cloud\",\n"
            "      \"sigla\": \"ARCLOUD\",\n"
            "      \"category\": \"Arquivos & Drive\",\n"
            "      \"description\": \"Armazenamento em nuvem soberano e documentos compartilhados\",\n"
            "      \"icon\": \"☁️\",\n"
            "      \"badge\": \"Storage\",\n"
            "      \"url\": \"https://arcloud.alrigroup.com\",\n"
            "      \"local_url\": \"http://localhost:3016\"\n"
            "    },\n"
            "    {\n"
            "      \"id\": \"arstock\",\n"
            "      \"name\": \"ALRI-Stock\",\n"
            "      \"sigla\": \"ARSTOCK\",\n"
            "      \"category\": \"Estoque & Suprimentos\",\n"
            "      \"description\": \"Controle de inventário, catalogação de SKUs e almoxarifados\",\n"
            "      \"icon\": \"📦\",\n"
            "      \"badge\": \"Logística\",\n"
            "      \"url\": \"https://arstock.alrigroup.com\",\n"
            "      \"local_url\": \"http://localhost:3018\"\n"
            "    },\n"
            "    {\n"
            "      \"id\": \"arlogs\",\n"
            "      \"name\": \"ALRI-Logs\",\n"
            "      \"sigla\": \"ARLOGS\",\n"
            "      \"category\": \"Auditoria & Observabilidade\",\n"
            "      \"description\": \"Central de logs de segurança, telemetria e integridade\",\n"
            "      \"icon\": \"📜\",\n"
            "      \"badge\": \"Segurança\",\n"
            "      \"url\": \"https://arlogs.alrigroup.com\",\n"
            "      \"local_url\": \"http://localhost:3005\"\n"
            "    }%s\n"
            "  ]\n"
            "}\n",
            sess.user, sess.tenant, sess.role, sess.is_master ? "true" : "false",
            sess.is_master ? ",\n    {\n      \"id\": \"arctrl\",\n      \"name\": \"ALRI-Server Control\",\n      \"sigla\": \"ARCTRL\",\n      \"category\": \"Infraestrutura\",\n      \"description\": \"Gerenciamento de servidores, modo manutenção e daemons\",\n      \"icon\": \"⚡\",\n      \"badge\": \"Master\",\n      \"url\": \"https://arctrl.alrigroup.com\",\n      \"local_url\": \"http://localhost:3019\"\n    }" : "");

        send_http_response(client_fd, 200, "OK", "application/json", NULL, json);
        ar_socket_close(client_fd);
        return NULL;
    }

    /* 3. GET /arapi/work/summary (Workspace Summary & Stats) */
    if (strcmp(method, "GET") == 0 && strcmp(path, "/arapi/work/summary") == 0) {
        char *token = extract_session_token_from_request(buf);
        AuthSession sess;
        int auth_ok = (token && verify_auth_token(token, &sess) == 0 && sess.valid);
        if (token) free(token);

        if (!auth_ok) {
            send_http_response(client_fd, 401, "Unauthorized", "application/json", NULL, "{\"error\":\"Authentication required\"}\n");
            ar_socket_close(client_fd);
            return NULL;
        }

        char json[1024];
        snprintf(json, sizeof(json),
            "{\n"
            "  \"user\": \"%s\",\n"
            "  \"tenant\": \"%s\",\n"
            "  \"role\": \"%s\",\n"
            "  \"unread_notifications\": 0,\n"
            "  \"active_tasks\": 4,\n"
            "  \"open_tickets\": 1,\n"
            "  \"system_status\": \"operational\",\n"
            "  \"security_shield\": \"Zero-Trust RBAC Active\"\n"
            "}\n",
            sess.user, sess.tenant, sess.role);

        send_http_response(client_fd, 200, "OK", "application/json", NULL, json);
        ar_socket_close(client_fd);
        return NULL;
    }

    /* Fallback 404 */
    send_http_response(client_fd, 404, "Not Found", "application/json", NULL, "{\"error\":\"Endpoint not found in /arapi/work\"}\n");
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

int arapiwork_http_server_start(const char *bind_ip, int port) {
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
    alri_print(GRN "[ARAPIWORK]" RST " Workspace Hub API Backend listening on %s:%d (prefix: /arapi/work)\n", bind_ip ? bind_ip : "127.0.0.1", port);
    return 0;
}

void arapiwork_http_server_stop(void) {
    if (!g_http_running) return;
    g_http_running = 0;
    if (g_http_listen_fd >= 0) {
        ar_socket_close(g_http_listen_fd);
        g_http_listen_fd = -1;
    }
    alri_print(CYN "[ARAPIWORK]" RST " Workspace Hub API Backend stopped.\n");
}

int arapiwork_http_is_running(void) {
    return g_http_running;
}
