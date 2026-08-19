/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARDB_FIREWALL_H
#define ARDB_FIREWALL_H

#include <stddef.h>

typedef enum {
    ARDB_FW_OK = 0,
    ARDB_FW_BLOCK_DESTRUCTIVE = 1,  /* DROP, ALTER SYSTEM, TRUNCATE não autorizados */
    ARDB_FW_BLOCK_RLS_BYPASS = 2,   /* Tentativa de burlar tenant_id com comentários ou injection */
    ARDB_FW_BLOCK_EXFILTRATION = 3  /* Excedeu o limite de volume de dados do perfil */
} ArdbFwAction;

/* Inspeciona e sanitiza a query SQL antes de enviar para o PostgreSQL */
ArdbFwAction ardb_firewall_inspect(const char *raw_sql, const char *tenant_id, const char *role,
                                   char *out_rewritten_sql, size_t out_size,
                                   char *out_reason, size_t out_reason_size);

#endif /* ARDB_FIREWALL_H */
