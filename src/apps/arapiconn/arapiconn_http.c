/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arapiconn_http.h"
#include "arapiconn_config.h"
#include "arapiconn_db.h"
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

    ArapiconnConfig *cfg = arapiconn_config_get();
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

static void log_conn_audit(const char *user, const char *tenant, const char *action, const char *severity, int status, const char *details) {
    ArapiconnConfig *cfg = arapiconn_config_get();
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
        "{\"user\":\"%s\",\"tenant\":\"%s\",\"service\":\"arconn\",\"action\":\"%s\",\"severity\":\"%s\",\"status_code\":%d,\"details\":\"%s\"}",
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

    /* 1. GET /arapi/conn/status or /health */
    if (strcmp(method, "GET") == 0 &&
        (strcmp(path, "/arapi/conn/status") == 0 || strcmp(path, "/arapi/conn/health") == 0 || strcmp(path, "/arapi/conn/") == 0)) {
        char json[512];
        snprintf(json, sizeof(json),
            "{\"status\":\"online\",\"backend\":\"arapiconn\",\"prefix\":\"/arapi/conn\","
            "\"features\":[\"channels\",\"messages\",\"kanban\",\"private_dms\"]}\n");
        send_http_response(client_fd, 200, "OK", "application/json", NULL, json);
        ar_socket_close(client_fd);
        return NULL;
    }

    // Authenticate token for all communication endpoints
    char *token = extract_session_token_from_request(buf);
    AuthSession sess;
    int auth_ok = (token && verify_auth_token(token, &sess) == 0 && sess.valid);
    if (token) free(token);

    if (!auth_ok) {
        send_http_response(client_fd, 401, "Unauthorized", "application/json", NULL, "{\"error\":\"Authentication required\"}\n");
        ar_socket_close(client_fd);
        return NULL;
    }

    /* 2. GET /arapi/conn/channels */
    if (strcmp(method, "GET") == 0 && strncmp(path, "/arapi/conn/channels", 20) == 0) {
        char *json = arapiconn_db_list_channels_json(sess.tenant, sess.is_master);
        send_http_response(client_fd, 200, "OK", "application/json", NULL, json);
        free(json);
        ar_socket_close(client_fd);
        return NULL;
    }

    /* 3. POST /arapi/conn/channels/create */
    if (strcmp(method, "POST") == 0 && strcmp(path, "/arapi/conn/channels/create") == 0) {
        char *comp = extract_json_string(body, "company_id");
        char *name = extract_json_string(body, "name");
        char *topic = extract_json_string(body, "topic");
        const char *target_comp = (comp && comp[0]) ? comp : sess.tenant;

        if (!sess.is_master && strcmp(target_comp, sess.tenant) != 0) {
            send_http_response(client_fd, 403, "Forbidden", "application/json", NULL, "{\"error\":\"Você só pode criar canais na sua própria empresa\"}\n");
            if (comp) free(comp); if (name) free(name); if (topic) free(topic);
            ar_socket_close(client_fd);
            return NULL;
        }

        arapiconn_db_create_channel(target_comp, name, topic, 0, sess.user);
        log_conn_audit(sess.user, target_comp, "channel_create", "INFO", 201, name);

        if (comp) free(comp); if (name) free(name); if (topic) free(topic);
        send_http_response(client_fd, 201, "Created", "application/json", NULL, "{\"status\":\"success\",\"message\":\"Canal criado com sucesso\"}\n");
        ar_socket_close(client_fd);
        return NULL;
    }

    /* 4. GET /arapi/conn/messages?channel=... */
    if (strcmp(method, "GET") == 0 && strncmp(path, "/arapi/conn/messages", 20) == 0) {
        char chan[64] = "geral";
        char *q = strstr(path, "channel=");
        if (q) sscanf(q + 8, "%63[^& \t\r\n]", chan);

        char *json = arapiconn_db_list_messages_json(chan, 100);
        send_http_response(client_fd, 200, "OK", "application/json", NULL, json);
        free(json);
        ar_socket_close(client_fd);
        return NULL;
    }

    /* 5. POST /arapi/conn/messages/post */
    if (strcmp(method, "POST") == 0 && strcmp(path, "/arapi/conn/messages/post") == 0) {
        char *chan = extract_json_string(body, "channel_id");
        char *content = extract_json_string(body, "content");

        if (!chan || !content) {
            if (chan) free(chan); if (content) free(content);
            send_http_response(client_fd, 400, "Bad Request", "application/json", NULL, "{\"error\":\"Campos 'channel_id' e 'content' obrigatórios\"}\n");
            ar_socket_close(client_fd);
            return NULL;
        }

        arapiconn_db_post_message(chan, sess.user, sess.tenant, content);
        // Public channel message metadata logged (content not full dump)
        log_conn_audit(sess.user, sess.tenant, "channel_message_post", "INFO", 201, chan);

        free(chan); free(content);
        send_http_response(client_fd, 201, "Created", "application/json", NULL, "{\"status\":\"success\",\"message\":\"Mensagem enviada\"}\n");
        ar_socket_close(client_fd);
        return NULL;
    }

    /* 6. GET /arapi/conn/tasks */
    if (strcmp(method, "GET") == 0 && strncmp(path, "/arapi/conn/tasks", 17) == 0) {
        char *json = arapiconn_db_list_tasks_json(sess.tenant, sess.is_master);
        send_http_response(client_fd, 200, "OK", "application/json", NULL, json);
        free(json);
        ar_socket_close(client_fd);
        return NULL;
    }

    /* 7. POST /arapi/conn/tasks/create */
    if (strcmp(method, "POST") == 0 && strcmp(path, "/arapi/conn/tasks/create") == 0) {
        char *comp = extract_json_string(body, "company_id");
        char *title = extract_json_string(body, "title");
        char *desc = extract_json_string(body, "description");
        char *assignee = extract_json_string(body, "assignee");
        char *prio = extract_json_string(body, "priority");

        const char *target_comp = (comp && comp[0]) ? comp : sess.tenant;

        if (!sess.is_master && strcmp(target_comp, sess.tenant) != 0) {
            send_http_response(client_fd, 403, "Forbidden", "application/json", NULL, "{\"error\":\"Você só pode criar tarefas na sua própria empresa\"}\n");
            if (comp) free(comp); if (title) free(title); if (desc) free(desc); if (assignee) free(assignee); if (prio) free(prio);
            ar_socket_close(client_fd);
            return NULL;
        }

        arapiconn_db_create_task(target_comp, title, desc, assignee, prio, sess.user);
        log_conn_audit(sess.user, target_comp, "task_create", "INFO", 201, title);

        if (comp) free(comp); if (title) free(title); if (desc) free(desc); if (assignee) free(assignee); if (prio) free(prio);
        send_http_response(client_fd, 201, "Created", "application/json", NULL, "{\"status\":\"success\",\"message\":\"Tarefa criada\"}\n");
        ar_socket_close(client_fd);
        return NULL;
    }

    /* 8. POST /arapi/conn/tasks/update */
    if (strcmp(method, "POST") == 0 && strcmp(path, "/arapi/conn/tasks/update") == 0) {
        char *task_id = extract_json_string(body, "task_id");
        char *new_status = extract_json_string(body, "status");

        if (!task_id || !new_status) {
            if (task_id) free(task_id); if (new_status) free(new_status);
            send_http_response(client_fd, 400, "Bad Request", "application/json", NULL, "{\"error\":\"Campos 'task_id' e 'status' obrigatórios\"}\n");
            ar_socket_close(client_fd);
            return NULL;
        }

        arapiconn_db_update_task_status(task_id, new_status);
        log_conn_audit(sess.user, sess.tenant, "task_update", "INFO", 200, task_id);

        free(task_id); free(new_status);
        send_http_response(client_fd, 200, "OK", "application/json", NULL, "{\"status\":\"success\",\"message\":\"Status atualizado\"}\n");
        ar_socket_close(client_fd);
        return NULL;
    }

    /* 9. GET /arapi/conn/dms?with=... (Direct Messages — 100% Confidential) */
    if (strcmp(method, "GET") == 0 && strncmp(path, "/arapi/conn/dms", 15) == 0) {
        char target_user[64] = {0};
        char *q = strstr(path, "with=");
        if (q) sscanf(q + 5, "%63[^& \t\r\n]", target_user);

        if (!target_user[0]) {
            send_http_response(client_fd, 400, "Bad Request", "application/json", NULL, "{\"error\":\"Parâmetro '?with=username' obrigatório\"}\n");
            ar_socket_close(client_fd);
            return NULL;
        }

        char *json = arapiconn_db_list_dms_json(sess.user, target_user);
        send_http_response(client_fd, 200, "OK", "application/json", NULL, json);
        free(json);
        ar_socket_close(client_fd);
        return NULL;
    }

    /* 10. POST /arapi/conn/dms/send (Direct Message Send — NO ARLOGS LOGGING) */
    if (strcmp(method, "POST") == 0 && strcmp(path, "/arapi/conn/dms/send") == 0) {
        char *recipient = extract_json_string(body, "recipient");
        char *content = extract_json_string(body, "content");

        if (!recipient || !content) {
            if (recipient) free(recipient); if (content) free(content);
            send_http_response(client_fd, 400, "Bad Request", "application/json", NULL, "{\"error\":\"Campos 'recipient' e 'content' obrigatórios\"}\n");
            ar_socket_close(client_fd);
            return NULL;
        }

        arapiconn_db_send_dm(sess.user, recipient, content);
        // CRITICAL: NEVER call log_conn_audit for direct messages to guarantee absolute privacy

        free(recipient); free(content);
        send_http_response(client_fd, 201, "Created", "application/json", NULL, "{\"status\":\"success\",\"message\":\"Mensagem privada enviada com sigilo absoluto\"}\n");
        ar_socket_close(client_fd);
        return NULL;
    }

    /* Fallback 404 */
    send_http_response(client_fd, 404, "Not Found", "application/json", NULL, "{\"error\":\"Endpoint not found in /arapi/conn\"}\n");
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

int arapiconn_http_server_start(const char *bind_ip, int port) {
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
    alri_print(GRN "[ARAPICONN]" RST " Communication & Kanban API Backend listening on %s:%d (prefix: /arapi/conn)\n", bind_ip ? bind_ip : "127.0.0.1", port);
    return 0;
}

void arapiconn_http_server_stop(void) {
    if (!g_http_running) return;
    g_http_running = 0;
    if (g_http_listen_fd >= 0) {
        ar_socket_close(g_http_listen_fd);
        g_http_listen_fd = -1;
    }
    alri_print(CYN "[ARAPICONN]" RST " Communication & Kanban API Backend stopped.\n");
}

int arapiconn_http_is_running(void) {
    return g_http_running;
}
