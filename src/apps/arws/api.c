/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "api.h"
#include "cJSON.h"
#include "aros_hal.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <stdlib.h>

static int path_is_safe(const char *path) {
    if (!path || path[0] == '\0') return 0;
    if (strstr(path, "..")) return 0;
    if (strchr(path, '\\')) return 0;
    if (strchr(path, '\0') && strlen(path) > 0) {
        size_t len = strlen(path);
        if (path[len - 1] == '.') return 0;
    }
    return 1;
}

#define ROUTE_HASH_SIZE 128
static Route *route_hash[ROUTE_HASH_SIZE] = {0};

static unsigned int hash_route(const char *path, const char *method) {
    unsigned int hash = 5381;
    int c;
    while ((c = *path++)) hash = ((hash << 5) + hash) + c;
    while ((c = *method++)) hash = ((hash << 5) + hash) + c;
    return hash % ROUTE_HASH_SIZE;
}

void api_add_route(const char *path, const char *method, RouteHandler handler) {
    Route *new_route = (Route *)ar_mem_alloc(sizeof(Route));
    strncpy(new_route->path, path, sizeof(new_route->path) - 1);
    strncpy(new_route->method, method, sizeof(new_route->method) - 1);
    new_route->handler = handler;
    unsigned int idx = hash_route(path, method);
    new_route->next = route_hash[idx];
    route_hash[idx] = new_route;
}

void send_page(ClientConnection *conn, const char *folder_name, const char *request_path) {
    if (!path_is_safe(request_path)) { server_send_404(conn); return; }
    char full_path[512];
    if (strchr(request_path, '.') == NULL) {
        snprintf(full_path, sizeof(full_path), "www/%s/%s.html", folder_name, folder_name);
        server_serve_file(conn, full_path, "text/html");
    } else if (strstr(request_path, ".js")) {
        const char *filename = strrchr(request_path, '/');
        filename = filename ? filename + 1 : request_path;
        if (!path_is_safe(filename)) { server_send_404(conn); return; }
        snprintf(full_path, sizeof(full_path), "www/%s/%s", folder_name, filename);
        server_serve_file(conn, full_path, "application/javascript");
    } else if (strstr(request_path, ".css")) {
        const char *filename = strrchr(request_path, '/');
        filename = filename ? filename + 1 : request_path;
        if (!path_is_safe(filename)) { server_send_404(conn); return; }
        snprintf(full_path, sizeof(full_path), "www/%s/%s", folder_name, filename);
        server_serve_file(conn, full_path, "text/css");
    }
}

static int serve_static_file(ClientConnection *conn, const char *path) {
    if (!path_is_safe(path)) return 0;
    const char *ext = strrchr(path, '.');
    if (!ext) return 0;
    char filepath[512];
    const char *clean_path = (path[0] == '/') ? path + 1 : path;
    snprintf(filepath, sizeof(filepath), "www/%s", clean_path);
    const char *content_type = "text/plain";
    if (strcmp(ext, ".html") == 0) content_type = "text/html";
    else if (strcmp(ext, ".css") == 0) content_type = "text/css";
    else if (strcmp(ext, ".js") == 0) content_type = "application/javascript";
    else if (strcmp(ext, ".png") == 0) content_type = "image/png";
    else if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) content_type = "image/jpeg";
    return server_serve_file(conn, filepath, content_type);
}

static void route_404(ClientConnection *conn, HttpRequest *req) {
    (void)req;
    server_send_404(conn);
}

void api_plugin_handler(ClientConnection *conn, HttpRequest *req) {
    if (serve_static_file(conn, req->path)) return;
    unsigned int idx = hash_route(req->path, req->method);
    Route *current = route_hash[idx];
    int found = 0;
    while (current != NULL) {
        if (strcmp(current->path, req->path) == 0 && strcmp(current->method, req->method) == 0) {
            current->handler(conn, req);
            found = 1;
            break;
        }
        current = current->next;
    }
    if (!found) route_404(conn, req);
}

static void sendpage(ClientConnection *conn, const char *folder_name) {
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "www/%s/index.html", folder_name);
    if (access(full_path, F_OK) != 0) {
        snprintf(full_path, sizeof(full_path), "www/%s/main.html", folder_name);
        if (access(full_path, F_OK) != 0) {
            snprintf(full_path, sizeof(full_path), "www/%s/dist/index.html", folder_name);
        }
    }
    server_serve_file(conn, full_path, "text/html");
}

static void home_handler(ClientConnection *conn, HttpRequest *req) {
    sendpage(conn, "home");
}

static void api_data_handler(ClientConnection *conn, HttpRequest *req) {
    const char *json = "{\"status\": \"success\", \"message\": \"Bemf HTTP server running on ALRIOS!\"}";
    server_send_response(conn, 200, "application/json", json);
}

static void manager_login_handler(ClientConnection *conn, HttpRequest *req) {
    sendpage(conn, "manager/login");
}

static void manager_dashboard_handler(ClientConnection *conn, HttpRequest *req) {
    sendpage(conn, "manager/dashboard");
}

void api_plugin_init() {
    alri_print_force(CYN "[API]" RST " Initializing Bemf routes...\n");
    api_add_route("/", "GET", home_handler);
    api_add_route("/home", "GET", home_handler);
    api_add_route("/manager/login", "GET", manager_login_handler);
    api_add_route("/manager/dashboard", "GET", manager_dashboard_handler);
    api_add_route("/api/data", "GET", api_data_handler);
}
