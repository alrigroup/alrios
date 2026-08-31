/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arapistock_http.h"
#include "arapistock_config.h"
#include "arapistock_db.h"
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

    ArapistockConfig *cfg = arapistock_config_get();
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

static void log_stock_audit(const char *user, const char *tenant, const char *action, const char *severity, int status, const char *details) {
    ArapistockConfig *cfg = arapistock_config_get();
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
        "{\"user\":\"%s\",\"tenant\":\"%s\",\"service\":\"arstock\",\"action\":\"%s\",\"severity\":\"%s\",\"status_code\":%d,\"details\":\"%s\"}",
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

    /* 1. GET /arapi/stock/status */
    if (strcmp(method, "GET") == 0 &&
        (strcmp(path, "/arapi/stock/status") == 0 || strcmp(path, "/arapi/stock/health") == 0 || strcmp(path, "/arapi/stock/") == 0)) {
        char json[512];
        snprintf(json, sizeof(json),
            "{\"status\":\"online\",\"backend\":\"arapistock\",\"prefix\":\"/arapi/stock\","
            "\"module\":\"ALRI Sovereign Inventory & Assets Management\"}\n");
        send_http_response(client_fd, 200, "OK", "application/json", NULL, json);
        ar_socket_close(client_fd);
        return NULL;
    }

    // Authenticate token for all inventory endpoints
    char *token = extract_session_token_from_request(buf);
    AuthSession sess;
    int auth_ok = (token && verify_auth_token(token, &sess) == 0 && sess.valid);
    if (token) free(token);

    if (!auth_ok) {
        send_http_response(client_fd, 401, "Unauthorized", "application/json", NULL, "{\"error\":\"Authentication required\"}\n");
        ar_socket_close(client_fd);
        return NULL;
    }

    /* 2. GET /arapi/stock/items */
    if (strcmp(method, "GET") == 0 && strncmp(path, "/arapi/stock/items", 18) == 0) {
        char *json = arapistock_db_list_items_json(sess.tenant, sess.is_master);
        send_http_response(client_fd, 200, "OK", "application/json", NULL, json);
        free(json);
        ar_socket_close(client_fd);
        return NULL;
    }

    /* 3. POST /arapi/stock/items/create */
    if (strcmp(method, "POST") == 0 && strcmp(path, "/arapi/stock/items/create") == 0) {
        char *sku = extract_json_string(body, "sku");
        char *comp = extract_json_string(body, "company_id");
        char *name = extract_json_string(body, "name");
        char *cat = extract_json_string(body, "category");
        char *qty_str = extract_json_string(body, "quantity");
        char *min_str = extract_json_string(body, "min_quantity");
        char *cost_str = extract_json_string(body, "unit_cost");
        char *loc = extract_json_string(body, "location");

        const char *target_comp = (comp && comp[0]) ? comp : sess.tenant;

        if (!sess.is_master && strcmp(target_comp, sess.tenant) != 0) {
            send_http_response(client_fd, 403, "Forbidden", "application/json", NULL, "{\"error\":\"Você só pode cadastrar itens para a sua própria empresa\"}\n");
            if (sku) free(sku); if (comp) free(comp); if (name) free(name); if (cat) free(cat);
            if (qty_str) free(qty_str); if (min_str) free(min_str); if (cost_str) free(cost_str); if (loc) free(loc);
            ar_socket_close(client_fd);
            return NULL;
        }

        int qty = qty_str ? atoi(qty_str) : 0;
        int min_qty = min_str ? atoi(min_str) : 1;
        double cost = cost_str ? atof(cost_str) : 0.0;

        arapistock_db_create_item(sku, target_comp, name, cat, qty, min_qty, cost, loc);
        log_stock_audit(sess.user, target_comp, "item_create", "INFO", 201, sku);

        if (sku) free(sku); if (comp) free(comp); if (name) free(name); if (cat) free(cat);
        if (qty_str) free(qty_str); if (min_str) free(min_str); if (cost_str) free(cost_str); if (loc) free(loc);

        send_http_response(client_fd, 201, "Created", "application/json", NULL, "{\"status\":\"success\",\"message\":\"Item cadastrado no estoque\"}\n");
        ar_socket_close(client_fd);
        return NULL;
    }

    /* 4. POST /arapi/stock/items/movement */
    if (strcmp(method, "POST") == 0 && strcmp(path, "/arapi/stock/items/movement") == 0) {
        char *item_id = extract_json_string(body, "item_id");
        char *type = extract_json_string(body, "type");
        char *qty_str = extract_json_string(body, "qty_change");
        char *reason = extract_json_string(body, "reason");

        int qty_change = qty_str ? atoi(qty_str) : 0;

        if (!item_id || !type || qty_change <= 0) {
            send_http_response(client_fd, 400, "Bad Request", "application/json", NULL, "{\"error\":\"Parâmetros inválidos para movimentação\"}\n");
            if (item_id) free(item_id); if (type) free(type); if (qty_str) free(qty_str); if (reason) free(reason);
            ar_socket_close(client_fd);
            return NULL;
        }

        int res = arapistock_db_record_movement(item_id, type, qty_change, reason, sess.user, sess.tenant, sess.is_master);
        if (res == -2) {
            send_http_response(client_fd, 403, "Forbidden", "application/json", NULL, "{\"error\":\"Sem permissão para movimentar estoque de outra empresa\"}\n");
        } else if (res == -3) {
            send_http_response(client_fd, 400, "Bad Request", "application/json", NULL, "{\"error\":\"Saldo de estoque insuficiente para saída\"}\n");
        } else if (res == -1) {
            send_http_response(client_fd, 404, "Not Found", "application/json", NULL, "{\"error\":\"Item não encontrado\"}\n");
        } else {
            log_stock_audit(sess.user, sess.tenant, "stock_movement", "INFO", 200, item_id);
            send_http_response(client_fd, 200, "OK", "application/json", NULL, "{\"status\":\"success\",\"message\":\"Movimentação registrada com sucesso\"}\n");
        }

        if (item_id) free(item_id); if (type) free(type); if (qty_str) free(qty_str); if (reason) free(reason);
        ar_socket_close(client_fd);
        return NULL;
    }

    /* 5. GET /arapi/stock/summary */
    if (strcmp(method, "GET") == 0 && strncmp(path, "/arapi/stock/summary", 20) == 0) {
        char *json = arapistock_db_get_summary_json(sess.tenant, sess.is_master);
        send_http_response(client_fd, 200, "OK", "application/json", NULL, json);
        free(json);
        ar_socket_close(client_fd);
        return NULL;
    }

    /* Fallback 404 */
    send_http_response(client_fd, 404, "Not Found", "application/json", NULL, "{\"error\":\"Endpoint not found in /arapi/stock\"}\n");
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

int arapistock_http_server_start(const char *bind_ip, int port) {
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
    alri_print(GRN "[ARAPISTOCK]" RST " Inventory Backend listening on %s:%d (prefix: /arapi/stock)\n", bind_ip ? bind_ip : "127.0.0.1", port);
    return 0;
}

void arapistock_http_server_stop(void) {
    if (!g_http_running) return;
    g_http_running = 0;
    if (g_http_listen_fd >= 0) {
        ar_socket_close(g_http_listen_fd);
        g_http_listen_fd = -1;
    }
    alri_print(CYN "[ARAPISTOCK]" RST " Inventory Backend stopped.\n");
}

int arapistock_http_is_running(void) {
    return g_http_running;
}
