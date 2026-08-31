/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arapicloud_db.h"
#include "aros_hal.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

static CloudFile g_files[MAX_FILES];
static int g_files_count = 0;
static void *g_cloud_mutex = NULL;

static void get_iso_now(char *out, size_t max_len) {
    time_t t = time(NULL);
    struct tm tm_buf;
    gmtime_r(&t, &tm_buf);
    strftime(out, max_len, "%Y-%m-%d %H:%M:%S", &tm_buf);
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

int arapicloud_db_init(const char *data_dir) {
    g_cloud_mutex = ar_mutex_create();
    if (data_dir && data_dir[0]) {
        mkdir(data_dir, 0755);
    }

    // Bootstrap default files (arapicloud_db_create_file will lock mutex internally)
    if (g_files_count == 0) {
        arapicloud_db_create_file("alrigroup", "Arquitetura_ALRIOS_v1.0.pdf", 4823400, "application/pdf", "alexsanderalri");
        arapicloud_db_create_file("alrigroup", "Contrato_Social_Holding.pdf", 1240000, "application/pdf", "alexsanderalri");
        arapicloud_db_create_file("detroitgg", "Regulamento_Staff_2026.docx", 850000, "application/vnd.openxmlformats", "alexsanderalri");
        arapicloud_db_create_file("detroitgg", "Logotipos_Detroit_Vector.zip", 14200000, "application/zip", "alexsanderalri");
    }

    return 0;
}

int arapicloud_db_create_file(const char *company_id, const char *name, uint64_t size_bytes, const char *mime, const char *creator) {
    if (!company_id || !name) return -1;
    if (g_cloud_mutex) ar_mutex_lock(g_cloud_mutex);

    if (g_files_count >= MAX_FILES) {
        if (g_cloud_mutex) ar_mutex_unlock(g_cloud_mutex);
        return -1;
    }

    CloudFile *f = &g_files[g_files_count++];
    memset(f, 0, sizeof(CloudFile));
    snprintf(f->id, sizeof(f->id), "file_%03d", g_files_count);
    strncpy(f->company_id, company_id, sizeof(f->company_id) - 1);
    strncpy(f->name, name, sizeof(f->name) - 1);
    f->size_bytes = size_bytes > 0 ? size_bytes : 1024;
    strncpy(f->mime_type, mime ? mime : "application/octet-stream", sizeof(f->mime_type) - 1);
    strncpy(f->created_by, creator ? creator : "system", sizeof(f->created_by) - 1);
    get_iso_now(f->created_at, sizeof(f->created_at));

    if (g_cloud_mutex) ar_mutex_unlock(g_cloud_mutex);
    return 0;
}

int arapicloud_db_delete_file(const char *file_id, const char *caller_company, int is_master) {
    if (!file_id) return -1;
    if (g_cloud_mutex) ar_mutex_lock(g_cloud_mutex);

    int idx = -1;
    for (int i = 0; i < g_files_count; i++) {
        if (strcmp(g_files[i].id, file_id) == 0) {
            if (!is_master && caller_company && caller_company[0] && strcmp(g_files[i].company_id, caller_company) != 0) {
                if (g_cloud_mutex) ar_mutex_unlock(g_cloud_mutex);
                return -2; // 403 Forbidden
            }
            idx = i;
            break;
        }
    }

    if (idx < 0) {
        if (g_cloud_mutex) ar_mutex_unlock(g_cloud_mutex);
        return -1; // 404 Not Found
    }

    // Shift
    for (int i = idx; i < g_files_count - 1; i++) {
        g_files[i] = g_files[i + 1];
    }
    g_files_count--;

    if (g_cloud_mutex) ar_mutex_unlock(g_cloud_mutex);
    return 0;
}

char* arapicloud_db_list_files_json(const char *caller_company, int is_master) {
    if (g_cloud_mutex) ar_mutex_lock(g_cloud_mutex);

    size_t alloc_size = 32768;
    char *buf = (char*)malloc(alloc_size);
    if (!buf) {
        if (g_cloud_mutex) ar_mutex_unlock(g_cloud_mutex);
        return strdup("[]");
    }

    size_t offset = 0;
    offset += snprintf(buf + offset, alloc_size - offset, "[\n");

    int matched = 0;
    for (int i = 0; i < g_files_count; i++) {
        CloudFile *f = &g_files[i];

        if (!is_master && caller_company && caller_company[0]) {
            if (strcmp(f->company_id, caller_company) != 0 && strcmp(f->company_id, "global") != 0) {
                continue;
            }
        }

        if (matched > 0) offset += snprintf(buf + offset, alloc_size - offset, ",\n");

        char esc_name[256];
        escape_json_str(f->name, esc_name, sizeof(esc_name));

        offset += snprintf(buf + offset, alloc_size - offset,
            "  {\n"
            "    \"id\": \"%s\",\n"
            "    \"company_id\": \"%s\",\n"
            "    \"name\": \"%s\",\n"
            "    \"size_bytes\": %lu,\n"
            "    \"mime_type\": \"%s\",\n"
            "    \"created_by\": \"%s\",\n"
            "    \"created_at\": \"%s\"\n"
            "  }",
            f->id, f->company_id, esc_name, (unsigned long)f->size_bytes, f->mime_type, f->created_by, f->created_at);
        matched++;
    }

    offset += snprintf(buf + offset, alloc_size - offset, "\n]");
    if (g_cloud_mutex) ar_mutex_unlock(g_cloud_mutex);
    return buf;
}

char* arapicloud_db_get_quota_json(const char *caller_company, int is_master) {
    if (g_cloud_mutex) ar_mutex_lock(g_cloud_mutex);

    uint64_t used_bytes = 0;
    int total_files = 0;

    for (int i = 0; i < g_files_count; i++) {
        CloudFile *f = &g_files[i];
        if (is_master || !caller_company || !caller_company[0] || strcmp(f->company_id, caller_company) == 0) {
            used_bytes += f->size_bytes;
            total_files++;
        }
    }

    uint64_t quota_limit_bytes = is_master ? 53687091200ULL : 21474836480ULL; // 50GB Master vs 20GB Subsidiary

    char *buf = (char*)malloc(1024);
    if (!buf) {
        if (g_cloud_mutex) ar_mutex_unlock(g_cloud_mutex);
        return strdup("{}");
    }

    double used_mb = (double)used_bytes / (1024.0 * 1024.0);
    double quota_gb = (double)quota_limit_bytes / (1024.0 * 1024.0 * 1024.0);
    double percent_used = ((double)used_bytes / (double)quota_limit_bytes) * 100.0;

    snprintf(buf, 1024,
        "{\n"
        "  \"company_id\": \"%s\",\n"
        "  \"total_files\": %d,\n"
        "  \"used_bytes\": %lu,\n"
        "  \"used_mb\": %.2f,\n"
        "  \"quota_limit_bytes\": %lu,\n"
        "  \"quota_limit_gb\": %.1f,\n"
        "  \"percent_used\": %.2f\n"
        "}\n",
        (is_master || !caller_company || !caller_company[0]) ? "alrigroup" : caller_company,
        total_files,
        (unsigned long)used_bytes,
        used_mb,
        (unsigned long)quota_limit_bytes,
        quota_gb,
        percent_used);

    if (g_cloud_mutex) ar_mutex_unlock(g_cloud_mutex);
    return buf;
}

void arapicloud_db_close(void) {
    if (g_cloud_mutex) {
        ar_mutex_destroy(g_cloud_mutex);
        g_cloud_mutex = NULL;
    }
}
