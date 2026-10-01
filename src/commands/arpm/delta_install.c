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

#include "alrios/deploy_layering.h"
#include <stdio.h>
#include <stdlib.h>

int alrios_arpm_install_delta(const char *old_slot,
                             const char *new_slot,
                             const arapp_index_entry_t *entries,
                             size_t count) {
    int rc = alrios_deploy_provision_delta(old_slot, new_slot, entries, count);
    if (rc != 0) {
        alrios_deploy_rollback_slot(new_slot);
        return rc;
    }

    rc = alrios_deploy_verify_slot(new_slot, entries, count);
    if (rc != 0) {
        alrios_deploy_rollback_slot(new_slot); // Rollback leaves active slot untouched!
        return rc;
    }

    return 0;
}
