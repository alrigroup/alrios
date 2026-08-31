/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "ardb_http.h"
#include "ardb_config.h"
#include "ardb_auth.h"
#include "ardb_firewall.h"
#include "ardb_audit.h"
#include "ardb_backend.h"
#include "ardb_storage_engine.h"
#include "aros_hal.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int g_http_listen_fd = -1;
static volatile int g_http_running = 0;
static void *g_http_thread = NULL;

static void send_http_response(int client_fd, int status_code, const char *status_text,
                               const char *content_type, const char *body) {
    size_t body_len = body ? strlen(body) : 0;
    char header[512];
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization, X-ARDB-Token\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Connection: close\r\n"
        "\r\n",
        status_code, status_text, content_type, body_len);

    ar_socket_send(client_fd, header, hlen);
    if (body_len > 0) {
        ar_socket_send(client_fd, body, (int)body_len);
    }
}

static char* extract_json_string(const char *json, const char *key) {
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
    }
    return NULL;
}

static void* http_client_worker(void *arg) {
    int client_fd = (int)(intptr_t)arg;
    char buf[8192];
    int r = ar_socket_recv(client_fd, buf, sizeof(buf) - 1);
    if (r <= 0) {
        ar_socket_close(client_fd);
        return NULL;
    }
    buf[r] = '\0';

    char method[16] = {0};
    char path[256] = {0};
    sscanf(buf, "%15s %255s", method, path);

    /* OPTIONS (CORS preflight) */
    if (strcmp(method, "OPTIONS") == 0) {
        send_http_response(client_fd, 204, "No Content", "text/plain", "");
        ar_socket_close(client_fd);
        return NULL;
    }

    ArdbConfig *cfg = ardb_config_get();

    /* 1. GET /api/v1/db/status ou /status */
    if (strcmp(method, "GET") == 0 &&
        (strstr(path, "/status") || strstr(path, "/health"))) {
        char resp_json[512];
        snprintf(resp_json, sizeof(resp_json),
            "{\"status\":\"online\",\"engine\":\"ALRI DB Sovereign Guardian\","
            "\"pgwire_port\":%d,\"http_port\":%d,\"sql_firewall\":\"%s\",\"audit\":\"active\"}\n",
            cfg->server_port, cfg->http_port, cfg->sql_firewall);
        send_http_response(client_fd, 200, "OK", "application/json", resp_json);
        ar_socket_close(client_fd);
        return NULL;
    }

    /* 2. POST /api/v1/db/query ou /query */
    if (strcmp(method, "POST") == 0 &&
        (strstr(path, "/query") || strstr(path, "/execute"))) {

        /* Extrair Token de Autenticação */
        char token[128] = {0};
        char *tok_hdr = strstr(buf, "X-ARDB-Token:");
        if (!tok_hdr) tok_hdr = strstr(buf, "x-ardb-token:");
        if (tok_hdr) {
            sscanf(tok_hdr + 13, "%127s", token);
        } else {
            char *auth_hdr = strstr(buf, "Authorization: Bearer ");
            if (!auth_hdr) auth_hdr = strstr(buf, "authorization: bearer ");
            if (auth_hdr) {
                sscanf(auth_hdr + 22, "%127s", token);
            }
        }

        char user[64] = "anonymous";
        char tenant[64] = "default";
        char role[32] = "operator";

        if (cfg->http_auth_required) {
            if (token[0] == '\0') {
                send_http_response(client_fd, 401, "Unauthorized", "application/json",
                    "{\"error\":\"Missing X-ARDB-Token or Authorization Bearer header\"}\n");
                ar_socket_close(client_fd);
                return NULL;
            }

            if (ardb_auth_verify_token(token, user, tenant, role) != 0) {
                send_http_response(client_fd, 401, "Unauthorized", "application/json",
                    "{\"error\":\"Invalid or expired session token\"}\n");
                ar_socket_close(client_fd);
                return NULL;
            }
        }

        /* Obter body JSON */
        char *body = strstr(buf, "\r\n\r\n");
        if (!body) {
            send_http_response(client_fd, 400, "Bad Request", "application/json", "{\"error\":\"Missing body\"}\n");
            ar_socket_close(client_fd);
            return NULL;
        }
        body += 4;

        char *sql = extract_json_string(body, "sql");
        if (!sql || sql[0] == '\0') {
            if (sql) free(sql);
            send_http_response(client_fd, 400, "Bad Request", "application/json", "{\"error\":\"Missing 'sql' in JSON body\"}\n");
            ar_socket_close(client_fd);
            return NULL;
        }

        uint64_t start_us = (uint64_t)ar_time_ms() * 1000;
        char rewritten[2048];
        char reason[512] = {0};

        ArdbFwAction act = ardb_firewall_inspect(sql, tenant, role,
                                                 rewritten, sizeof(rewritten),
                                                 reason, sizeof(reason));

        if (act != ARDB_FW_OK) {
            ardb_audit_log_query(user, tenant, "127.0.0.1",
                                 sql, 403, (uint64_t)ar_time_ms() * 1000 - start_us);
            char err_json[512];
            snprintf(err_json, sizeof(err_json), "{\"error\":\"SQL Firewall Blocked: %s\"}\n", reason);
            send_http_response(client_fd, 403, "Forbidden", "application/json", err_json);
            free(sql);
            ar_socket_close(client_fd);
            return NULL;
        }

        /* Execução da consulta via ARDB Storage Engine */
        ArdbQueryResult qres;
        ardb_storage_execute_query("postgres", rewritten, &qres);

        char resp_body[16384] = "{\"status\":\"success\",\"rows\":[";
        for (int r = 0; r < qres.row_count; r++) {
            if (r > 0) strcat(resp_body, ",");
            strcat(resp_body, "{");
            for (int c = 0; c < qres.column_count; c++) {
                if (c > 0) strcat(resp_body, ",");
                char item[1024];
                snprintf(item, sizeof(item), "\"%s\":\"%s\"", qres.columns[c].name,
                         qres.rows[r].fields[c] ? qres.rows[r].fields[c] : "");
                strcat(resp_body, item);
            }
            strcat(resp_body, "}");
            if (strlen(resp_body) > 15000) break;
        }
        char tail[128];
        snprintf(tail, sizeof(tail), "],\"row_count\":%d}\n", qres.row_count);
        strcat(resp_body, tail);
        ardb_storage_free_result(&qres);

        ardb_audit_log_query(user, tenant, "127.0.0.1",
                             rewritten, 200, (uint64_t)ar_time_ms() * 1000 - start_us);

        send_http_response(client_fd, 200, "OK", "application/json", resp_body);
        free(sql);
        ar_socket_close(client_fd);
        return NULL;
    }

    send_http_response(client_fd, 404, "Not Found", "application/json", "{\"error\":\"Route not found\"}\n");
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

int ardb_http_server_start(const char *bind_ip, int port) {
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
    alri_print(GRN "[ARDB-HTTP]" RST " REST API Gateway listening on %s:%d\n", bind_ip ? bind_ip : "127.0.0.1", port);
    return 0;
}

void ardb_http_server_stop(void) {
    if (!g_http_running) return;
    g_http_running = 0;
    if (g_http_listen_fd >= 0) {
        ar_socket_close(g_http_listen_fd);
        g_http_listen_fd = -1;
    }
    alri_print(CYN "[ARDB-HTTP]" RST " REST API Gateway stopped.\n");
}

int ardb_http_is_running(void) {
    return g_http_running;
}
