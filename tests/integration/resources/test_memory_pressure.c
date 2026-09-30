/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/resources/psi.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    // 1. Acceptance Criterion 1: 80 percent warning
    assert(alrios_psi_evaluate_pressure(79.9) == ALRIOS_PRESSURE_NORMAL);
    assert(alrios_psi_evaluate_pressure(80.0) == ALRIOS_PRESSURE_WARN);
    assert(alrios_psi_evaluate_pressure(85.5) == ALRIOS_PRESSURE_WARN);

    // 2. Acceptance Criterion 2: 90 percent selective shedding
    assert(alrios_psi_evaluate_pressure(90.0) == ALRIOS_PRESSURE_SHED);
    assert(alrios_psi_evaluate_pressure(95.0) == ALRIOS_PRESSURE_SHED);

    // 3. Acceptance Criterion 3: Core daemons never selected for sacrifice
    alrios_target_process_t targets[3] = {
        { .pid = 1001, .tier = ALRIOS_TIER_CORE, .is_sacrificed = 0 },
        { .pid = 1002, .tier = ALRIOS_TIER_WORKER, .is_sacrificed = 0 },
        { .pid = 1003, .tier = ALRIOS_TIER_GUEST, .is_sacrificed = 0 }
    };

    size_t shed_count = 0;
    // At warning level: no sacrifice
    int rc = alrios_psi_handle_notification(ALRIOS_PRESSURE_WARN, targets, 3, &shed_count);
    (void)rc;
    assert(rc == 0);
    assert(shed_count == 0);
    assert(targets[0].is_sacrificed == 0);
    assert(targets[1].is_sacrificed == 0);
    assert(targets[2].is_sacrificed == 0);

    // At shed level: only guest is sacrificed, CORE daemon remains 100% immune
    rc = alrios_psi_handle_notification(ALRIOS_PRESSURE_SHED, targets, 3, &shed_count);
    assert(rc == 0);
    assert(shed_count == 1);
    assert(targets[0].is_sacrificed == 0); // CORE daemon NEVER sacrificed!
    assert(targets[1].is_sacrificed == 0); // WORKER daemon protected
    assert(targets[2].is_sacrificed == 1); // Guest sacrificed

    printf("TEST_MEMORY_PRESSURE: PASS\n");
    return 0;
}
