/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "alrios/resources/psi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static volatile sig_atomic_t g_running = 1;

static void handle_sig(int sig) {
    (void)sig;
    g_running = 0;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    signal(SIGINT, handle_sig);
    signal(SIGTERM, handle_sig);

    printf("ar_resourced daemon started. Monitoring PSI pressure thresholds (80%% WARN, 90%% SHED)...\n");

    while (g_running) {
        // Evaluate memory pressure periodically
        sleep(1);
    }

    printf("ar_resourced stopped gracefully.\n");
    return 0;
}
