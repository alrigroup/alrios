/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */
#ifndef AR_KERNEL_H
#define AR_KERNEL_H

#include <stdint.h>
#include "ar_svc.h"

#define AR_MAX_SERVICES 64

int ar_init(void);
int ar_shutdown(void);
int ar_run(void);

void ar_health_init(uint64_t interval_ms);
int  ar_health_register(const char *svc_name);
int  ar_health_ping(const char *svc_name);
int  ar_health_check(const char *svc_name);
int  ar_health_is_alive(const char *svc_name);

int ar_app_repack(const char *app_name);
int ar_app_storage_dir(const char *app_name, char *out, int out_size);

#endif
