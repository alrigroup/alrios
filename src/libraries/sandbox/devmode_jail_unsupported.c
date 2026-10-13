/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/sandbox/devmode_jail.h"

int alrios_devmode_jail_validate_admission(
    alrios_profile_t node_profile,
    int devmode_allowed_on_node,
    int app_marked_devmode,
    int app_signed
) {
    (void)node_profile;
    (void)devmode_allowed_on_node;
    (void)app_marked_devmode;
    (void)app_signed;
    return ALRIOS_DEVMODE_JAIL_ERR_UNSHARE;
}

int alrios_devmode_jail_setup_cgroups(
    const alrios_devmode_jail_config_t *config
) {
    (void)config;
    return ALRIOS_DEVMODE_JAIL_ERR_CGROUP;
}

int alrios_devmode_jail_cleanup_cgroups(const char *slot_id) {
    (void)slot_id;
    return ALRIOS_DEVMODE_JAIL_ERR_CGROUP;
}

int alrios_devmode_jail_apply_namespaces(
    const alrios_devmode_jail_config_t *config
) {
    (void)config;
    return ALRIOS_DEVMODE_JAIL_ERR_UNSHARE;
}

int alrios_devmode_jail_apply_seccomp(void) {
    return ALRIOS_DEVMODE_JAIL_ERR_SECCOMP;
}

int alrios_devmode_jail_enter(
    alrios_profile_t node_profile,
    int devmode_allowed_on_node,
    int app_marked_devmode,
    int app_signed,
    const alrios_devmode_jail_config_t *config
) {
    (void)node_profile;
    (void)devmode_allowed_on_node;
    (void)app_marked_devmode;
    (void)app_signed;
    (void)config;
    return ALRIOS_DEVMODE_JAIL_ERR_UNSHARE;
}
