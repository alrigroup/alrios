/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "ardb_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static ArdbConfig g_config;
static char g_last_cfg_path[256] = "ardb.cfg";

static void trim(char *s) {
    if (!s) return;
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}

static void set_defaults(ArdbConfig *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->server_port = 5432;
    strncpy(cfg->server_bind, "0.0.0.0", sizeof(cfg->server_bind) - 1);
    cfg->max_connections = 1024;

    strncpy(cfg->backend_host, "127.0.0.1", sizeof(cfg->backend_host) - 1);
    cfg->backend_port = 5433;
    strncpy(cfg->backend_user, "postgres", sizeof(cfg->backend_user) - 1);
    strncpy(cfg->backend_password, "postgres", sizeof(cfg->backend_password) - 1);
    strncpy(cfg->backend_database, "postgres", sizeof(cfg->backend_database) - 1);

    cfg->http_enabled = 1;
    strncpy(cfg->http_bind, "127.0.0.1", sizeof(cfg->http_bind) - 1);
    cfg->http_port = 5435;
    strncpy(cfg->http_route_prefix, "/api/v1/db", sizeof(cfg->http_route_prefix) - 1);
    cfg->http_auth_required = 1;
    cfg->http_max_payload_bytes = 1048576;

    strncpy(cfg->sql_firewall, "enforce", sizeof(cfg->sql_firewall) - 1);
    strncpy(cfg->ddl_shield, "enforce", sizeof(cfg->ddl_shield) - 1);
    strncpy(cfg->audit_log, "storage/ardb/audit.log", sizeof(cfg->audit_log) - 1);
    strncpy(cfg->crypto_hash_chain, "sha256", sizeof(cfg->crypto_hash_chain) - 1);
}

int ardb_config_load(const char *cfg_path, ArdbConfig *out_cfg) {
    if (!out_cfg) out_cfg = &g_config;
    set_defaults(out_cfg);

    const char *paths_to_try[] = {
        cfg_path,
        "ardb.cfg",
        "storage/ardb/ardb.cfg",
        "arcore/storage/ardb/ardb.cfg",
        "../storage/ardb/ardb.cfg",
        "../../storage/ardb/ardb.cfg",
        "arcore/programfiles/ardb/ardb.cfg",
        "/mnt/HD/ALRIGROUP/local/alrios/arcore/storage/ardb/ardb.cfg",
        NULL
    };

    FILE *f = NULL;
    for (int i = 0; paths_to_try[i]; i++) {
        if (!paths_to_try[i] || !paths_to_try[i][0]) continue;
        f = fopen(paths_to_try[i], "r");
        if (f) {
            strncpy(g_last_cfg_path, paths_to_try[i], sizeof(g_last_cfg_path) - 1);
            break;
        }
    }

    if (!f) {
        /* Usa defaults */
        return 0;
    }

    char line[512];
    char section[64] = "";

    while (fgets(line, sizeof(line), f)) {
        trim(line);
        if (line[0] == '#' || line[0] == ';' || line[0] == '\0') continue;

        if (line[0] == '[' && line[strlen(line) - 1] == ']') {
            strncpy(section, line + 1, sizeof(section) - 1);
            section[strlen(section) - 1] = '\0';
            trim(section);
            continue;
        }

        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        trim(key);
        trim(val);

        if (strcmp(section, "server") == 0) {
            if (strcmp(key, "port") == 0) out_cfg->server_port = atoi(val);
            else if (strcmp(key, "bind") == 0) strncpy(out_cfg->server_bind, val, sizeof(out_cfg->server_bind) - 1);
            else if (strcmp(key, "max_connections") == 0) out_cfg->max_connections = atoi(val);
        } else if (strcmp(section, "backend") == 0) {
            if (strcmp(key, "host") == 0) strncpy(out_cfg->backend_host, val, sizeof(out_cfg->backend_host) - 1);
            else if (strcmp(key, "port") == 0) out_cfg->backend_port = atoi(val);
            else if (strcmp(key, "user") == 0) strncpy(out_cfg->backend_user, val, sizeof(out_cfg->backend_user) - 1);
            else if (strcmp(key, "password") == 0) strncpy(out_cfg->backend_password, val, sizeof(out_cfg->backend_password) - 1);
            else if (strcmp(key, "database") == 0) strncpy(out_cfg->backend_database, val, sizeof(out_cfg->backend_database) - 1);
        } else if (strcmp(section, "http_api") == 0) {
            if (strcmp(key, "enabled") == 0) {
                out_cfg->http_enabled = (strcmp(val, "yes") == 0 || strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
            } else if (strcmp(key, "bind") == 0) {
                strncpy(out_cfg->http_bind, val, sizeof(out_cfg->http_bind) - 1);
            } else if (strcmp(key, "port") == 0) {
                out_cfg->http_port = atoi(val);
            } else if (strcmp(key, "route_prefix") == 0) {
                strncpy(out_cfg->http_route_prefix, val, sizeof(out_cfg->http_route_prefix) - 1);
            } else if (strcmp(key, "auth_required") == 0) {
                out_cfg->http_auth_required = (strcmp(val, "yes") == 0 || strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
            } else if (strcmp(key, "max_payload_bytes") == 0) {
                out_cfg->http_max_payload_bytes = (uint32_t)atoi(val);
            }
        } else if (strcmp(section, "security") == 0) {
            if (strcmp(key, "sql_firewall") == 0) strncpy(out_cfg->sql_firewall, val, sizeof(out_cfg->sql_firewall) - 1);
            else if (strcmp(key, "ddl_shield") == 0) strncpy(out_cfg->ddl_shield, val, sizeof(out_cfg->ddl_shield) - 1);
            else if (strcmp(key, "audit_log") == 0) strncpy(out_cfg->audit_log, val, sizeof(out_cfg->audit_log) - 1);
            else if (strcmp(key, "crypto_hash_chain") == 0) strncpy(out_cfg->crypto_hash_chain, val, sizeof(out_cfg->crypto_hash_chain) - 1);
        }
    }

    fclose(f);
    return 0;
}

ArdbConfig* ardb_config_get(void) {
    return &g_config;
}

int ardb_config_reload(const char *cfg_path) {
    ArdbConfig tmp;
    if (ardb_config_load(cfg_path, &tmp) == 0) {
        memcpy(&g_config, &tmp, sizeof(g_config));
        return 0;
    }
    return -1;
}
