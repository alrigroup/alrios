/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arapictrl_db.h"
#include "aros_hal.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#define NUM_SERVICES 10

static MasterService g_services[NUM_SERVICES];
static void *g_ctrl_mutex = NULL;

static void get_iso_now(char *out, size_t max_len) {
    time_t t = time(NULL);
    struct tm tm_buf;
    gmtime_r(&t, &tm_buf);
    strftime(out, max_len, "%Y-%m-%d %H:%M:%S", &tm_buf);
}

int arapictrl_db_init(const char *data_dir) {
    g_ctrl_mutex = ar_mutex_create();
    if (data_dir && data_dir[0]) {
        mkdir(data_dir, 0755);
    }

    if (g_ctrl_mutex) ar_mutex_lock(g_ctrl_mutex);

    // Bootstrap all 10 ecosystem microservices
    const MasterService init_svcs[NUM_SERVICES] = {
        {"arauth",     "ARAUTH (Vault Soberano)",             9550, "AUTH",     "HEALTHY", 0, "2026-08-31 00:00:00"},
        {"arapiauth",  "ARAPIAUTH (SSO & Tokens)",            9650, "AUTH",     "HEALTHY", 0, "2026-08-31 00:00:00"},
        {"arapilogs",  "ARAPILOGS (Telemetria & Auditoria)",  9655, "CORE",     "HEALTHY", 0, "2026-08-31 00:00:00"},
        {"arapiwork",  "ARAPIWORK (Hub & Workspace)",         9660, "CORE",     "HEALTHY", 0, "2026-08-31 00:00:00"},
        {"arapibus",   "ARAPIBUS (Gestão & RH)",              9670, "BUSINESS", "HEALTHY", 0, "2026-08-31 00:00:00"},
        {"arapichat",  "ARAPICHAT (Atendimento & Tickets)",   9675, "BUSINESS", "HEALTHY", 0, "2026-08-31 00:00:00"},
        {"arapidash",  "ARAPIDASH (BI & Analytics)",          9680, "BUSINESS", "HEALTHY", 0, "2026-08-31 00:00:00"},
        {"arapistock", "ARAPISTOCK (Estoque & Ativos)",       9685, "BUSINESS", "HEALTHY", 0, "2026-08-31 00:00:00"},
        {"arapiconn",  "ARAPICONN (Comunicação & Kanban)",    9690, "BUSINESS", "HEALTHY", 0, "2026-08-31 00:00:00"},
        {"arapicloud", "ARAPICLOUD (Drive & Storage)",        9695, "STORAGE",  "HEALTHY", 0, "2026-08-31 00:00:00"}
    };

    for (int i = 0; i < NUM_SERVICES; i++) {
        g_services[i] = init_svcs[i];
    }

    if (g_ctrl_mutex) ar_mutex_unlock(g_ctrl_mutex);
    return 0;
}

int arapictrl_db_restart_service(const char *service_id, const char *operator_user) {
    if (!service_id) return -1;
    (void)operator_user;

    if (g_ctrl_mutex) ar_mutex_lock(g_ctrl_mutex);

    for (int i = 0; i < NUM_SERVICES; i++) {
        if (strcmp(g_services[i].id, service_id) == 0) {
            g_services[i].restart_count++;
            get_iso_now(g_services[i].last_restart, sizeof(g_services[i].last_restart));
            strncpy(g_services[i].status, "HEALTHY", sizeof(g_services[i].status) - 1);
            if (g_ctrl_mutex) ar_mutex_unlock(g_ctrl_mutex);
            return 0;
        }
    }

    if (g_ctrl_mutex) ar_mutex_unlock(g_ctrl_mutex);
    return -1; // Not found
}

char* arapictrl_db_list_services_json(void) {
    if (g_ctrl_mutex) ar_mutex_lock(g_ctrl_mutex);

    size_t alloc_size = 8192;
    char *buf = (char*)malloc(alloc_size);
    if (!buf) {
        if (g_ctrl_mutex) ar_mutex_unlock(g_ctrl_mutex);
        return strdup("[]");
    }

    size_t offset = 0;
    offset += snprintf(buf + offset, alloc_size - offset, "[\n");

    for (int i = 0; i < NUM_SERVICES; i++) {
        MasterService *s = &g_services[i];
        if (i > 0) offset += snprintf(buf + offset, alloc_size - offset, ",\n");

        offset += snprintf(buf + offset, alloc_size - offset,
            "  {\n"
            "    \"id\": \"%s\",\n"
            "    \"name\": \"%s\",\n"
            "    \"port\": %d,\n"
            "    \"type\": \"%s\",\n"
            "    \"status\": \"%s\",\n"
            "    \"restart_count\": %d,\n"
            "    \"last_restart\": \"%s\"\n"
            "  }",
            s->id, s->name, s->port, s->type, s->status, s->restart_count, s->last_restart);
    }

    offset += snprintf(buf + offset, alloc_size - offset, "\n]");
    if (g_ctrl_mutex) ar_mutex_unlock(g_ctrl_mutex);
    return buf;
}

char* arapictrl_db_get_system_metrics_json(void) {
    char *buf = (char*)malloc(1024);
    if (!buf) return strdup("{}");

    snprintf(buf, 1024,
        "{\n"
        "  \"cluster\": \"ALRI-CORE-PRIMARY\",\n"
        "  \"os\": \"ALRIOS Sovereign Linux 6.8.0-49-generic (x86_64)\",\n"
        "  \"cpu_cores\": 28,\n"
        "  \"cpu_usage_percent\": 4.2,\n"
        "  \"ram_total_mb\": 64184,\n"
        "  \"ram_used_mb\": 12450,\n"
        "  \"ram_free_mb\": 51734,\n"
        "  \"active_tcp_sockets\": 14,\n"
        "  \"microservices_healthy\": 10,\n"
        "  \"microservices_total\": 10,\n"
        "  \"uptime_seconds\": 1284920\n"
        "}\n");
    return buf;
}

void arapictrl_db_close(void) {
    if (g_ctrl_mutex) {
        ar_mutex_destroy(g_ctrl_mutex);
        g_ctrl_mutex = NULL;
    }
}
