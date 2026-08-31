/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARDB_PGWIRE_H
#define ARDB_PGWIRE_H

#include "aros_hal.h"
#include <stdint.h>
#include <stddef.h>

#define ARDB_DEFAULT_PORT 5432
#define ARDB_PGWIRE_MAX_BUF 65536
#define ARDB_MAX_SESSION_TOKEN_LEN 128
#define ARDB_MAX_USER_LEN 64
#define ARDB_MAX_DATABASE_LEN 64
#define ARDB_MAX_TENANT_LEN 64

/* Protocolo PG-Wire Códigos de Mensagem */
#define PG_MSG_STARTUP_V3 0x00030000
#define PG_MSG_SSL_REQUEST 80877103
#define PG_MSG_CANCEL_REQUEST 80877102

/* Tipos de Mensagens do Backend (Servidor -> Cliente) */
#define PG_TYPE_AUTH_REQ 'R'
#define PG_TYPE_KEY_DATA 'K'
#define PG_TYPE_PARAM_STATUS 'S'
#define PG_TYPE_READY_FOR_QUERY 'Z'
#define PG_TYPE_ROW_DESC 'T'
#define PG_TYPE_DATA_ROW 'D'
#define PG_TYPE_CMD_COMPLETE 'C'
#define PG_TYPE_ERROR_RESP 'E'
#define PG_TYPE_NOTICE_RESP 'N'

/* Tipos de Mensagens do Frontend (Cliente -> Servidor) */
#define PG_TYPE_PASSWORD 'p'
#define PG_TYPE_QUERY 'Q'
#define PG_TYPE_PARSE 'P'
#define PG_TYPE_BIND 'B'
#define PG_TYPE_EXECUTE 'E'
#define PG_TYPE_SYNC 'S'
#define PG_TYPE_TERMINATE 'X'

/* Status da Transação */
#define PG_TX_IDLE 'I'
#define PG_TX_IN_BLOCK 'T'
#define PG_TX_ERROR 'E'

typedef struct {
    int client_fd;
    char client_ip[64];
    char user[ARDB_MAX_USER_LEN];
    char database[ARDB_MAX_DATABASE_LEN];
    char tenant_id[ARDB_MAX_TENANT_LEN];
    char auth_token[ARDB_MAX_SESSION_TOKEN_LEN];
    int is_authenticated;
    int is_ssl;
    uint32_t process_id;
    uint32_t secret_key;
    char tx_status;
    uint64_t session_start_ms;
    char prepared_query[2048];
} ArdbClientSession;

/* Inicialização do Gateway PG-Wire */
int ardb_pgwire_server_start(int port);
void ardb_pgwire_server_stop(void);

/* Funções de Construção de Pacotes PG-Wire */
int ardb_pgwire_send_auth_ok(int fd);
int ardb_pgwire_send_auth_cleartext_req(int fd);
int ardb_pgwire_send_auth_md5_req(int fd, const char salt[4]);
int ardb_pgwire_send_param_status(int fd, const char *param, const char *value);
int ardb_pgwire_send_backend_key_data(int fd, uint32_t pid, uint32_t key);
int ardb_pgwire_send_ready_for_query(int fd, char tx_status);
int ardb_pgwire_send_error(int fd, const char *severity, const char *code, const char *message);
int ardb_pgwire_send_command_complete(int fd, const char *tag);

#endif /* ARDB_PGWIRE_H */
