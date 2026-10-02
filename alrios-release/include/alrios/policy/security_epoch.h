/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_POLICY_SECURITY_EPOCH_H
#define ALRIOS_POLICY_SECURITY_EPOCH_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALRIOS_EPOCH_OK 0
#define ALRIOS_EPOCH_ERR_INVALID -1001
#define ALRIOS_EPOCH_ERR_ROLLBACK -1002
#define ALRIOS_EPOCH_ERR_POLICY -1003

typedef struct alrios_security_state {
    uint64_t current_epoch;
    uint64_t last_timestamp;
    uint64_t min_allowed_epoch;
} alrios_security_state_t;

int alrios_epoch_init(alrios_security_state_t *state, uint64_t initial_epoch, uint64_t initial_timestamp);
int alrios_epoch_verify_time(alrios_security_state_t *state, uint64_t new_timestamp);
int alrios_epoch_advance(alrios_security_state_t *state, uint64_t new_epoch, uint64_t new_timestamp);
int alrios_epoch_verify_crypto_policy(uint64_t policy_version, uint64_t required_min_version);

#ifdef __cplusplus
}
#endif

#endif
