/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef AR_PATH_H
#define AR_PATH_H

#define AR_PATH_MAX 16
#define AR_PATH_NAME_MAX 64
#define AR_PATH_ENTRY_MAX 1024

typedef struct {
    char name[AR_PATH_NAME_MAX];
    char path[AR_PATH_ENTRY_MAX];
} ar_path_entry_t;

int ar_path_register(const char *name, const char *full_path);
int ar_path_exists(const char *name);
const char *ar_path_find(const char *name);

#endif
