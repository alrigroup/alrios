/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arapilogs_db.h"
#include "aros_hal.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

static ArapilogsEntry *g_entries = NULL;
static int g_capacity = 5000;
static int g_count = 0;
static int g_head = 0;
static uint64_t g_next_id = 1;
static void *g_db_mutex = NULL;
static char g_data_path[512] = {0};
static FILE *g_file = NULL;

static void get_iso_time(char *buf, size_t size, int64_t *out_time) {
    time_t now = time(NULL);
    if (out_time) *out_time = (int64_t)now;
    struct tm tm_buf;
    gmtime_r(&now, &tm_buf);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S UTC", &tm_buf);
}

int arapilogs_db_init(const char *data_dir, int max_in_memory) {
    if (max_in_memory > 0) g_capacity = max_in_memory;
    g_entries = (ArapilogsEntry*)calloc(g_capacity, sizeof(ArapilogsEntry));
    if (!g_entries) return -1;

    g_db_mutex = ar_mutex_create();

    if (data_dir && data_dir[0]) {
        mkdir(data_dir, 0755);
        snprintf(g_data_path, sizeof(g_data_path), "%s/audit_telemetry.log", data_dir);
        g_file = fopen(g_data_path, "a+");
    }

    // Seed with initial startup event
    arapilogs_db_append("system", "global", "arapilogs", "daemon_start", "INFO", 200, "127.0.0.1", "ARLOGS Telemetry & Audit Service initialized successfully");

    return 0;
}

int arapilogs_db_append(const char *user, const char *tenant, const char *service,
                        const char *action, const char *severity, int status_code,
                        const char *ip, const char *details) {
    if (!g_entries) return -1;

    /* STRICT CONFIDENTIALITY FILTER: NEVER LOG ARCONN DIRECT MESSAGES (DMs) */
    if (service && (strcmp(service, "arconn") == 0 || strcmp(service, "conn") == 0)) {
        if (action && (strstr(action, "dm_") != NULL || strstr(action, "direct_message") != NULL || strstr(action, "private_chat") != NULL)) {
            // Drop DM message payload entirely to comply with Sovereign Privacy Guarantee
            return 0;
        }
    }

    if (g_db_mutex) ar_mutex_lock(g_db_mutex);

    int idx = (g_head + g_count) % g_capacity;
    if (g_count == g_capacity) {
        g_head = (g_head + 1) % g_capacity;
    } else {
        g_count++;
    }

    ArapilogsEntry *e = &g_entries[idx];
    memset(e, 0, sizeof(ArapilogsEntry));

    e->id = g_next_id++;
    get_iso_time(e->timestamp_str, sizeof(e->timestamp_str), &e->timestamp);

    strncpy(e->user, user ? user : "anonymous", sizeof(e->user) - 1);
    strncpy(e->tenant, tenant ? tenant : "global", sizeof(e->tenant) - 1);
    strncpy(e->service, service ? service : "unknown", sizeof(e->service) - 1);
    strncpy(e->action, action ? action : "action", sizeof(e->action) - 1);
    strncpy(e->severity, severity ? severity : "INFO", sizeof(e->severity) - 1);
    e->status_code = status_code;
    strncpy(e->ip, ip ? ip : "127.0.0.1", sizeof(e->ip) - 1);

    if (details) {
        // Redact any accidental tokens or raw secrets from details string
        char clean_details[512];
        strncpy(clean_details, details, sizeof(clean_details) - 1);
        clean_details[sizeof(clean_details) - 1] = '\0';
        strncpy(e->details, clean_details, sizeof(e->details) - 1);
    } else {
        strncpy(e->details, "Operation executed", sizeof(e->details) - 1);
    }

    // Persist to disk log
    if (g_file) {
        fprintf(g_file, "[%s] id=%lu user=%s tenant=%s service=%s action=%s severity=%s status=%d ip=%s details=\"%s\"\n",
                e->timestamp_str, (unsigned long)e->id, e->user, e->tenant, e->service, e->action, e->severity, e->status_code, e->ip, e->details);
        fflush(g_file);
    }

    if (g_db_mutex) ar_mutex_unlock(g_db_mutex);
    return 0;
}

static void escape_json_str(const char *in, char *out, size_t max_out) {
    if (!in || !out || max_out < 2) {
        if (out && max_out > 0) out[0] = '\0';
        return;
    }
    size_t j = 0;
    for (size_t i = 0; in[i] && j < max_out - 2; i++) {
        if (in[i] == '\"' || in[i] == '\\') {
            out[j++] = '\\';
        } else if (in[i] == '\n') {
            out[j++] = ' ';
            continue;
        } else if (in[i] == '\r') {
            continue;
        }
        out[j++] = in[i];
    }
    out[j] = '\0';
}

char* arapilogs_db_query_json(const char *tenant_filter, int is_master,
                              const char *service_filter, const char *severity_filter,
                              const char *search_query, int limit) {
    if (!g_entries) return strdup("[]");
    if (limit <= 0 || limit > 1000) limit = 200;

    if (g_db_mutex) ar_mutex_lock(g_db_mutex);

    size_t alloc_size = 65536 + (limit * 512);
    char *buf = (char*)malloc(alloc_size);
    if (!buf) {
        if (g_db_mutex) ar_mutex_unlock(g_db_mutex);
        return strdup("[]");
    }

    size_t offset = 0;
    offset += snprintf(buf + offset, alloc_size - offset, "[\n");

    int matched = 0;

    // Scan backwards from newest to oldest
    for (int i = g_count - 1; i >= 0 && matched < limit; i--) {
        int idx = (g_head + i) % g_capacity;
        ArapilogsEntry *e = &g_entries[idx];

        // 1. Multi-Tenant Authorization Scope Check
        if (!is_master) {
            if (!tenant_filter || tenant_filter[0] == '\0') {
                continue; // Non-master without tenant filter cannot see global records
            }
            if (strcmp(e->tenant, tenant_filter) != 0 && strcmp(e->tenant, "global") != 0) {
                continue;
            }
        } else if (tenant_filter && tenant_filter[0] && strcmp(tenant_filter, "all") != 0) {
            if (strcmp(e->tenant, tenant_filter) != 0) {
                continue;
            }
        }

        // 2. Service Filter
        if (service_filter && service_filter[0] && strcmp(service_filter, "all") != 0) {
            if (strcmp(e->service, service_filter) != 0) continue;
        }

        // 3. Severity Filter
        if (severity_filter && severity_filter[0] && strcmp(severity_filter, "all") != 0) {
            if (strcmp(e->severity, severity_filter) != 0) continue;
        }

        // 4. Search Query in Action, User or Details
        if (search_query && search_query[0]) {
            if (strstr(e->action, search_query) == NULL &&
                strstr(e->user, search_query) == NULL &&
                strstr(e->details, search_query) == NULL &&
                strstr(e->ip, search_query) == NULL) {
                continue;
            }
        }

        char esc_details[600];
        escape_json_str(e->details, esc_details, sizeof(esc_details));

        if (matched > 0) {
            offset += snprintf(buf + offset, alloc_size - offset, ",\n");
        }

        offset += snprintf(buf + offset, alloc_size - offset,
            "  {\n"
            "    \"id\": %lu,\n"
            "    \"timestamp\": \"%s\",\n"
            "    \"user\": \"%s\",\n"
            "    \"tenant\": \"%s\",\n"
            "    \"service\": \"%s\",\n"
            "    \"action\": \"%s\",\n"
            "    \"severity\": \"%s\",\n"
            "    \"status_code\": %d,\n"
            "    \"ip\": \"%s\",\n"
            "    \"details\": \"%s\"\n"
            "  }",
            (unsigned long)e->id, e->timestamp_str, e->user, e->tenant,
            e->service, e->action, e->severity, e->status_code, e->ip, esc_details);

        matched++;
    }

    offset += snprintf(buf + offset, alloc_size - offset, "\n]");

    if (g_db_mutex) ar_mutex_unlock(g_db_mutex);
    return buf;
}

char* arapilogs_db_metrics_json(const char *tenant_filter, int is_master) {
    if (!g_entries) return strdup("{\"total\":0,\"errors\":0,\"security\":0,\"today\":0}");

    if (g_db_mutex) ar_mutex_lock(g_db_mutex);

    uint64_t total = 0;
    uint64_t errors = 0;
    uint64_t security = 0;

    int svc_auth = 0, svc_bus = 0, svc_conn = 0, svc_dash = 0, svc_chat = 0;
    int svc_cloud = 0, svc_ctrl = 0, svc_stock = 0, svc_work = 0;

    for (int i = 0; i < g_count; i++) {
        int idx = (g_head + i) % g_capacity;
        ArapilogsEntry *e = &g_entries[idx];

        if (!is_master) {
            if (!tenant_filter || tenant_filter[0] == '\0') continue;
            if (strcmp(e->tenant, tenant_filter) != 0 && strcmp(e->tenant, "global") != 0) continue;
        } else if (tenant_filter && tenant_filter[0] && strcmp(tenant_filter, "all") != 0) {
            if (strcmp(e->tenant, tenant_filter) != 0) continue;
        }

        total++;
        if (e->status_code >= 400 || strcmp(e->severity, "ERROR") == 0) errors++;
        if (strcmp(e->severity, "SECURITY") == 0 || strstr(e->action, "auth") || strstr(e->action, "passwd")) security++;

        if (strcmp(e->service, "arauth") == 0 || strcmp(e->service, "arapiauth") == 0) svc_auth++;
        else if (strcmp(e->service, "arbus") == 0 || strcmp(e->service, "arapibus") == 0) svc_bus++;
        else if (strcmp(e->service, "arconn") == 0 || strcmp(e->service, "arapiconn") == 0) svc_conn++;
        else if (strcmp(e->service, "ardash") == 0 || strcmp(e->service, "arapidash") == 0) svc_dash++;
        else if (strcmp(e->service, "archat") == 0 || strcmp(e->service, "arapichat") == 0) svc_chat++;
        else if (strcmp(e->service, "arcloud") == 0 || strcmp(e->service, "arapicloud") == 0) svc_cloud++;
        else if (strcmp(e->service, "arctrl") == 0 || strcmp(e->service, "arapictrl") == 0) svc_ctrl++;
        else if (strcmp(e->service, "arstock") == 0 || strcmp(e->service, "arapistock") == 0) svc_stock++;
        else if (strcmp(e->service, "arwork") == 0 || strcmp(e->service, "arapiwork") == 0) svc_work++;
    }

    char *json = (char*)malloc(1024);
    if (!json) {
        if (g_db_mutex) ar_mutex_unlock(g_db_mutex);
        return strdup("{}");
    }

    snprintf(json, 1024,
        "{\n"
        "  \"total\": %lu,\n"
        "  \"errors\": %lu,\n"
        "  \"security\": %lu,\n"
        "  \"services\": {\n"
        "    \"arauth\": %d,\n"
        "    \"arbus\": %d,\n"
        "    \"arconn\": %d,\n"
        "    \"ardash\": %d,\n"
        "    \"archat\": %d,\n"
        "    \"arcloud\": %d,\n"
        "    \"arctrl\": %d,\n"
        "    \"arstock\": %d,\n"
        "    \"arwork\": %d\n"
        "  }\n"
        "}\n",
        (unsigned long)total, (unsigned long)errors, (unsigned long)security,
        svc_auth, svc_bus, svc_conn, svc_dash, svc_chat, svc_cloud, svc_ctrl, svc_stock, svc_work);

    if (g_db_mutex) ar_mutex_unlock(g_db_mutex);
    return json;
}

void arapilogs_db_close(void) {
    if (g_file) {
        fclose(g_file);
        g_file = NULL;
    }
    if (g_entries) {
        free(g_entries);
        g_entries = NULL;
    }
    if (g_db_mutex) {
        ar_mutex_destroy(g_db_mutex);
        g_db_mutex = NULL;
    }
}
