/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */
#include "ar_path.h"
#include <stdio.h>
#include <string.h>

static ar_path_entry_t entries[AR_PATH_MAX];
static int count = 0;

int ar_path_register(const char *name, const char *full_path) {
    if (count >= AR_PATH_MAX) return -1;
    strncpy(entries[count].name, name, AR_PATH_NAME_MAX - 1);
    entries[count].name[AR_PATH_NAME_MAX - 1] = '\0';
    strncpy(entries[count].path, full_path, AR_PATH_ENTRY_MAX - 1);
    entries[count].path[AR_PATH_ENTRY_MAX - 1] = '\0';
    count++;
    return 0;
}

int ar_path_exists(const char *name) {
    for (int i = 0; i < count; i++)
        if (strcmp(entries[i].name, name) == 0)
            return 1;
    return 0;
}

const char *ar_path_find(const char *name) {
    for (int i = 0; i < count; i++)
        if (strcmp(entries[i].name, name) == 0)
            return entries[i].path;
    return NULL;
}
