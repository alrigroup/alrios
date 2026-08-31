/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arapidash_db.h"
#include "aros_hal.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void *g_dash_mutex = NULL;

int arapidash_db_init(const char *data_dir) {
    g_dash_mutex = ar_mutex_create();
    if (data_dir && data_dir[0]) {
        mkdir(data_dir, 0755);
    }
    return 0;
}

char* arapidash_db_get_overview_json(const char *caller_company, int is_master) {
    if (g_dash_mutex) ar_mutex_lock(g_dash_mutex);

    char *buf = (char*)malloc(2048);
    if (!buf) {
        if (g_dash_mutex) ar_mutex_unlock(g_dash_mutex);
        return strdup("{}");
    }

    if (is_master || !caller_company || !caller_company[0] || strcmp(caller_company, "alrigroup") == 0) {
        // Holding Master aggregated numbers
        snprintf(buf, 2048,
            "{\n"
            "  \"scope\": \"global_holding\",\n"
            "  \"currency\": \"BRL\",\n"
            "  \"mrr\": 148500.00,\n"
            "  \"arr\": 1782000.00,\n"
            "  \"revenue_growth\": 18.4,\n"
            "  \"active_employees\": 48,\n"
            "  \"total_companies\": 3,\n"
            "  \"system_uptime\": 99.98,\n"
            "  \"resolved_tickets_rate\": 97.2,\n"
            "  \"security_score\": \"A+\"\n"
            "}\n");
    } else {
        // Scoped Subsidiary numbers (e.g. Detroit GG)
        snprintf(buf, 2048,
            "{\n"
            "  \"scope\": \"%s\",\n"
            "  \"currency\": \"BRL\",\n"
            "  \"mrr\": 42300.00,\n"
            "  \"arr\": 507600.00,\n"
            "  \"revenue_growth\": 12.1,\n"
            "  \"active_employees\": 14,\n"
            "  \"total_companies\": 1,\n"
            "  \"system_uptime\": 99.99,\n"
            "  \"resolved_tickets_rate\": 96.5,\n"
            "  \"security_score\": \"A\"\n"
            "}\n",
            caller_company);
    }

    if (g_dash_mutex) ar_mutex_unlock(g_dash_mutex);
    return buf;
}

char* arapidash_db_get_charts_json(const char *caller_company, int is_master) {
    if (g_dash_mutex) ar_mutex_lock(g_dash_mutex);

    char *buf = (char*)malloc(4096);
    if (!buf) {
        if (g_dash_mutex) ar_mutex_unlock(g_dash_mutex);
        return strdup("[]");
    }

    if (is_master || !caller_company || strcmp(caller_company, "alrigroup") == 0) {
        snprintf(buf, 4096,
            "[\n"
            "  {\"month\": \"Mar\", \"revenue\": 108000, \"expenses\": 32000, \"profit\": 76000},\n"
            "  {\"month\": \"Abr\", \"revenue\": 115000, \"expenses\": 34000, \"profit\": 81000},\n"
            "  {\"month\": \"Mai\", \"revenue\": 124000, \"expenses\": 33500, \"profit\": 90500},\n"
            "  {\"month\": \"Jun\", \"revenue\": 131000, \"expenses\": 36000, \"profit\": 95000},\n"
            "  {\"month\": \"Jul\", \"revenue\": 142000, \"expenses\": 38000, \"profit\": 104000},\n"
            "  {\"month\": \"Ago\", \"revenue\": 148500, \"expenses\": 39200, \"profit\": 109300}\n"
            "]\n");
    } else {
        snprintf(buf, 4096,
            "[\n"
            "  {\"month\": \"Mar\", \"revenue\": 28000, \"expenses\": 11000, \"profit\": 17000},\n"
            "  {\"month\": \"Abr\", \"revenue\": 31000, \"expenses\": 12000, \"profit\": 19000},\n"
            "  {\"month\": \"Mai\", \"revenue\": 34000, \"expenses\": 11500, \"profit\": 22500},\n"
            "  {\"month\": \"Jun\", \"revenue\": 36500, \"expenses\": 13000, \"profit\": 23500},\n"
            "  {\"month\": \"Jul\", \"revenue\": 39000, \"expenses\": 13800, \"profit\": 25200},\n"
            "  {\"month\": \"Ago\", \"revenue\": 42300, \"expenses\": 14200, \"profit\": 28100}\n"
            "]\n");
    }

    if (g_dash_mutex) ar_mutex_unlock(g_dash_mutex);
    return buf;
}

char* arapidash_db_get_breakdown_json(const char *caller_company, int is_master) {
    if (g_dash_mutex) ar_mutex_lock(g_dash_mutex);

    char *buf = (char*)malloc(2048);
    if (!buf) {
        if (g_dash_mutex) ar_mutex_unlock(g_dash_mutex);
        return strdup("[]");
    }

    if (is_master || !caller_company || !caller_company[0] || strcmp(caller_company, "alrigroup") == 0) {
        snprintf(buf, 2048,
            "[\n"
            "  {\"id\": \"alrigroup\", \"name\": \"ALRIGROUP Holding\", \"percentage\": 58.5, \"mrr\": 86872.50},\n"
            "  {\"id\": \"detroitgg\", \"name\": \"Detroit GG\", \"percentage\": 28.5, \"mrr\": 42322.50},\n"
            "  {\"id\": \"alripay\", \"name\": \"AlriPay Pagamentos\", \"percentage\": 13.0, \"mrr\": 19305.00}\n"
            "]\n");
    } else {
        snprintf(buf, 2048,
            "[\n"
            "  {\"id\": \"%s\", \"name\": \"%s\", \"percentage\": 100.0, \"mrr\": 42300.00}\n"
            "]\n",
            caller_company, caller_company);
    }

    if (g_dash_mutex) ar_mutex_unlock(g_dash_mutex);
    return buf;
}

void arapidash_db_close(void) {
    if (g_dash_mutex) {
        ar_mutex_destroy(g_dash_mutex);
        g_dash_mutex = NULL;
    }
}
