/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/events/broker.h"

#include <signal.h>
#include <stdio.h>
#include <unistd.h>

static volatile sig_atomic_t running = 1;

static void stop_eventd(int signal_number) {
    (void)signal_number;
    running = 0;
}

static int default_authorize(const char *consumer_id,
                             const char *topic,
                             void *context) {
    (void)consumer_id;
    (void)topic;
    (void)context;
    return 1;
}

int main(void) {
    alrios_event_broker_t *broker = NULL;

    if (alrios_event_broker_create(256U, default_authorize, NULL, &broker) !=
        ALRIOS_EVENT_OK) {
        (void)fprintf(stderr, "ar_eventd: failed to initialize broker\n");
        return 1;
    }
    (void)signal(SIGINT, stop_eventd);
    (void)signal(SIGTERM, stop_eventd);
    while (running) {
        (void)sleep(1U);
    }
    alrios_event_broker_destroy(broker);
    return 0;
}
