/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARAPICTRL_DB_H
#define ARAPICTRL_DB_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    char id[32];
    char name[64];
    int port;
    char type[32]; // "AUTH", "CORE", "BUSINESS", "STORAGE", "CONTROL"
    char status[16]; // "HEALTHY", "WARNING", "STOPPED"
    int restart_count;
    char last_restart[32];
} MasterService;

int arapictrl_db_init(const char *data_dir);

// Services
char* arapictrl_db_list_services_json(void);
int arapictrl_db_restart_service(const char *service_id, const char *operator_user);

// System Metrics
char* arapictrl_db_get_system_metrics_json(void);

void arapictrl_db_close(void);

#endif /* ARAPICTRL_DB_H */
