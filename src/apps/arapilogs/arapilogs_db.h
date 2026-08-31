/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARAPILOGS_DB_H
#define ARAPILOGS_DB_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint64_t id;
    int64_t timestamp;
    char timestamp_str[32];
    char user[64];
    char tenant[64];
    char service[32];
    char action[64];
    char severity[16];
    int status_code;
    char ip[64];
    char details[512];
} ArapilogsEntry;

typedef struct {
    uint64_t total_events;
    uint64_t total_errors;
    uint64_t total_security_events;
    uint64_t total_today;
    int active_services_count;
} ArapilogsMetrics;

int arapilogs_db_init(const char *data_dir, int max_in_memory);
int arapilogs_db_append(const char *user, const char *tenant, const char *service,
                        const char *action, const char *severity, int status_code,
                        const char *ip, const char *details);
char* arapilogs_db_query_json(const char *tenant_filter, int is_master,
                              const char *service_filter, const char *severity_filter,
                              const char *search_query, int limit);
char* arapilogs_db_metrics_json(const char *tenant_filter, int is_master);
void arapilogs_db_close(void);

#endif /* ARAPILOGS_DB_H */
