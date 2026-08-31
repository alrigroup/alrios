/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARAPICONN_DB_H
#define ARAPICONN_DB_H

#include <stdint.h>
#include <stddef.h>

#define MAX_CHANNELS 64
#define MAX_MESSAGES 1024
#define MAX_TASKS 512
#define MAX_DMS 1024

typedef struct {
    char id[64];
    char company_id[64];
    char name[64];
    char topic[256];
    int is_private;
    char created_by[64];
} ConnChannel;

typedef struct {
    uint64_t id;
    char channel_id[64];
    char sender_user[64];
    char sender_company[64];
    char content[1024];
    char timestamp[32];
} ConnMessage;

typedef struct {
    char id[64];
    char company_id[64];
    char title[128];
    char description[512];
    char assignee[64];
    char status[32]; // todo, in_progress, done
    char priority[32]; // low, medium, high, urgent
    char created_by[64];
    char created_at[32];
} ConnTask;

typedef struct {
    uint64_t id;
    char sender[64];
    char recipient[64];
    char content[1024];
    char timestamp[32];
} ConnDirectMessage;

int arapiconn_db_init(const char *data_dir);

// Channels
char* arapiconn_db_list_channels_json(const char *caller_company, int is_master);
int arapiconn_db_create_channel(const char *company_id, const char *name, const char *topic, int is_private, const char *creator);

// Channel Messages
char* arapiconn_db_list_messages_json(const char *channel_id, int limit);
int arapiconn_db_post_message(const char *channel_id, const char *sender, const char *company, const char *content);

// Tasks / Kanban
char* arapiconn_db_list_tasks_json(const char *caller_company, int is_master);
int arapiconn_db_create_task(const char *company_id, const char *title, const char *desc, const char *assignee, const char *priority, const char *creator);
int arapiconn_db_update_task_status(const char *task_id, const char *new_status);

// Direct Messages (Private - Zero-DM Logs)
char* arapiconn_db_list_dms_json(const char *user1, const char *user2);
int arapiconn_db_send_dm(const char *sender, const char *recipient, const char *content);

void arapiconn_db_close(void);

#endif /* ARAPICONN_DB_H */
