/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arapichat_http.h"
#include "arapichat_config.h"
#include "arapichat_db.h"
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

    ArapichatConfig *cfg = arapichat_config_get();
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

static void log_chat_audit(const char *user, const char *tenant, const char *action, const char *details) {
    ArapichatConfig *cfg = arapichat_config_get();
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
        "{\"user\":\"%s\",\"tenant\":\"%s\",\"service\":\"archat\",\"action\":\"%s\",\"severity\":\"INFO\",\"status_code\":200,\"details\":\"%s\"}",
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

    char *body = strstr(buf, "\r\n\r\n");
    if (body) body += 4;

    /* 1. GET /arapi/chat/status or /health */
    if (strcmp(method, "GET") == 0 &&
        (strcmp(path, "/arapi/chat/status") == 0 || strcmp(path, "/arapi/chat/health") == 0 || strcmp(path, "/arapi/chat/") == 0)) {
        char json[512];
        snprintf(json, sizeof(json),
            "{\"status\":\"online\",\"backend\":\"arapichat\",\"prefix\":\"/arapi/chat\","
            "\"service\":\"ALRI Sovereign Customer Support & Tickets API\"}\n");
        send_http_response(client_fd, 200, "OK", "application/json", NULL, json);
        ar_socket_close(client_fd);
        return NULL;
    }

    /* 2. POST /arapi/chat/tickets/create (Public Support Ticket Submission) */
    if (strcmp(method, "POST") == 0 && strcmp(path, "/arapi/chat/tickets/create") == 0) {
        char *comp = extract_json_string(body, "company_id");
        char *name = extract_json_string(body, "customer_name");
        char *email = extract_json_string(body, "customer_email");
        char *subject = extract_json_string(body, "subject");
        char *msg = extract_json_string(body, "message");

        const char *target_comp = (comp && comp[0]) ? comp : "detroitgg";
        if (!name || !subject) {
            if (comp) free(comp); if (name) free(name); if (email) free(email); if (subject) free(subject); if (msg) free(msg);
            send_http_response(client_fd, 400, "Bad Request", "application/json", NULL, "{\"error\":\"Campos 'customer_name' e 'subject' obrigatórios\"}\n");
            ar_socket_close(client_fd);
            return NULL;
        }

        arapichat_db_create_ticket(target_comp, name, email, subject, msg);
        log_chat_audit(name, target_comp, "ticket_create", subject);

        if (comp) free(comp); if (name) free(name); if (email) free(email); if (subject) free(subject); if (msg) free(msg);
        send_http_response(client_fd, 201, "Created", "application/json", NULL, "{\"status\":\"success\",\"message\":\"Chamado aberto com sucesso\"}\n");
        ar_socket_close(client_fd);
        return NULL;
    }

    // Authenticate staff token for operator endpoints
    char *token = extract_session_token_from_request(buf);
    AuthSession sess;
    int auth_ok = (token && verify_auth_token(token, &sess) == 0 && sess.valid);
    if (token) free(token);

    if (!auth_ok) {
        send_http_response(client_fd, 401, "Unauthorized", "application/json", NULL, "{\"error\":\"Authentication required\"}\n");
        ar_socket_close(client_fd);
        return NULL;
    }

    /* 3. GET /arapi/chat/tickets */
    if (strcmp(method, "GET") == 0 && strncmp(path, "/arapi/chat/tickets", 19) == 0) {
        char *json = arapichat_db_list_tickets_json(sess.tenant, sess.is_master);
        send_http_response(client_fd, 200, "OK", "application/json", NULL, json);
        free(json);
        ar_socket_close(client_fd);
        return NULL;
    }

    /* 4. GET /arapi/chat/messages?ticket_id=... */
    if (strcmp(method, "GET") == 0 && strncmp(path, "/arapi/chat/messages", 20) == 0) {
        char tkt_id[64] = {0};
        char *q = strstr(path, "ticket_id=");
        if (q) sscanf(q + 10, "%63[^& \t\r\n]", tkt_id);

        char *json = arapichat_db_list_messages_json(tkt_id);
        send_http_response(client_fd, 200, "OK", "application/json", NULL, json);
        free(json);
        ar_socket_close(client_fd);
        return NULL;
    }

    /* 5. POST /arapi/chat/tickets/reply */
    if (strcmp(method, "POST") == 0 && strcmp(path, "/arapi/chat/tickets/reply") == 0) {
        char *tkt_id = extract_json_string(body, "ticket_id");
        char *content = extract_json_string(body, "content");

        if (!tkt_id || !content) {
            if (tkt_id) free(tkt_id); if (content) free(content);
            send_http_response(client_fd, 400, "Bad Request", "application/json", NULL, "{\"error\":\"Campos 'ticket_id' e 'content' obrigatórios\"}\n");
            ar_socket_close(client_fd);
            return NULL;
        }

        arapichat_db_add_message(tkt_id, "staff", sess.user, content);
        log_chat_audit(sess.user, sess.tenant, "ticket_reply", tkt_id);

        free(tkt_id); free(content);
        send_http_response(client_fd, 201, "Created", "application/json", NULL, "{\"status\":\"success\",\"message\":\"Resposta enviada\"}\n");
        ar_socket_close(client_fd);
        return NULL;
    }

    /* 6. POST /arapi/chat/tickets/close */
    if (strcmp(method, "POST") == 0 && strcmp(path, "/arapi/chat/tickets/close") == 0) {
        char *tkt_id = extract_json_string(body, "ticket_id");
        if (!tkt_id) {
            send_http_response(client_fd, 400, "Bad Request", "application/json", NULL, "{\"error\":\"Campo 'ticket_id' obrigatório\"}\n");
            ar_socket_close(client_fd);
            return NULL;
        }

        arapichat_db_update_ticket_status(tkt_id, "resolved");
        log_chat_audit(sess.user, sess.tenant, "ticket_resolve", tkt_id);

        free(tkt_id);
        send_http_response(client_fd, 200, "OK", "application/json", NULL, "{\"status\":\"success\",\"message\":\"Chamado resolvido\"}\n");
        ar_socket_close(client_fd);
        return NULL;
    }

    /* Fallback 404 */
    send_http_response(client_fd, 404, "Not Found", "application/json", NULL, "{\"error\":\"Endpoint not found in /arapi/chat\"}\n");
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

int arapichat_http_server_start(const char *bind_ip, int port) {
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
    alri_print(GRN "[ARAPICHAT]" RST " Support & Tickets API Backend listening on %s:%d (prefix: /arapi/chat)\n", bind_ip ? bind_ip : "127.0.0.1", port);
    return 0;
}

void arapichat_http_server_stop(void) {
    if (!g_http_running) return;
    g_http_running = 0;
    if (g_http_listen_fd >= 0) {
        ar_socket_close(g_http_listen_fd);
        g_http_listen_fd = -1;
    }
    alri_print(CYN "[ARAPICHAT]" RST " Support & Tickets API Backend stopped.\n");
}

int arapichat_http_is_running(void) {
    return g_http_running;
}
