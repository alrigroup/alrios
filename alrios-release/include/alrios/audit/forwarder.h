/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_AUDIT_FORWARDER_H
#define ALRIOS_AUDIT_FORWARDER_H

#include "alrios/audit/block.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct alrios_audit_checkpoint {
    uint64_t sequence;
    uint64_t timestamp;
    uint8_t root_hash[ALRIOS_AUDIT_HASH_SIZE];
    uint8_t signature[64];
} alrios_audit_checkpoint_t;

int alrios_audit_create_checkpoint(const char *audit_log_path, alrios_audit_checkpoint_t *out_checkpoint);

int alrios_audit_forward_checkpoint(int socket_fd, const alrios_audit_checkpoint_t *checkpoint, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif
