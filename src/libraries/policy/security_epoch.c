/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "alrios/policy/security_epoch.h"
#include <string.h>

int alrios_epoch_init(alrios_security_state_t *state, uint64_t initial_epoch, uint64_t initial_timestamp) {
    if (!state) {
        return ALRIOS_EPOCH_ERR_INVALID;
    }
    state->current_epoch = initial_epoch;
    state->last_timestamp = initial_timestamp;
    state->min_allowed_epoch = initial_epoch;
    return ALRIOS_EPOCH_OK;
}

int alrios_epoch_verify_time(alrios_security_state_t *state, uint64_t new_timestamp) {
    if (!state) {
        return ALRIOS_EPOCH_ERR_INVALID;
    }
    // Backward time is strictly rejected (Anti-rollback security enforcement)
    if (new_timestamp < state->last_timestamp) {
        return ALRIOS_EPOCH_ERR_ROLLBACK;
    }
    state->last_timestamp = new_timestamp;
    return ALRIOS_EPOCH_OK;
}

int alrios_epoch_advance(alrios_security_state_t *state, uint64_t new_epoch, uint64_t new_timestamp) {
    if (!state) {
        return ALRIOS_EPOCH_ERR_INVALID;
    }
    // Epoch must be monotonic (strictly increasing or equal)
    if (new_epoch < state->current_epoch) {
        return ALRIOS_EPOCH_ERR_ROLLBACK;
    }
    int time_rc = alrios_epoch_verify_time(state, new_timestamp);
    if (time_rc != ALRIOS_EPOCH_OK) {
        return time_rc;
    }
    state->current_epoch = new_epoch;
    if (new_epoch > state->min_allowed_epoch) {
        state->min_allowed_epoch = new_epoch;
    }
    return ALRIOS_EPOCH_OK;
}

int alrios_epoch_verify_crypto_policy(uint64_t policy_version, uint64_t required_min_version) {
    // Old crypto policy versions below required minimum are rejected
    if (policy_version < required_min_version) {
        return ALRIOS_EPOCH_ERR_POLICY;
    }
    return ALRIOS_EPOCH_OK;
}
