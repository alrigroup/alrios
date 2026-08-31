/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arapichat_db.h"
#include "aros_hal.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

static ChatTicket g_tickets[MAX_TICKETS];
static int g_tickets_count = 0;

static ChatTicketMessage g_messages[MAX_TICKET_MSGS];
static int g_messages_count = 0;
static uint64_t g_next_msg_id = 1;

static void *g_chat_mutex = NULL;

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

int arapichat_db_init(const char *data_dir) {
    g_chat_mutex = ar_mutex_create();
    if (data_dir && data_dir[0]) {
        mkdir(data_dir, 0755);
    }

    if (g_chat_mutex) ar_mutex_lock(g_chat_mutex);

    // Bootstrap default tickets
    if (g_tickets_count == 0) {
        arapichat_db_create_ticket("detroitgg", "Lucas Pereira", "lucas@gmail.com", "Dúvida sobre VIP e whitelist", "Olá staff, como realizo a liberação do meu passe VIP?");
        arapichat_db_create_ticket("alrigroup", "Empresa Parceira", "contato@parceiro.com", "Integração via API REST", "Gostaria de agendar uma reunião técnica para integração.");
    }

    if (g_chat_mutex) ar_mutex_unlock(g_chat_mutex);
    return 0;
}

int arapichat_db_create_ticket(const char *company_id, const char *cust_name, const char *cust_email, const char *subject, const char *initial_msg) {
    if (!company_id || !cust_name || !subject) return -1;
    if (g_tickets_count >= MAX_TICKETS) return -1;

    ChatTicket *t = &g_tickets[g_tickets_count++];
    memset(t, 0, sizeof(ChatTicket));
    snprintf(t->id, sizeof(t->id), "tkt_%03d", g_tickets_count);
    strncpy(t->company_id, company_id, sizeof(t->company_id) - 1);
    strncpy(t->customer_name, cust_name, sizeof(t->customer_name) - 1);
    strncpy(t->customer_email, cust_email ? cust_email : "", sizeof(t->customer_email) - 1);
    strncpy(t->subject, subject, sizeof(t->subject) - 1);
    strncpy(t->status, "open", sizeof(t->status) - 1);
    strncpy(t->priority, "normal", sizeof(t->priority) - 1);
    get_iso_now(t->created_at, sizeof(t->created_at));

    if (initial_msg && initial_msg[0]) {
        arapichat_db_add_message(t->id, "customer", cust_name, initial_msg);
    }
    return 0;
}

int arapichat_db_update_ticket_status(const char *ticket_id, const char *status) {
    if (!ticket_id || !status) return -1;
    if (g_chat_mutex) ar_mutex_lock(g_chat_mutex);

    int ok = -1;
    for (int i = 0; i < g_tickets_count; i++) {
        if (strcmp(g_tickets[i].id, ticket_id) == 0) {
            strncpy(g_tickets[i].status, status, sizeof(g_tickets[i].status) - 1);
            ok = 0;
            break;
        }
    }
    if (g_chat_mutex) ar_mutex_unlock(g_chat_mutex);
    return ok;
}

char* arapichat_db_list_tickets_json(const char *caller_company, int is_master) {
    if (g_chat_mutex) ar_mutex_lock(g_chat_mutex);

    size_t alloc_size = 32768;
    char *buf = (char*)malloc(alloc_size);
    if (!buf) {
        if (g_chat_mutex) ar_mutex_unlock(g_chat_mutex);
        return strdup("[]");
    }

    size_t offset = 0;
    offset += snprintf(buf + offset, alloc_size - offset, "[\n");

    int matched = 0;
    for (int i = 0; i < g_tickets_count; i++) {
        ChatTicket *t = &g_tickets[i];

        if (!is_master && caller_company && caller_company[0]) {
            if (strcmp(t->company_id, caller_company) != 0) continue;
        }

        if (matched > 0) offset += snprintf(buf + offset, alloc_size - offset, ",\n");

        char esc_subj[300], esc_cust[200];
        escape_json_str(t->subject, esc_subj, sizeof(esc_subj));
        escape_json_str(t->customer_name, esc_cust, sizeof(esc_cust));

        offset += snprintf(buf + offset, alloc_size - offset,
            "  {\n"
            "    \"id\": \"%s\",\n"
            "    \"company_id\": \"%s\",\n"
            "    \"customer_name\": \"%s\",\n"
            "    \"customer_email\": \"%s\",\n"
            "    \"subject\": \"%s\",\n"
            "    \"status\": \"%s\",\n"
            "    \"priority\": \"%s\",\n"
            "    \"created_at\": \"%s\"\n"
            "  }",
            t->id, t->company_id, esc_cust, t->customer_email, esc_subj, t->status, t->priority, t->created_at);
        matched++;
    }

    offset += snprintf(buf + offset, alloc_size - offset, "\n]");
    if (g_chat_mutex) ar_mutex_unlock(g_chat_mutex);
    return buf;
}

int arapichat_db_add_message(const char *ticket_id, const char *sender_type, const char *sender_name, const char *content) {
    if (!ticket_id || !sender_name || !content) return -1;
    if (g_messages_count >= MAX_TICKET_MSGS) {
        memmove(&g_messages[0], &g_messages[1], sizeof(ChatTicketMessage) * (MAX_TICKET_MSGS - 1));
        g_messages_count = MAX_TICKET_MSGS - 1;
    }

    ChatTicketMessage *m = &g_messages[g_messages_count++];
    memset(m, 0, sizeof(ChatTicketMessage));
    m->id = g_next_msg_id++;
    strncpy(m->ticket_id, ticket_id, sizeof(m->ticket_id) - 1);
    strncpy(m->sender_type, sender_type ? sender_type : "staff", sizeof(m->sender_type) - 1);
    strncpy(m->sender_name, sender_name, sizeof(m->sender_name) - 1);
    strncpy(m->content, content, sizeof(m->content) - 1);
    get_iso_now(m->timestamp, sizeof(m->timestamp));
    return 0;
}

char* arapichat_db_list_messages_json(const char *ticket_id) {
    if (!ticket_id) return strdup("[]");
    if (g_chat_mutex) ar_mutex_lock(g_chat_mutex);

    size_t alloc_size = 32768;
    char *buf = (char*)malloc(alloc_size);
    if (!buf) {
        if (g_chat_mutex) ar_mutex_unlock(g_chat_mutex);
        return strdup("[]");
    }

    size_t offset = 0;
    offset += snprintf(buf + offset, alloc_size - offset, "[\n");

    int matched = 0;
    for (int i = 0; i < g_messages_count; i++) {
        ChatTicketMessage *m = &g_messages[i];
        if (strcmp(m->ticket_id, ticket_id) != 0) continue;

        if (matched > 0) offset += snprintf(buf + offset, alloc_size - offset, ",\n");

        char esc_content[1200];
        escape_json_str(m->content, esc_content, sizeof(esc_content));

        offset += snprintf(buf + offset, alloc_size - offset,
            "  {\n"
            "    \"id\": %lu,\n"
            "    \"ticket_id\": \"%s\",\n"
            "    \"sender_type\": \"%s\",\n"
            "    \"sender_name\": \"%s\",\n"
            "    \"content\": \"%s\",\n"
            "    \"timestamp\": \"%s\"\n"
            "  }",
            (unsigned long)m->id, m->ticket_id, m->sender_type, m->sender_name, esc_content, m->timestamp);
        matched++;
    }

    offset += snprintf(buf + offset, alloc_size - offset, "\n]");
    if (g_chat_mutex) ar_mutex_unlock(g_chat_mutex);
    return buf;
}

void arapichat_db_close(void) {
    if (g_chat_mutex) {
        ar_mutex_destroy(g_chat_mutex);
        g_chat_mutex = NULL;
    }
}
