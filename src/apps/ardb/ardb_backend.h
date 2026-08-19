/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARDB_BACKEND_H
#define ARDB_BACKEND_H

#include "aros_hal.h"
#include <stdint.h>
#include <stddef.h>

#define ARDB_BACKEND_DEFAULT_HOST "127.0.0.1"
#define ARDB_BACKEND_DEFAULT_PORT 5433 /* Porta privada e isolada do Postgres nativo */
#define ARDB_BACKEND_POOL_SIZE 16

typedef struct {
    int socket_fd;
    char host[64];
    int port;
    char user[64];
    char password[128];
    char database[64];
    int is_connected;
    int in_use;
    uint64_t last_used_ms;
} ArdbBackendConn;

/* Configuração e Inicialização do Pool de Conexões do Backend */
int ardb_backend_init(const char *host, int port, const char *user, const char *password, const char *database);
void ardb_backend_cleanup(void);

/* Adquire uma conexão ativa com o PostgreSQL nativo isolado */
ArdbBackendConn* ardb_backend_acquire(void);
void ardb_backend_release(ArdbBackendConn *conn);

/* Encaminha a query sanitizada/reescrita para o PostgreSQL e transmite os pacotes de resposta ao cliente */
int ardb_backend_relay_query(ArdbBackendConn *conn, const char *sql_query, int client_fd);

#endif /* ARDB_BACKEND_H */
