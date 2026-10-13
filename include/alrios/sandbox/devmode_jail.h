/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_SANDBOX_DEVMODE_JAIL_H
#define ALRIOS_SANDBOX_DEVMODE_JAIL_H

#include "alrios/profiles.h"
#include "alrios/sandbox/devmode.h"
#include "alrios/platform/types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALRIOS_DEVMODE_JAIL_ABI_VERSION            1U

#define ALRIOS_DEVMODE_JAIL_OK                     0
#define ALRIOS_DEVMODE_JAIL_ERR_INVALID           -5201
#define ALRIOS_DEVMODE_JAIL_ERR_REJECTED          -5202
#define ALRIOS_DEVMODE_JAIL_ERR_SOVEREIGN_DENIED  -5203
#define ALRIOS_DEVMODE_JAIL_ERR_CGROUP            -5204
#define ALRIOS_DEVMODE_JAIL_ERR_UNSHARE           -5205
#define ALRIOS_DEVMODE_JAIL_ERR_SECCOMP           -5206
#define ALRIOS_DEVMODE_JAIL_ERR_NET               -5207

typedef enum alrios_devmode_net_mode {
    ALRIOS_DEVMODE_NET_ISOLATED = 1,
    ALRIOS_DEVMODE_NET_LOOPBACK_ONLY = 2
} alrios_devmode_net_mode_t;

typedef struct alrios_devmode_jail_config {
    uint32_t abi_version;
    const char *slot_id;
    pid_t target_pid;
    uint64_t memory_max_bytes;
    uint32_t pids_max;
    alrios_devmode_net_mode_t net_mode;
} alrios_devmode_jail_config_t;

int alrios_devmode_jail_validate_admission(
    alrios_profile_t node_profile,
    int devmode_allowed_on_node,
    int app_marked_devmode,
    int app_signed
);

int alrios_devmode_jail_setup_cgroups(
    const alrios_devmode_jail_config_t *config
);

int alrios_devmode_jail_cleanup_cgroups(const char *slot_id);

int alrios_devmode_jail_apply_namespaces(
    const alrios_devmode_jail_config_t *config
);

int alrios_devmode_jail_apply_seccomp(void);

int alrios_devmode_jail_enter(
    alrios_profile_t node_profile,
    int devmode_allowed_on_node,
    int app_marked_devmode,
    int app_signed,
    const alrios_devmode_jail_config_t *config
);

#ifdef __cplusplus
}
#endif

#endif /* ALRIOS_SANDBOX_DEVMODE_JAIL_H */
