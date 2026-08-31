/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arapiconn_db.h"
#include "aros_hal.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

static ConnChannel g_channels[MAX_CHANNELS];
static int g_channels_count = 0;

static ConnMessage g_messages[MAX_MESSAGES];
static int g_messages_count = 0;
static uint64_t g_next_msg_id = 1;

static ConnTask g_tasks[MAX_TASKS];
static int g_tasks_count = 0;

static ConnDirectMessage g_dms[MAX_DMS];
static int g_dms_count = 0;
static uint64_t g_next_dm_id = 1;

static void *g_conn_mutex = NULL;

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

int arapiconn_db_init(const char *data_dir) {
    g_conn_mutex = ar_mutex_create();
    if (data_dir && data_dir[0]) {
        mkdir(data_dir, 0755);
    }

    if (g_conn_mutex) ar_mutex_lock(g_conn_mutex);

    // Bootstrap default channels
    if (g_channels_count == 0) {
        arapiconn_db_create_channel("alrigroup", "geral", "Canal geral da holding ALRIGROUP", 0, "alexsanderalri");
        arapiconn_db_create_channel("alrigroup", "desenvolvimento", "Arquitetura e engenharia do ecossistema ALRIOS", 0, "alexsanderalri");
        arapiconn_db_create_channel("detroitgg", "staff-detroit", "Coordenação e atendimento Detroit GG", 0, "alexsanderalri");
        arapiconn_db_create_channel("detroitgg", "eventos", "Planejamento de eventos e torneios", 0, "alexsanderalri");
    }

    // Bootstrap default initial messages
    if (g_messages_count == 0) {
        arapiconn_db_post_message("geral", "alexsanderalri", "alrigroup", "Bem-vindos ao ALRI-Connect! Comunicação e Kanban integrados.");
        arapiconn_db_post_message("desenvolvimento", "alexsanderalri", "alrigroup", "Iniciando a Fase 2: Módulos ARCONN, ARDASH e ARCHAT.");
    }

    // Bootstrap default Kanban tasks
    if (g_tasks_count == 0) {
        arapiconn_db_create_task("alrigroup", "Deploy da Nova Arquitetura Soberana", "Separar todos os 10 microsserviços do ecossistema", "alexsanderalri", "urgent", "alexsanderalri");
        arapiconn_db_create_task("alrigroup", "Auditoria de Segurança Zero-Trust", "Validar isolamento multi-empresas e sigilo de DMs", "alexsanderalri", "high", "alexsanderalri");
        arapiconn_db_create_task("detroitgg", "Configurar Staff e Regimento", "Atualizar os termos de serviço e canal de suporte", "alexsanderalri", "medium", "alexsanderalri");
    }

    if (g_conn_mutex) ar_mutex_unlock(g_conn_mutex);
    return 0;
}

// -----------------------------------------------------------------------------
// CHANNELS
// -----------------------------------------------------------------------------
int arapiconn_db_create_channel(const char *company_id, const char *name, const char *topic, int is_private, const char *creator) {
    if (!company_id || !name) return -1;
    if (g_channels_count >= MAX_CHANNELS) return -1;

    for (int i = 0; i < g_channels_count; i++) {
        if (strcmp(g_channels[i].name, name) == 0 && strcmp(g_channels[i].company_id, company_id) == 0) {
            return 0; // exists
        }
    }

    ConnChannel *c = &g_channels[g_channels_count++];
    memset(c, 0, sizeof(ConnChannel));
    snprintf(c->id, sizeof(c->id), "%s", name);
    strncpy(c->company_id, company_id, sizeof(c->company_id) - 1);
    strncpy(c->name, name, sizeof(c->name) - 1);
    strncpy(c->topic, topic ? topic : "", sizeof(c->topic) - 1);
    c->is_private = is_private;
    strncpy(c->created_by, creator ? creator : "system", sizeof(c->created_by) - 1);
    return 0;
}

char* arapiconn_db_list_channels_json(const char *caller_company, int is_master) {
    if (g_conn_mutex) ar_mutex_lock(g_conn_mutex);

    size_t alloc_size = 16384;
    char *buf = (char*)malloc(alloc_size);
    if (!buf) {
        if (g_conn_mutex) ar_mutex_unlock(g_conn_mutex);
        return strdup("[]");
    }

    size_t offset = 0;
    offset += snprintf(buf + offset, alloc_size - offset, "[\n");

    int matched = 0;
    for (int i = 0; i < g_channels_count; i++) {
        ConnChannel *c = &g_channels[i];

        if (!is_master && caller_company && caller_company[0]) {
            if (strcmp(c->company_id, caller_company) != 0 && strcmp(c->company_id, "global") != 0) {
                continue;
            }
        }

        if (matched > 0) offset += snprintf(buf + offset, alloc_size - offset, ",\n");

        char esc_topic[300];
        escape_json_str(c->topic, esc_topic, sizeof(esc_topic));

        offset += snprintf(buf + offset, alloc_size - offset,
            "  {\n"
            "    \"id\": \"%s\",\n"
            "    \"company_id\": \"%s\",\n"
            "    \"name\": \"%s\",\n"
            "    \"topic\": \"%s\",\n"
            "    \"is_private\": %s,\n"
            "    \"created_by\": \"%s\"\n"
            "  }",
            c->id, c->company_id, c->name, esc_topic, c->is_private ? "true" : "false", c->created_by);
        matched++;
    }

    offset += snprintf(buf + offset, alloc_size - offset, "\n]");
    if (g_conn_mutex) ar_mutex_unlock(g_conn_mutex);
    return buf;
}

// -----------------------------------------------------------------------------
// MESSAGES
// -----------------------------------------------------------------------------
int arapiconn_db_post_message(const char *channel_id, const char *sender, const char *company, const char *content) {
    if (!channel_id || !sender || !content) return -1;
    if (g_messages_count >= MAX_MESSAGES) {
        // Shift circular
        memmove(&g_messages[0], &g_messages[1], sizeof(ConnMessage) * (MAX_MESSAGES - 1));
        g_messages_count = MAX_MESSAGES - 1;
    }

    ConnMessage *m = &g_messages[g_messages_count++];
    memset(m, 0, sizeof(ConnMessage));
    m->id = g_next_msg_id++;
    strncpy(m->channel_id, channel_id, sizeof(m->channel_id) - 1);
    strncpy(m->sender_user, sender, sizeof(m->sender_user) - 1);
    strncpy(m->sender_company, company ? company : "alrigroup", sizeof(m->sender_company) - 1);
    strncpy(m->content, content, sizeof(m->content) - 1);
    get_iso_now(m->timestamp, sizeof(m->timestamp));
    return 0;
}

char* arapiconn_db_list_messages_json(const char *channel_id, int limit) {
    if (!channel_id) return strdup("[]");
    if (limit <= 0 || limit > 200) limit = 100;

    if (g_conn_mutex) ar_mutex_lock(g_conn_mutex);

    size_t alloc_size = 32768;
    char *buf = (char*)malloc(alloc_size);
    if (!buf) {
        if (g_conn_mutex) ar_mutex_unlock(g_conn_mutex);
        return strdup("[]");
    }

    size_t offset = 0;
    offset += snprintf(buf + offset, alloc_size - offset, "[\n");

    int matched = 0;
    for (int i = 0; i < g_messages_count && matched < limit; i++) {
        ConnMessage *m = &g_messages[i];
        if (strcmp(m->channel_id, channel_id) != 0) continue;

        if (matched > 0) offset += snprintf(buf + offset, alloc_size - offset, ",\n");

        char esc_content[1200];
        escape_json_str(m->content, esc_content, sizeof(esc_content));

        offset += snprintf(buf + offset, alloc_size - offset,
            "  {\n"
            "    \"id\": %lu,\n"
            "    \"channel_id\": \"%s\",\n"
            "    \"sender_user\": \"%s\",\n"
            "    \"sender_company\": \"%s\",\n"
            "    \"content\": \"%s\",\n"
            "    \"timestamp\": \"%s\"\n"
            "  }",
            (unsigned long)m->id, m->channel_id, m->sender_user, m->sender_company, esc_content, m->timestamp);
        matched++;
    }

    offset += snprintf(buf + offset, alloc_size - offset, "\n]");
    if (g_conn_mutex) ar_mutex_unlock(g_conn_mutex);
    return buf;
}

// -----------------------------------------------------------------------------
// TASKS / KANBAN
// -----------------------------------------------------------------------------
int arapiconn_db_create_task(const char *company_id, const char *title, const char *desc, const char *assignee, const char *priority, const char *creator) {
    if (!company_id || !title) return -1;
    if (g_tasks_count >= MAX_TASKS) return -1;

    ConnTask *t = &g_tasks[g_tasks_count++];
    memset(t, 0, sizeof(ConnTask));
    snprintf(t->id, sizeof(t->id), "tsk_%d", g_tasks_count);
    strncpy(t->company_id, company_id, sizeof(t->company_id) - 1);
    strncpy(t->title, title, sizeof(t->title) - 1);
    strncpy(t->description, desc ? desc : "", sizeof(t->description) - 1);
    strncpy(t->assignee, assignee ? assignee : "unassigned", sizeof(t->assignee) - 1);
    strncpy(t->status, "todo", sizeof(t->status) - 1);
    strncpy(t->priority, priority ? priority : "medium", sizeof(t->priority) - 1);
    strncpy(t->created_by, creator ? creator : "system", sizeof(t->created_by) - 1);
    get_iso_now(t->created_at, sizeof(t->created_at));
    return 0;
}

int arapiconn_db_update_task_status(const char *task_id, const char *new_status) {
    if (!task_id || !new_status) return -1;
    if (g_conn_mutex) ar_mutex_lock(g_conn_mutex);

    int ok = -1;
    for (int i = 0; i < g_tasks_count; i++) {
        if (strcmp(g_tasks[i].id, task_id) == 0) {
            strncpy(g_tasks[i].status, new_status, sizeof(g_tasks[i].status) - 1);
            ok = 0;
            break;
        }
    }
    if (g_conn_mutex) ar_mutex_unlock(g_conn_mutex);
    return ok;
}

char* arapiconn_db_list_tasks_json(const char *caller_company, int is_master) {
    if (g_conn_mutex) ar_mutex_lock(g_conn_mutex);

    size_t alloc_size = 32768;
    char *buf = (char*)malloc(alloc_size);
    if (!buf) {
        if (g_conn_mutex) ar_mutex_unlock(g_conn_mutex);
        return strdup("[]");
    }

    size_t offset = 0;
    offset += snprintf(buf + offset, alloc_size - offset, "[\n");

    int matched = 0;
    for (int i = 0; i < g_tasks_count; i++) {
        ConnTask *t = &g_tasks[i];

        if (!is_master && caller_company && caller_company[0]) {
            if (strcmp(t->company_id, caller_company) != 0) continue;
        }

        if (matched > 0) offset += snprintf(buf + offset, alloc_size - offset, ",\n");

        char esc_desc[600];
        escape_json_str(t->description, esc_desc, sizeof(esc_desc));

        offset += snprintf(buf + offset, alloc_size - offset,
            "  {\n"
            "    \"id\": \"%s\",\n"
            "    \"company_id\": \"%s\",\n"
            "    \"title\": \"%s\",\n"
            "    \"description\": \"%s\",\n"
            "    \"assignee\": \"%s\",\n"
            "    \"status\": \"%s\",\n"
            "    \"priority\": \"%s\",\n"
            "    \"created_by\": \"%s\",\n"
            "    \"created_at\": \"%s\"\n"
            "  }",
            t->id, t->company_id, t->title, esc_desc, t->assignee, t->status, t->priority, t->created_by, t->created_at);
        matched++;
    }

    offset += snprintf(buf + offset, alloc_size - offset, "\n]");
    if (g_conn_mutex) ar_mutex_unlock(g_conn_mutex);
    return buf;
}

// -----------------------------------------------------------------------------
// DIRECT MESSAGES (DMs) — 100% Isolated in memory / local private
// -----------------------------------------------------------------------------
int arapiconn_db_send_dm(const char *sender, const char *recipient, const char *content) {
    if (!sender || !recipient || !content) return -1;
    if (g_conn_mutex) ar_mutex_lock(g_conn_mutex);

    if (g_dms_count >= MAX_DMS) {
        memmove(&g_dms[0], &g_dms[1], sizeof(ConnDirectMessage) * (MAX_DMS - 1));
        g_dms_count = MAX_DMS - 1;
    }

    ConnDirectMessage *d = &g_dms[g_dms_count++];
    memset(d, 0, sizeof(ConnDirectMessage));
    d->id = g_next_dm_id++;
    strncpy(d->sender, sender, sizeof(d->sender) - 1);
    strncpy(d->recipient, recipient, sizeof(d->recipient) - 1);
    strncpy(d->content, content, sizeof(d->content) - 1);
    get_iso_now(d->timestamp, sizeof(d->timestamp));

    if (g_conn_mutex) ar_mutex_unlock(g_conn_mutex);
    return 0;
}

char* arapiconn_db_list_dms_json(const char *user1, const char *user2) {
    if (!user1 || !user2) return strdup("[]");
    if (g_conn_mutex) ar_mutex_lock(g_conn_mutex);

    size_t alloc_size = 32768;
    char *buf = (char*)malloc(alloc_size);
    if (!buf) {
        if (g_conn_mutex) ar_mutex_unlock(g_conn_mutex);
        return strdup("[]");
    }

    size_t offset = 0;
    offset += snprintf(buf + offset, alloc_size - offset, "[\n");

    int matched = 0;
    for (int i = 0; i < g_dms_count; i++) {
        ConnDirectMessage *d = &g_dms[i];
        int is_between = (strcmp(d->sender, user1) == 0 && strcmp(d->recipient, user2) == 0) ||
                         (strcmp(d->sender, user2) == 0 && strcmp(d->recipient, user1) == 0);
        if (!is_between) continue;

        if (matched > 0) offset += snprintf(buf + offset, alloc_size - offset, ",\n");

        char esc_content[1200];
        escape_json_str(d->content, esc_content, sizeof(esc_content));

        offset += snprintf(buf + offset, alloc_size - offset,
            "  {\n"
            "    \"id\": %lu,\n"
            "    \"sender\": \"%s\",\n"
            "    \"recipient\": \"%s\",\n"
            "    \"content\": \"%s\",\n"
            "    \"timestamp\": \"%s\"\n"
            "  }",
            (unsigned long)d->id, d->sender, d->recipient, esc_content, d->timestamp);
        matched++;
    }

    offset += snprintf(buf + offset, alloc_size - offset, "\n]");
    if (g_conn_mutex) ar_mutex_unlock(g_conn_mutex);
    return buf;
}

void arapiconn_db_close(void) {
    if (g_conn_mutex) {
        ar_mutex_destroy(g_conn_mutex);
        g_conn_mutex = NULL;
    }
}
