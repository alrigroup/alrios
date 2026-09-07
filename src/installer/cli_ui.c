/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "cli_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLR_RESET   "\033[0m"
#define CLR_BOLD    "\033[1m"
#define CLR_CYAN    "\033[1;36m"
#define CLR_GREEN   "\033[1;32m"
#define CLR_YELLOW  "\033[1;33m"
#define CLR_RED     "\033[1;31m"
#define CLR_GRAY    "\033[0;90m"

static void cli_on_log(const char *msg, void *user_data) {
    (void)user_data;
    printf("%s", msg);
    fflush(stdout);
}

static void cli_on_progress(int step, int total, int pct, const char *name, void *user_data) {
    (void)user_data;
    int bar_width = 24;
    int filled = (pct * bar_width) / 100;

    printf("\r%s[%d/%d]%s %-32s [", CLR_CYAN, step, total, CLR_RESET, name);
    for (int i = 0; i < bar_width; i++) {
        if (i < filled) printf("▓");
        else printf("░");
    }
    printf("] %3d%%", pct);
    fflush(stdout);
    if (pct == 100 || step == total) {
        printf("\n");
    }
}

static void cli_on_complete(int success, const char *msg, void *user_data) {
    (void)user_data;
    if (success) {
        printf("\n%s[OK]%s %s\n", CLR_GREEN, CLR_RESET, msg);
    } else {
        printf("\n%s[FALHA]%s %s\n", CLR_RED, CLR_RESET, msg);
    }
}

int run_cli_installer(installer_config_t *cfg) {
    printf("\n");
    printf("%s════════════════════════════════════════════════════════════════════════%s\n", CLR_CYAN, CLR_RESET);
    printf("%s           ALRIOS — Sovereign Operating System Installer               %s\n", CLR_BOLD, CLR_RESET);
    printf("%s════════════════════════════════════════════════════════════════════════%s\n\n", CLR_CYAN, CLR_RESET);

    if (!cfg->dest_dir[0]) {
        installer_get_default_paths(cfg->dest_dir, sizeof(cfg->dest_dir), NULL);
    }

    if (!cfg->is_unattended) {
        printf("Destino da instalacao [%s%s%s]: ", CLR_GREEN, cfg->dest_dir, CLR_RESET);
        char input[1024];
        if (fgets(input, sizeof(input), stdin)) {
            size_t len = strlen(input);
            if (len > 0 && input[len - 1] == '\n') input[len - 1] = '\0';
            if (strlen(input) > 0) {
                strncpy(cfg->dest_dir, input, sizeof(cfg->dest_dir) - 1);
            }
        }

        printf("\nOpcoes de instalacao:\n");
        printf("  [1] Registrar no PATH do sistema: %s%s%s\n", CLR_GREEN, cfg->add_to_path ? "SIM" : "NAO", CLR_RESET);
        printf("  [2] Auto-instalar GCC C/C++ se faltar: %s%s%s\n", CLR_GREEN, cfg->install_gcc ? "SIM" : "NAO", CLR_RESET);
        printf("  [3] Auto-instalar Node.js se faltar: %s%s%s\n", CLR_GREEN, cfg->install_node ? "SIM" : "NAO", CLR_RESET);
        printf("  [4] Auto-instalar Python se faltar: %s%s%s\n", CLR_GREEN, cfg->install_python ? "SIM" : "NAO", CLR_RESET);

        printf("\nDeseja prosseguir com a instalacao? (S/n): ");
        char conf[16];
        if (fgets(conf, sizeof(conf), stdin)) {
            if (conf[0] == 'n' || conf[0] == 'N') {
                printf("Instalacao cancelada pelo usuario.\n");
                return 0;
            }
        }
        printf("\n");
    }

    installer_callbacks_t cb = {
        .on_log = cli_on_log,
        .on_progress = cli_on_progress,
        .on_complete = cli_on_complete
    };

    return installer_run(cfg, &cb, NULL);
}
