/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARWN_SERVER_H
#define ARWN_SERVER_H

#include <stddef.h>
#include <stdint.h>

#include "arwn.h"

/* Servidor HTTP event-loop (Fase 2), modelo nginx:
 * - serve só da tabela em memória (nada de disco por request);
 * - rota por nome exato validado no build (sem path-traversal);
 * - keep-alive, timeouts anti-slowloris, headers de segurança;
 * - Content-Type: application/wasm nos módulos wasm.
 */

#define ARWN_SERVER_MAX_ROUTES  128
#define ARWN_SERVER_MAX_CONN    1024
#define ARWN_SERVER_LISTEN_BACKLOG 4096
#define ARWN_SERVER_HEADER_TIMEOUT_MS 10000
#define ARWN_SERVER_BODY_TIMEOUT_MS   10000
#define ARWN_SERVER_KEEPALIVE_MS      5000
#define ARWN_SERVER_RECV_BUF   ARWN_HTTP_MAX_REQUEST

/* Cache policy por rota. */
typedef enum {
    ARWN_CACHE_NO_STORE,      /* 200 de erro/aviso */
    ARWN_CACHE_NO_CACHE,      /* entrada .arhtml: no-cache + ETag */
    ARWN_CACHE_IMMUTABLE      /* estáticos com hash: immutable 1y */
} arwn_cache_t;

typedef struct {
    const char *path;         /* rota exata (ex.: "/", "/mod/main.wasm") */
    const uint8_t *data;
    size_t        size;
    const char   *content_type;
    arwn_cache_t  cache;
} arwn_route_t;

typedef struct arwn_server arwn_server_t;

/* Cria e prepara o servidor (não abre socket ainda). */
arwn_server_t *arwn_server_create(void);

/* Adiciona uma rota estática à tabela em memória. `path` deve ser uma
   string estável (o servidor não copia). */
int arwn_server_add_route(arwn_server_t *s, const char *path,
                          const uint8_t *data, size_t size,
                          const char *content_type, arwn_cache_t cache);

/* Carrega um .arweb (validado) e expõe suas seções como rotas:
 *   - <entry> e "/"  -> seção app.html (no-cache)
 *   - /mod/main.wasm -> seção mod/main.wasm (immutable, application/wasm)
 *   - /bundle.js     -> seção bundle.js (immutable, application/javascript)
 * Retorna 0 ou -1. */
int arwn_server_load_arweb(arwn_server_t *s, const arwn_unit_t *unit,
                           const uint8_t *arweb, size_t arweb_len);

/* Abre o socket, bind/listen e roda o event loop no thread atual.
   Retorna 0 em sucesso (loop sai em arwn_server_stop) ou <0 no bind. */
int arwn_server_run(arwn_server_t *s, const char *bind, uint16_t port);

/* Pede ao event loop para parar (chamado de outro thread). */
void arwn_server_stop(arwn_server_t *s);

void arwn_server_free(arwn_server_t *s);

/* Rota do servidor (para logs/query). */
int arwn_server_route_count(const arwn_server_t *s);
const arwn_route_t *arwn_server_route(const arwn_server_t *s, int idx);

#endif /* ARWN_SERVER_H */