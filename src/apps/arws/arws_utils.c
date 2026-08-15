/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arws_utils.h"
#include "arws_web_data.h"
#include "server.h"
#include "aros_hal.h"
#include <string.h>
#include <stdio.h>

static void json_escape(char *dst, size_t dst_size, const char *src) {
    if (!src) { dst[0] = '\0'; return; }
    size_t di = 0;
    for (size_t si = 0; src[si] != '\0' && di < dst_size - 1; si++) {
        unsigned char c = (unsigned char)src[si];
        switch (c) {
            case '"':  if (di + 2 < dst_size) { dst[di++] = '\\'; dst[di++] = '"'; } break;
            case '\\': if (di + 2 < dst_size) { dst[di++] = '\\'; dst[di++] = '\\'; } break;
            case '\n': if (di + 2 < dst_size) { dst[di++] = '\\'; dst[di++] = 'n'; } break;
            case '\r': if (di + 2 < dst_size) { dst[di++] = '\\'; dst[di++] = 'r'; } break;
            case '\t': if (di + 2 < dst_size) { dst[di++] = '\\'; dst[di++] = 't'; } break;
            default:
                if (c < 0x20) {
                    if (di + 6 < dst_size)
                        di += snprintf(dst + di, dst_size - di, "\\u%04x", c);
                } else {
                    dst[di++] = c;
                }
                break;
        }
    }
    dst[di] = '\0';
}

void arws_sendpage(ClientConnection *conn, const char *html) {
    server_send_response(conn, 200, "text/html; charset=utf-8", html);
}

void arws_send_200(ClientConnection *conn, const char *body) {
    server_send_response(conn, 200, "text/plain; charset=utf-8", body);
}

void arws_send_201(ClientConnection *conn, const char *body) {
    server_send_response(conn, 201, "text/plain; charset=utf-8", body);
}

void arws_send_204(ClientConnection *conn) {
    server_add_header(conn, "Content-Length: 0");
    server_send_response(conn, 204, "text/plain", "");
}

void arws_send_301(ClientConnection *conn, const char *url) {
    char loc[1024];
    snprintf(loc, sizeof(loc), "Location: %s", url);
    server_add_header(conn, loc);
    server_send_response(conn, 301, "text/plain", "Moved Permanently");
}

void arws_send_302(ClientConnection *conn, const char *url) {
    char loc[1024];
    snprintf(loc, sizeof(loc), "Location: %s", url);
    server_add_header(conn, loc);
    server_send_response(conn, 302, "text/plain", "Found");
}

void arws_send_400(ClientConnection *conn, const char *msg) {
    char json[512], escaped[384];
    json_escape(escaped, sizeof(escaped), msg);
    snprintf(json, sizeof(json), "{\"status\":400,\"error\":\"%s\"}", escaped[0] ? escaped : "Bad Request");
    server_send_response(conn, 400, "application/json", json);
}

void arws_send_401(ClientConnection *conn, const char *msg) {
    char json[512], escaped[384];
    json_escape(escaped, sizeof(escaped), msg);
    snprintf(json, sizeof(json), "{\"status\":401,\"error\":\"%s\"}", escaped[0] ? escaped : "Unauthorized");
    server_add_header(conn, "WWW-Authenticate: Bearer");
    server_send_response(conn, 401, "application/json", json);
}

void arws_send_403(ClientConnection *conn, const char *msg) {
    char json[512], escaped[384];
    json_escape(escaped, sizeof(escaped), msg);
    snprintf(json, sizeof(json), "{\"status\":403,\"error\":\"%s\"}", escaped[0] ? escaped : "Forbidden");
    server_send_response(conn, 403, "application/json", json);
}

void arws_send_429(ClientConnection *conn, const char *msg) {
    char json[512], escaped[384];
    json_escape(escaped, sizeof(escaped), msg);
    snprintf(json, sizeof(json), "{\"status\":429,\"error\":\"%s\"}", escaped[0] ? escaped : "Too Many Requests");
    server_send_response(conn, 429, "application/json", json);
}

static void send_html_file_or_embedded(ClientConnection *conn, int status, const char *filepath, const char *embedded) {
    FILE *f = fopen(filepath, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (fsize > 0 && fsize < 65536) {
            char *buf = (char *)ar_mem_alloc(fsize + 1);
            if (buf) {
                fread(buf, 1, fsize, f);
                buf[fsize] = '\0';
                fclose(f);
                server_send_response(conn, status, "text/html; charset=utf-8", buf);
                ar_mem_free(buf);
                return;
            }
        }
        fclose(f);
    }
    server_send_response(conn, status, "text/html; charset=utf-8", embedded);
}

void arws_send_404(ClientConnection *conn) {
    send_html_file_or_embedded(conn, 404, "programfiles/arws/web/404/index.html", arws_404_html);
}

void arws_send_maintenance(ClientConnection *conn) {
    send_html_file_or_embedded(conn, 503, "programfiles/arws/web/maintenance/index.html", arws_maintenance_html);
}

void arws_send_500(ClientConnection *conn, const char *msg) {
    char json[512], escaped[384];
    json_escape(escaped, sizeof(escaped), msg);
    snprintf(json, sizeof(json), "{\"status\":500,\"error\":\"%s\"}", escaped[0] ? escaped : "Internal Server Error");
    server_send_response(conn, 500, "application/json", json);
}

void arws_send_501(ClientConnection *conn, const char *msg) {
    char json[512], escaped[384];
    json_escape(escaped, sizeof(escaped), msg);
    snprintf(json, sizeof(json), "{\"status\":501,\"error\":\"%s\"}", escaped[0] ? escaped : "Not Implemented");
    server_send_response(conn, 501, "application/json", json);
}

void arws_send_502(ClientConnection *conn, const char *msg) {
    char json[512], escaped[384];
    json_escape(escaped, sizeof(escaped), msg);
    snprintf(json, sizeof(json), "{\"status\":502,\"error\":\"%s\"}", escaped[0] ? escaped : "Bad Gateway");
    server_send_response(conn, 502, "application/json", json);
}

void arws_send_503(ClientConnection *conn, const char *msg) {
    char json[512], escaped[384];
    json_escape(escaped, sizeof(escaped), msg);
    snprintf(json, sizeof(json), "{\"status\":503,\"error\":\"%s\"}", escaped[0] ? escaped : "Service Unavailable");
    server_send_response(conn, 503, "application/json", json);
}

void arws_send_json(ClientConnection *conn, int status, const char *json) {
    server_send_response(conn, status, "application/json", json);
}

void arws_send_error(ClientConnection *conn, int status, const char *msg) {
    char json[512], escaped[384];
    json_escape(escaped, sizeof(escaped), msg);
    snprintf(json, sizeof(json),
             "{\"status\":%d,\"error\":\"%s\"}", status, escaped);
    server_send_response(conn, status, "application/json", json);
}

int arws_serve_file(ClientConnection *conn, const char *path,
                    const char *content_type) {
    return server_serve_file(conn, path, content_type);
}
