/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/supervisor/context.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

extern int alrios_supervisor_loader_reload(alrios_supervisor_ctx_t *ctx, const char *app_id);

int main(void) {
    alrios_supervisor_ctx_t *ctx1 = NULL;
    alrios_supervisor_ctx_t *ctx2 = NULL;

    // 1. Acceptance Criterion 1: Create/destroy lifecycle and no mutable file-scope globals
    assert(alrios_supervisor_ctx_create(10U, &ctx1) == ALRIOS_SUPERVISOR_OK);
    assert(alrios_supervisor_ctx_create(10U, &ctx2) == ALRIOS_SUPERVISOR_OK);

    // 2. Acceptance Criterion 3: Multiple supervisor contexts testable concurrently and independently
    assert(alrios_supervisor_register_app(ctx1, "app.one", "/opt/1.arapp") == ALRIOS_SUPERVISOR_OK);
    assert(alrios_supervisor_register_app(ctx2, "app.two", "/opt/2.arapp") == ALRIOS_SUPERVISOR_OK);

    assert(alrios_supervisor_get_app_count(ctx1) == 1U);
    assert(alrios_supervisor_get_app_count(ctx2) == 1U);

    assert(alrios_supervisor_loader_reload(ctx1, "app.one") == ALRIOS_SUPERVISOR_OK);
    assert(alrios_supervisor_loader_reload(ctx2, "app.two") == ALRIOS_SUPERVISOR_OK);

    assert(alrios_supervisor_unregister_app(ctx1, "app.one") == ALRIOS_SUPERVISOR_OK);
    assert(alrios_supervisor_get_app_count(ctx1) == 0U);
    assert(alrios_supervisor_get_app_count(ctx2) == 1U);

    alrios_supervisor_ctx_destroy(ctx1);
    alrios_supervisor_ctx_destroy(ctx2);

    printf("TEST_SUPERVISOR_CONTEXT: PASS\n");
    return 0;
}
