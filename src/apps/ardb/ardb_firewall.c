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

/* Converte string para minúsculo para análise de palavras-chave */
static void to_lower_str(const char *src, char *dst, size_t dst_size) {
    size_t i = 0;
    while (src[i] && i < dst_size - 1) {
        dst[i] = (char)tolower((unsigned char)src[i]);
        i++;
    }
    dst[i] = '\0';
}

ArdbFwAction ardb_firewall_inspect(const char *raw_sql, const char *tenant_id, const char *role,
                                   char *out_rewritten_sql, size_t out_size,
                                   char *out_reason, size_t out_reason_size) {
    if (!raw_sql || raw_sql[0] == '\0') {
        if (out_reason) snprintf(out_reason, out_reason_size, "Empty query");
        return ARDB_FW_OK;
    }

    char lower[2048];
    to_lower_str(raw_sql, lower, sizeof(lower));

    /* 1. TEST-4.2: Bloqueio de Comandos Destrutivos por apps e operadores comuns */
    int is_admin = (role && strcmp(role, "admin") == 0);
    if (!is_admin) {
        if (strstr(lower, "drop table") || strstr(lower, "drop database") ||
            strstr(lower, "alter system") || strstr(lower, "truncate ") ||
            strstr(lower, "drop schema") || strstr(lower, "grant all")) {
            
            if (out_reason) {
                snprintf(out_reason, out_reason_size,
                         "ERROR: 42501: Permissao negada pelo ALRI Firewall (Comando DDL/DML destrutivo)");
            }
            return ARDB_FW_BLOCK_DESTRUCTIVE;
        }
    }

    /* 2. TEST-4.1: Detecção de tentativa de Bypass de RLS com comentários e injeção */
    if (strstr(lower, "where tenant_id") || strstr(lower, "/*") || strstr(lower, "--")) {
        /* Se tentar forjar diretamente um tenant_id diferente do seu */
        char forged_tenant_check[128];
        if (tenant_id && tenant_id[0]) {
            snprintf(forged_tenant_check, sizeof(forged_tenant_check), "tenant_id = '%s'", tenant_id);
            if (!strstr(lower, forged_tenant_check) && strstr(lower, "tenant_id =")) {
                if (out_reason) {
                    snprintf(out_reason, out_reason_size,
                             "CRITICAL: RLS Bypass Attempt detectado (Tentativa de acesso a outro tenant)");
                }
                return ARDB_FW_BLOCK_RLS_BYPASS;
            }
        }
    }

    /* 3. Injeção dinâmica e segura do Tenant ID do ALRIOS via SET LOCAL */
    if (tenant_id && tenant_id[0]) {
        snprintf(out_rewritten_sql, out_size,
                 "SET LOCAL alri.tenant_id = '%s'; %s", tenant_id, raw_sql);
    } else {
        strncpy(out_rewritten_sql, raw_sql, out_size - 1);
        out_rewritten_sql[out_size - 1] = '\0';
    }

    return ARDB_FW_OK;
}
