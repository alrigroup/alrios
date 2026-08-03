/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef SERVER_H
#define SERVER_H

typedef struct cJSON cJSON;

#define MODE_INSECURE 0
#define MODE_SECURE   1

typedef struct {
    const char *name;
    const char *value;
} HttpHeader;

typedef struct {
    const char *key;
    const char *value;
} PathParam;

typedef struct {
    const char *key;
    const char *value;
} QueryParam;

typedef struct {
    char *method;
    char *path;
    char *query_params;
    char *cookies;
    char *body;
    HttpHeader headers[100];
    int header_count;
    PathParam path_params[20];
    int path_param_count;
    QueryParam parsed_query[50];
    int query_count;
    cJSON *json_doc;
    int body_length_in_buffer;
    char host[256];
    int admin_role;
    char admin_user[64];
} HttpRequest;

typedef struct ClientConnection ClientConnection;
typedef void (*RequestHandler)(ClientConnection *conn, HttpRequest *req);
typedef void (*LoggerCallback)(const char *ip, const char *path, int status, const char *anon_id);

const char* get_header(HttpRequest *req, const char *header_name);
const char* get_query_param(HttpRequest *req, const char *key);
const char* get_path_param(HttpRequest *req, const char *key);
int server_start(int port, int mode, RequestHandler handler);
void server_set_logger(LoggerCallback callback);
void server_set_bind_address(const char *addr);
void server_stop(void);
void server_send_response(ClientConnection *conn, int status, const char *content_type, const char *body);
void server_add_header(ClientConnection *conn, const char *header_line);
void server_redirect(ClientConnection *conn, const char *url);
const char* server_get_client_ip(ClientConnection *conn);
int  server_serve_file(ClientConnection *conn, const char *filepath, const char *content_type);
void server_send_404(ClientConnection *conn);
cJSON* parse_json_body(HttpRequest *req);
void server_send_json(ClientConnection *conn, int status, cJSON *json_obj);
int  server_conn_read(ClientConnection *conn, void *buf, int num);
int  server_conn_write(ClientConnection *conn, const void *buf, int num);
int  server_conn_get_fd(ClientConnection *conn);
void server_conn_close(ClientConnection *conn);

#endif
