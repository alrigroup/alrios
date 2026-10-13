/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/loader/sovereign_loader.h"

int alrios_sovereign_loader_stage_memfd(
    const uint8_t *package_data,
    size_t package_len,
    uint64_t now,
    uint32_t policy_flags,
    const alrios_sovereign_keys_t *keys,
    int *out_memfd
) {
    (void)package_data;
    (void)package_len;
    (void)now;
    (void)policy_flags;
    (void)keys;
    if (out_memfd) *out_memfd = -1;
    return ALRIOS_SOVEREIGN_ERR_PLATFORM;
}

int alrios_sovereign_execute_package(
    const uint8_t *package_data,
    size_t package_len,
    uint64_t now,
    int require_pqc,
    const alrios_sovereign_keys_t *keys,
    char *const argv[],
    char *const envp[],
    int *out_child_status
) {
    (void)package_data;
    (void)package_len;
    (void)now;
    (void)require_pqc;
    (void)keys;
    (void)argv;
    (void)envp;
    if (out_child_status) *out_child_status = -1;
    return ALRIOS_SOVEREIGN_ERR_PLATFORM;
}
