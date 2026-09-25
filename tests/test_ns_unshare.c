/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#include "alrios/sandbox_defs.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(alrios_sandbox_enter_jail("slot_alpha", 1234) == ALRIOS_SANDBOX_OK);
    printf("TASK-019 (Namespace Virtualization): PASS\n");
    return 0;
}
