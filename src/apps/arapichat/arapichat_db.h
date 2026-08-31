/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARAPICHAT_DB_H
#define ARAPICHAT_DB_H

#include <stdint.h>
#include <stddef.h>

#define MAX_TICKETS 256
#define MAX_TICKET_MSGS 1024

typedef struct {
    char id[64];
    char company_id[64];
    char customer_name[128];
    char customer_email[128];
    char subject[256];
    char status[32]; // open, in_progress, resolved, closed
    char priority[32]; // normal, high, urgent
    char created_at[32];
} ChatTicket;

typedef struct {
    uint64_t id;
    char ticket_id[64];
    char sender_type[16]; // customer, staff
    char sender_name[64];
    char content[1024];
    char timestamp[32];
} ChatTicketMessage;

int arapichat_db_init(const char *data_dir);

// Tickets CRUD
char* arapichat_db_list_tickets_json(const char *caller_company, int is_master);
int arapichat_db_create_ticket(const char *company_id, const char *cust_name, const char *cust_email, const char *subject, const char *initial_msg);
int arapichat_db_update_ticket_status(const char *ticket_id, const char *status);

// Messages in a ticket
char* arapichat_db_list_messages_json(const char *ticket_id);
int arapichat_db_add_message(const char *ticket_id, const char *sender_type, const char *sender_name, const char *content);

void arapichat_db_close(void);

#endif /* ARAPICHAT_DB_H */
