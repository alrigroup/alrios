/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/policy/security_epoch.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    alrios_security_state_t state;
    int init_rc = alrios_epoch_init(&state, 10U, 1000U);
    (void)init_rc;
    assert(init_rc == ALRIOS_EPOCH_OK);

    // 1. Acceptance Criterion 1: Backward time rejected
    assert(alrios_epoch_verify_time(&state, 999U) == ALRIOS_EPOCH_ERR_ROLLBACK);
    assert(alrios_epoch_verify_time(&state, 1005U) == ALRIOS_EPOCH_OK);

    // 2. Acceptance Criterion 3: Epoch monotonic
    assert(alrios_epoch_advance(&state, 9U, 1006U) == ALRIOS_EPOCH_ERR_ROLLBACK); // Decreasing epoch rejected
    assert(alrios_epoch_advance(&state, 11U, 1010U) == ALRIOS_EPOCH_OK);
    assert(state.current_epoch == 11U);

    // 3. Acceptance Criterion 2: Old crypto policy rejected
    assert(alrios_epoch_verify_crypto_policy(1U, 2U) == ALRIOS_EPOCH_ERR_POLICY);
    assert(alrios_epoch_verify_crypto_policy(2U, 2U) == ALRIOS_EPOCH_OK);
    assert(alrios_epoch_verify_crypto_policy(3U, 2U) == ALRIOS_EPOCH_OK);

    printf("TEST_SECURITY_EPOCH: PASS\n");
    return 0;
}
