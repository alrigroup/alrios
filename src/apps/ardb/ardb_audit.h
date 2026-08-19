/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARDB_AUDIT_H
#define ARDB_AUDIT_H

#include <stdint.h>
#include <stddef.h>

void ardb_audit_init(const char *log_path);
void ardb_audit_cleanup(void);

/* Grava log forense imutável com Hash em Cadeia (TEST-5.1 e TEST-5.2) */
void ardb_audit_log_query(const char *user, const char *tenant_id, const char *client_ip,
                          const char *sql_query, int status_code, uint64_t duration_us);

/* Verifica a integridade da cadeia de hashes do arquivo de log */
int ardb_audit_verify_integrity(const char *log_path, char *out_error, size_t out_error_size);

#endif /* ARDB_AUDIT_H */
