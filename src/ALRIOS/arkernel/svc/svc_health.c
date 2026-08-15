/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "ar_kernel.h"
#include "ar_svc.h"
#include "aros_hal.h"

#include <string.h>

#define MAX_HEARTBEAT_SKIP 3

typedef struct {
    char     svc_name[64];
    uint64_t last_ping_ms;
    int      missed_pings;
    int      active;
} heartbeat_entry_t;

static heartbeat_entry_t heartbeats[AR_MAX_SERVICES];
static int               heartbeat_count = 0;
static uint64_t          heartbeat_interval_ms = 1000;

static heartbeat_entry_t *find_heartbeat(const char *name) {
    for (int i = 0; i < heartbeat_count; i++)
        if (strcmp(heartbeats[i].svc_name, name) == 0)
            return &heartbeats[i];
    return NULL;
}

void ar_health_init(uint64_t interval_ms) {
    heartbeat_interval_ms = interval_ms > 0 ? interval_ms : 1000;
    heartbeat_count = 0;
}

int ar_health_register(const char *svc_name) {
    if (!svc_name) return -1;
    if (heartbeat_count >= AR_MAX_SERVICES) return -1;
    if (find_heartbeat(svc_name)) return -1;

    heartbeat_entry_t *h = &heartbeats[heartbeat_count++];
    strncpy(h->svc_name, svc_name, sizeof(h->svc_name) - 1);
    h->svc_name[sizeof(h->svc_name) - 1] = '\0';
    h->last_ping_ms = ar_time_ms();
    h->missed_pings = 0;
    h->active = 1;

    return 0;
}

int ar_health_ping(const char *svc_name) {
    heartbeat_entry_t *h = find_heartbeat(svc_name);
    if (!h) return -1;

    h->last_ping_ms = ar_time_ms();
    h->missed_pings = 0;
    return 0;
}

int ar_health_check(const char *svc_name) {
    heartbeat_entry_t *h = find_heartbeat(svc_name);
    if (!h) return 0;

    uint64_t now = ar_time_ms();
    uint64_t elapsed = now - h->last_ping_ms;

    if (elapsed > heartbeat_interval_ms) {
        int skips = (int)(elapsed / heartbeat_interval_ms);
        h->missed_pings += skips;
    }

    return h->missed_pings < MAX_HEARTBEAT_SKIP;
}

int ar_health_is_alive(const char *svc_name) {
    heartbeat_entry_t *h = find_heartbeat(svc_name);
    if (!h) return 0;

    uint64_t now = ar_time_ms();
    return (now - h->last_ping_ms) < (heartbeat_interval_ms * MAX_HEARTBEAT_SKIP);
}
