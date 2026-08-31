/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARAPICLOUD_DB_H
#define ARAPICLOUD_DB_H

#include <stdint.h>
#include <stddef.h>

#define MAX_FILES 512
#define MAX_FOLDERS 64

typedef struct {
    char id[64];
    char company_id[64];
    char name[128];
    uint64_t size_bytes;
    char mime_type[64];
    char folder_id[64];
    char created_by[64];
    char created_at[32];
} CloudFile;

typedef struct {
    char id[64];
    char company_id[64];
    char name[128];
    char parent_id[64];
    char created_by[64];
} CloudFolder;

int arapicloud_db_init(const char *data_dir);

// Files CRUD
char* arapicloud_db_list_files_json(const char *caller_company, int is_master);
int arapicloud_db_create_file(const char *company_id, const char *name, uint64_t size_bytes, const char *mime, const char *creator);
int arapicloud_db_delete_file(const char *file_id, const char *caller_company, int is_master);

// Quota
char* arapicloud_db_get_quota_json(const char *caller_company, int is_master);

void arapicloud_db_close(void);

#endif /* ARAPICLOUD_DB_H */
