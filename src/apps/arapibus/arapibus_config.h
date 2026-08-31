/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARAPIBUS_CONFIG_H
#define ARAPIBUS_CONFIG_H

typedef struct {
    char app_id[64];
    char app_name[128];
    int server_port;
    char server_bind[64];
    char route_prefix[64];
    char auth_host[64];
    int auth_port;
    char logs_host[64];
    int logs_port;
    char data_dir[256];
} ArapibusConfig;

int arapibus_config_load(const char *path);
ArapibusConfig* arapibus_config_get(void);

#endif /* ARAPIBUS_CONFIG_H */
