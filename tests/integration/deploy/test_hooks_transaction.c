/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/hooks.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

extern int alrios_armake_run_pipeline(const char *app_id, const char *slot_path);
extern int alrios_supervisor_deploy_swap_and_drain(const char *app_id, const char *slot_path);

int main(void) {
    const char *slot_dir = "/tmp/alrios_test_hook_slot";
    int command_status = system("rm -rf /tmp/alrios_test_hook_slot");
    if (command_status != 0) {
        return 1;
    }
    mkdir(slot_dir, 0700);

    char pre_swap_dir[512];
    snprintf(pre_swap_dir, sizeof(pre_swap_dir), "%s/hooks.d/pre-swap", slot_dir);
    command_status = system("mkdir -p /tmp/alrios_test_hook_slot/hooks.d/pre-swap /tmp/alrios_test_hook_slot/hooks.d/post-swap");
    if (command_status != 0) {
        return 1;
    }

    // 1. Acceptance Criterion 1: pre-swap failure aborts
    FILE *f_bad = fopen("/tmp/alrios_test_hook_slot/hooks.d/pre-swap/01_fail.sh", "w");
    assert(f_bad != NULL);
    fprintf(f_bad, "#!/bin/sh\nexit 1\n");
    fclose(f_bad);
    chmod("/tmp/alrios_test_hook_slot/hooks.d/pre-swap/01_fail.sh", 0755);

    int rc = alrios_supervisor_deploy_swap_and_drain("test.app", slot_dir);
    (void)rc;
    assert(rc != 0); // Pre-swap hook failure aborted deployment!

    // Remove failing hook and add a succeeding one
    unlink("/tmp/alrios_test_hook_slot/hooks.d/pre-swap/01_fail.sh");
    FILE *f_good = fopen("/tmp/alrios_test_hook_slot/hooks.d/pre-swap/01_ok.sh", "w");
    assert(f_good != NULL);
    fprintf(f_good, "#!/bin/sh\nexit 0\n");
    fclose(f_good);
    chmod("/tmp/alrios_test_hook_slot/hooks.d/pre-swap/01_ok.sh", 0755);

    // 2. Acceptance Criterion 2: post-swap timeout is isolated
    FILE *f_slow = fopen("/tmp/alrios_test_hook_slot/hooks.d/post-swap/02_slow.sh", "w");
    assert(f_slow != NULL);
    fprintf(f_slow, "#!/bin/sh\nsleep 2\nexit 0\n");
    fclose(f_slow);
    chmod("/tmp/alrios_test_hook_slot/hooks.d/post-swap/02_slow.sh", 0755);

    // Swap should succeed because post-swap timeouts are non-blocking / isolated
    rc = alrios_supervisor_deploy_swap_and_drain("test.app", slot_dir);
    assert(rc == 0);

    // 3. armake build pipeline execution
    rc = alrios_armake_run_pipeline("test.app", slot_dir);
    assert(rc == 0);

    command_status = system("rm -rf /tmp/alrios_test_hook_slot");
    if (command_status != 0) {
        return 1;
    }
    printf("TEST_HOOKS_TRANSACTION: PASS\n");
    return 0;
}
