/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

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
