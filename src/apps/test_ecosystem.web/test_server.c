/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arwn.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int get_exe_dir(char *buf, size_t size) {
    ssize_t len = readlink("/proc/self/exe", buf, size - 1);
    if (len < 0) return -1;
    buf[len] = '\0';
    char *p = strrchr(buf, '/');
    if (p) *p = '\0';
    return 0;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    char dir[1024];
    if (get_exe_dir(dir, sizeof(dir)) != 0) {
        strncpy(dir, ".", sizeof(dir) - 1);
    }

    printf("[test-ecosystem-web] Starting ARWN native server in %s...\n", dir);
    arwn_app_t *app = arwn_app_new(dir);
    if (!app) {
        fprintf(stderr, "[test-ecosystem-web] Failed to create arwn app\n");
        return 1;
    }

    char cfg_path[1200];
    snprintf(cfg_path, sizeof(cfg_path), "%s/config.arwn", dir);

    if (arwn_config_load(app, cfg_path) != 0) {
        /* Fallback direto na pasta atual */
        if (arwn_config_load(app, "config.arwn") != 0) {
            fprintf(stderr, "[test-ecosystem-web] Failed to load config.arwn\n");
            arwn_app_free(app);
            return 1;
        }
    }

    int rc = arwn_mount(app);
    arwn_app_free(app);
    return rc;
}
