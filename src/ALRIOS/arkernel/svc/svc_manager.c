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

static svc_t services[AR_MAX_SERVICES];
static int   service_count = 0;

static svc_t *find_svc(const char *name) {
    for (int i = 0; i < service_count; i++)
        if (strcmp(services[i].name, name) == 0)
            return &services[i];
    return NULL;
}

static void *svc_thread_wrapper(void *arg) {
    svc_t *svc = (svc_t *)arg;
    int rc = svc->entry();
    /* entry returning non-zero while we think we are running means the
       service failed to start (e.g. missing TLS certs, privileged port
       without root). Report CRASHED instead of leaving a false RUNNING. */
    if (rc != 0 && svc->status == SVC_RUNNING)
        svc->status = SVC_CRASHED;
    return NULL;
}

int ar_svc_register(const char *name, svc_entry_fn_t entry) {
    if (!name || !entry) return -1;
    if (service_count >= AR_MAX_SERVICES) return -1;
    if (find_svc(name)) return -1;

    svc_t *svc = &services[service_count++];
    strncpy(svc->name, name, sizeof(svc->name) - 1);
    svc->name[sizeof(svc->name) - 1] = '\0';
    svc->entry = entry;
    svc->stop_hook = NULL;
    svc->thread_handle = NULL;
    svc->status = SVC_STOPPED;

    return 0;
}

int ar_svc_set_stop_hook(const char *name, svc_stop_fn_t hook) {
    svc_t *svc = find_svc(name);
    if (!svc) return -1;
    svc->stop_hook = hook;
    return 0;
}

int ar_svc_start(const char *name) {
    svc_t *svc = find_svc(name);
    if (!svc) return -1;
    if (svc->status != SVC_STOPPED) return -1;

    svc->thread_handle = ar_thread_create(svc_thread_wrapper, svc);
    if (!svc->thread_handle) return -1;

    svc->status = SVC_RUNNING;
    return 0;
}

int ar_svc_stop(const char *name) {
    svc_t *svc = find_svc(name);
    if (!svc) return -1;
    if (svc->status != SVC_RUNNING && svc->status != SVC_CRASHED) return -1;

    if (svc->stop_hook)
        svc->stop_hook();
    if (svc->thread_handle) {
        ar_thread_join(svc->thread_handle);
        svc->thread_handle = NULL;
    }
    svc->status = SVC_STOPPED;
    return 0;
}

int ar_svc_restart(const char *name) {
    svc_t *svc = find_svc(name);
    if (!svc) return -1;

    if (svc->status == SVC_RUNNING) {
        svc->status = SVC_RESTARTING;
        if (svc->stop_hook)
            svc->stop_hook();
        ar_thread_join(svc->thread_handle);
        svc->thread_handle = NULL;
    }

    svc->thread_handle = ar_thread_create(svc_thread_wrapper, svc);
    if (!svc->thread_handle) {
        svc->status = SVC_CRASHED;
        return -1;
    }

    svc->status = SVC_RUNNING;
    return 0;
}

svc_status_t ar_svc_status(const char *name) {
    svc_t *svc = find_svc(name);
    if (!svc) return SVC_STOPPED;
    return svc->status;
}

int ar_svc_start_all(void) {
    int started = 0;
    for (int i = 0; i < service_count; i++) {
        if (services[i].status == SVC_STOPPED) {
            if (ar_svc_start(services[i].name) == 0)
                started++;
        }
    }
    return started;
}

int ar_svc_get_count(void) {
    return service_count;
}

const char *ar_svc_get_name(int index) {
    if (index < 0 || index >= service_count) return NULL;
    return services[index].name;
}
