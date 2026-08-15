/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARWS_GATEWAY_H
#define ARWS_GATEWAY_H

#include "server.h"

#define ARWS_MAX_BACKENDS 64
#define ARWS_MAX_ROUTES  512

typedef struct {
    int id;
    int fd;
    int pid;
    char name[64];
    int registered;
} ArwsBackend;

typedef struct {
    char prefix[256];
    char method[16];
    char host[256];
    char mode[16];
    int backend_id;
    int use_handler;
    int use_stream;
    int use_redirect;
    char proxy_target[256];
    char redirect_target[256];
    RequestHandler handler;
} ArwsRoute;

int arws_init(void);
int arws_start(int port, int mode);
void arws_stop(void);

int arws_add_route(const char *prefix, const char *method,
                   const char *host, const char *mode,
                   int backend_id);
int arws_add_proxy_route(const char *prefix, const char *method,
                         const char *host, const char *mode,
                         const char *target_url);
int arws_add_stream_route(const char *prefix, const char *method,
                          const char *host, const char *mode,
                          const char *target_url);
int arws_add_redirect_route(const char *prefix, const char *method,
                            const char *host, const char *mode,
                            const char *target_url);
int arws_add_handler(const char *prefix, const char *method,
                     const char *host, const char *mode,
                     RequestHandler handler);
int arws_remove_route(const char *prefix, const char *method,
                      const char *host);

int arws_register_backend(const char *name, int fd);
int arws_unregister_backend(int backend_id);
int arws_backend_count(void);
void arws_close_all_backends(void);
ArwsBackend* arws_get_backend(int backend_id);
int arws_get_backend_fd(int backend_id);
int arws_find_backend_by_name(const char *name);
int arws_find_backend_id_by_fd(int fd);
int arws_remove_routes_by_backend(int backend_id);
int arws_remove_route(const char *prefix, const char *method, const char *host);
int arws_backend_send_request(int backend_id, const unsigned char *raw_req, int raw_len);
int arws_backend_read_response(int backend_id, unsigned char *buf, int maxlen);

int arws_route_match(ClientConnection *conn, HttpRequest *req,
                     const char *effective_mode, ArwsRoute *out_route);

int arws_stream_proxy_forward(ClientConnection *conn, HttpRequest *req, const char *target_url);
int arws_dispatch(ClientConnection *conn, HttpRequest *req, const char *effective_mode);

void arws_dispatch_lock(int backend_id);
void arws_dispatch_unlock(int backend_id);

#endif
