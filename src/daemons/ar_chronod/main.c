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

#include "alrios/policy/security_epoch.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

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

    printf("ar_chronod secure time monitor and security epoch daemon started.\n");

    alrios_security_state_t state;
    alrios_epoch_init(&state, 1ULL, (uint64_t)time(NULL));

    while (g_running) {
        uint64_t now = (uint64_t)time(NULL);
        if (alrios_epoch_verify_time(&state, now) != ALRIOS_EPOCH_OK) {
            fprintf(stderr, "SECURITY ALERT: Backward time jump detected! Enforcing anti-rollback.\n");
        }
        sleep(1);
    }

    printf("ar_chronod stopped gracefully.\n");
    return 0;
}
