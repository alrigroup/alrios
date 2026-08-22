/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARDB_AUTH_H
#define ARDB_AUTH_H

#include <stdint.h>
#include <stddef.h>

#define ARDB_MAX_USERS 64
#define ARDB_MAX_ACTIVE_TOKENS 128
#define ARDB_MAX_GROUPS 32
#define ARDB_MAX_GROUP_TABLES 64
#define ARDB_MAX_GROUP_APPS 64
#define ARDB_TOKEN_DEFAULT_TTL_SEC 14400 /* 4 horas */

typedef struct {
    char name[64];
    char tables[ARDB_MAX_GROUP_TABLES][64];
    int  table_count;
    char apps[ARDB_MAX_GROUP_APPS][64];
    int  app_count;
} ArdbAppGroup;

typedef struct {
    char username[64];
    char password_hash[128];
    char tenant_id[64];
    char role[32];               /* "admin", "operator", "app" */
    char allowed_tables[32][64];
    int  allowed_table_count;
    char app_groups[8][64];
    int  app_group_count;
    int  is_active;
    int  requires_2fa;
    char totp_secret[64];
} ArdbUser;

typedef struct {
    char token[128];
    char username[64];
    char tenant_id[64];
    char role[32];
    uint64_t created_at_ms;
    uint64_t expires_at_ms;
    int is_revoked;
} ArdbSessionToken;

void ardb_auth_init(void);
void ardb_auth_cleanup(void);

/* Gestão de Usuários e Apps */
int ardb_auth_add_user(const char *username, const char *password, const char *tenant_id, const char *role);
int ardb_auth_add_app(const char *app_name, const char *token_or_secret, const char *group_name, const char *tables_csv);
int ardb_auth_user_exists(const char *username);

/* Gestão de App Groups Compartilhados */
int ardb_auth_create_group(const char *group_name, const char *tables_csv);
int ardb_auth_add_app_to_group(const char *group_name, const char *app_name);

/* Verificação de Permissão de Tabelas */
int ardb_auth_is_table_allowed(const char *app_name, const char *role, const char *table_name);

/* Validação de Senha / Token Efêmero */
int ardb_auth_verify_token(const char *token, char *out_user, char *out_tenant, char *out_role);
int ardb_auth_generate_token(const char *username, const char *password, const char *totp_code,
                             int ttl_seconds, char *out_token, size_t out_token_size);
int ardb_auth_revoke_token(const char *token);

#endif /* ARDB_AUTH_H */
