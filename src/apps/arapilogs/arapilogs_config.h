/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARAPILOGS_CONFIG_H
#define ARAPILOGS_CONFIG_H

typedef struct {
    char app_id[64];
    char app_name[128];
    int server_port;
    char server_bind[64];
    char route_prefix[64];
    char auth_host[64];
    int auth_port;
    char data_dir[256];
    int max_logs;
} ArapilogsConfig;

int arapilogs_config_load(const char *path);
ArapilogsConfig* arapilogs_config_get(void);

#endif /* ARAPILOGS_CONFIG_H */
