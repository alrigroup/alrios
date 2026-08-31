/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "ardb_firewall.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "ardb_auth.h"

/* Convert string to lowercase for keyword analysis */
static void to_lower_str(const char *src, char *dst, size_t dst_size) {
    size_t i = 0;
    while (src[i] && i < dst_size - 1) {
        dst[i] = (char)tolower((unsigned char)src[i]);
        i++;
    }
    dst[i] = '\0';
}

static int is_valid_tenant_id(const char *t) {
    if (!t || !t[0]) return 0;
    size_t len = strlen(t);
    if (len > 64) return 0;
    for (size_t i = 0; i < len; i++) {
        char c = t[i];
        if (!isalnum((unsigned char)c) && c != '_' && c != '-') {
            return 0;
        }
    }
    return 1;
}

ArdbFwAction ardb_firewall_inspect(const char *raw_sql, const char *tenant_id, const char *role,
                                   char *out_rewritten_sql, size_t out_size,
                                   char *out_reason, size_t out_reason_size) {
    if (!raw_sql || raw_sql[0] == '\0') {
        if (out_reason) snprintf(out_reason, out_reason_size, "Empty query");
        return ARDB_FW_OK;
    }

    /* 0. Strict validation of tenant_id against SQL injection / Metacharacter injection */
    if (tenant_id && tenant_id[0]) {
        if (!is_valid_tenant_id(tenant_id)) {
            if (out_reason) {
                snprintf(out_reason, out_reason_size,
                         "CRITICAL: 42601: Invalid or malicious characters in tenant identifier");
            }
            return ARDB_FW_BLOCK_RLS_BYPASS;
        }
    }

    char lower[2048];
    to_lower_str(raw_sql, lower, sizeof(lower));

    /* 1. Block destructive DDL/DML commands for non-admin roles */
    int is_admin = (role && strcmp(role, "admin") == 0);
    if (!is_admin) {
        if (strstr(lower, "drop table") || strstr(lower, "drop database") ||
            strstr(lower, "alter system") || strstr(lower, "truncate ") ||
            strstr(lower, "drop schema") || strstr(lower, "grant all") ||
            strstr(lower, "shutdown") || strstr(lower, "copy to program")) {
            
            if (out_reason) {
                snprintf(out_reason, out_reason_size,
                         "ERROR: 42501: Permission denied by ALRI Firewall (Destructive DDL/DML command)");
            }
            return ARDB_FW_BLOCK_DESTRUCTIVE;
        }

        /* 2. Table-Level Access Control & App Groups validation */
        const char *table_keywords[] = {"from ", "join ", "into ", "update ", "table "};
        for (size_t k = 0; k < sizeof(table_keywords) / sizeof(table_keywords[0]); k++) {
            char *p = strstr(lower, table_keywords[k]);
            if (p) {
                p += strlen(table_keywords[k]);
                while (*p == ' ' || *p == '\t') p++;
                char tbl[64] = {0};
                size_t ti = 0;
                while (*p && (isalnum((unsigned char)*p) || *p == '_' || *p == '.') && ti < sizeof(tbl) - 1) {
                    tbl[ti++] = *p++;
                }
                tbl[ti] = '\0';
                if (ti > 0 && strncmp(tbl, "pg_", 3) != 0 &&
                    strncmp(tbl, "information_schema", 18) != 0 &&
                    strncmp(tbl, "pg_catalog", 10) != 0) {
                    if (role && strcmp(role, "admin") != 0 && !ardb_auth_is_table_allowed(tenant_id, role, tbl)) {
                        if (out_reason) {
                            snprintf(out_reason, out_reason_size,
                                     "ERROR: 42501: Permission denied for table '%s' by ALRI Firewall", tbl);
                        }
                        return ARDB_FW_BLOCK_DESTRUCTIVE;
                    }
                }
            }
        }
    }

    /* 3. Detect RLS bypass attempts using comments and forged tenant clauses */
    if (strstr(lower, "where tenant_id") || strstr(lower, "/*") || strstr(lower, "--")) {
        char forged_tenant_check[128];
        if (tenant_id && tenant_id[0]) {
            snprintf(forged_tenant_check, sizeof(forged_tenant_check), "tenant_id = '%s'", tenant_id);
            if (!strstr(lower, forged_tenant_check) && strstr(lower, "tenant_id =")) {
                if (out_reason) {
                    snprintf(out_reason, out_reason_size,
                             "CRITICAL: RLS Bypass Attempt detected (Unauthorized cross-tenant access)");
                }
                return ARDB_FW_BLOCK_RLS_BYPASS;
            }
        }
    }

    /* 4. Pass clean validated SQL query */
    strncpy(out_rewritten_sql, raw_sql, out_size - 1);
    out_rewritten_sql[out_size - 1] = '\0';

    return ARDB_FW_OK;
}
