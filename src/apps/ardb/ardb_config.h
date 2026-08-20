/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARDB_CONFIG_H
#define ARDB_CONFIG_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    /* [server] */
    int server_port;
    char server_bind[64];
    int max_connections;

    /* [backend] */
    char backend_host[128];
    int backend_port;
    char backend_user[64];
    char backend_password[64];
    char backend_database[64];

    /* [http_api] */
    int http_enabled;
    char http_bind[64];
    int http_port;
    char http_route_prefix[128];
    int http_auth_required;
    uint32_t http_max_payload_bytes;

    /* [security] */
    char sql_firewall[32];
    char ddl_shield[32];
    char audit_log[256];
    char crypto_hash_chain[32];
} ArdbConfig;

int ardb_config_load(const char *cfg_path, ArdbConfig *out_cfg);
ArdbConfig* ardb_config_get(void);
int ardb_config_reload(const char *cfg_path);

#endif /* ARDB_CONFIG_H */
