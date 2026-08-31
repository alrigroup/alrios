/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arapictrl_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static ArapictrlConfig g_cfg;
static int g_loaded = 0;

static void set_defaults(void) {
    memset(&g_cfg, 0, sizeof(g_cfg));
    strncpy(g_cfg.app_id, "arapictrl", sizeof(g_cfg.app_id) - 1);
    strncpy(g_cfg.app_name, "ALRI Sovereign Master Infrastructure Control Plane API Backend", sizeof(g_cfg.app_name) - 1);
    g_cfg.server_port = 9700;
    strncpy(g_cfg.server_bind, "127.0.0.1", sizeof(g_cfg.server_bind) - 1);
    strncpy(g_cfg.route_prefix, "/arapi/ctrl", sizeof(g_cfg.route_prefix) - 1);
    strncpy(g_cfg.auth_host, "127.0.0.1", sizeof(g_cfg.auth_host) - 1);
    g_cfg.auth_port = 9650;
    strncpy(g_cfg.logs_host, "127.0.0.1", sizeof(g_cfg.logs_host) - 1);
    g_cfg.logs_port = 9655;
    strncpy(g_cfg.data_dir, "storage/arctrl", sizeof(g_cfg.data_dir) - 1);
}

static char* trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    return s;
}

int arapictrl_config_load(const char *path) {
    set_defaults();
    if (!path) {
        g_loaded = 1;
        return 0;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        g_loaded = 1;
        return 0;
    }

    char line[256];
    char section[64] = {0};

    while (fgets(line, sizeof(line), f)) {
        char *p = trim(line);
        if (!p[0] || p[0] == '#' || p[0] == ';') continue;

        if (p[0] == '[' && p[strlen(p) - 1] == ']') {
            strncpy(section, p + 1, sizeof(section) - 1);
            section[strlen(section) - 1] = '\0';
            continue;
        }

        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = trim(p);
        char *val = trim(eq + 1);

        if (strcmp(key, "port") == 0) {
            if (strcmp(section, "auth") == 0) g_cfg.auth_port = atoi(val);
            else if (strcmp(section, "logs") == 0) g_cfg.logs_port = atoi(val);
            else g_cfg.server_port = atoi(val);
        } else if (strcmp(key, "bind") == 0) {
            strncpy(g_cfg.server_bind, val, sizeof(g_cfg.server_bind) - 1);
        } else if (strcmp(key, "host") == 0) {
            if (strcmp(section, "auth") == 0) strncpy(g_cfg.auth_host, val, sizeof(g_cfg.auth_host) - 1);
            else if (strcmp(section, "logs") == 0) strncpy(g_cfg.logs_host, val, sizeof(g_cfg.logs_host) - 1);
        } else if (strcmp(key, "route_prefix") == 0) {
            strncpy(g_cfg.route_prefix, val, sizeof(g_cfg.route_prefix) - 1);
        } else if (strcmp(key, "data_dir") == 0) {
            strncpy(g_cfg.data_dir, val, sizeof(g_cfg.data_dir) - 1);
        }
    }

    fclose(f);
    g_loaded = 1;
    return 0;
}

ArapictrlConfig* arapictrl_config_get(void) {
    if (!g_loaded) set_defaults();
    return &g_cfg;
}
