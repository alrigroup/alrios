/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef AR_SVC_H
#define AR_SVC_H

typedef enum {
    SVC_STOPPED,
    SVC_RUNNING,
    SVC_RESTARTING,
    SVC_CRASHED
} svc_status_t;

typedef int (*svc_entry_fn_t)(void);
typedef void (*svc_stop_fn_t)(void);

typedef struct {
    char         name[64];
    svc_entry_fn_t entry;
    svc_stop_fn_t  stop_hook;
    void         *thread_handle;
    svc_status_t  status;
} svc_t;

int        ar_svc_register(const char *name, svc_entry_fn_t entry);
int        ar_svc_set_stop_hook(const char *name, svc_stop_fn_t hook);
int        ar_svc_start(const char *name);
int        ar_svc_start_all(void);
int        ar_svc_stop(const char *name);
int        ar_svc_restart(const char *name);
svc_status_t ar_svc_status(const char *name);
int        ar_svc_get_count(void);
const char *ar_svc_get_name(int index);

#endif
