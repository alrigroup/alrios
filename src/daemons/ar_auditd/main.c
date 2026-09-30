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

#include "alrios/audit/block.h"
#include "alrios/audit/forwarder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static volatile sig_atomic_t g_running = 1;

static void handle_signal(int sig) {
    (void)sig;
    g_running = 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <audit_log_path>\n", argv[0]);
        return 1;
    }

    const char *log_path = argv[1];
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("ar_auditd started monitoring audit log: %s\n", log_path);

    while (g_running) {
        alrios_audit_checkpoint_t cp;
        int rc = alrios_audit_create_checkpoint(log_path, &cp);
        if (rc == ALRIOS_AUDIT_OK) {
            // Checkpoint created successfully
        }
        sleep(1);
    }

    printf("ar_auditd stopped gracefully.\n");
    return 0;
}
