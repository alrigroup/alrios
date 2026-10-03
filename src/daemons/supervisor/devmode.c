/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/package/arapp_v2.h"
#include <string.h>
#include <stdlib.h>

int supervisor_devmode_validate(const arapp_v2_package_t *pkg, int is_signed, int devmode_enabled) {
    if (!pkg) {
        return -1;
    }

    int is_sovereign = 0;
    if (pkg->manifest_json && strstr(pkg->manifest_json, "\"execution_ring\":\"sovereign_trust\"")) {
        is_sovereign = 1;
    }

    if (is_sovereign && !is_signed) {
        return -101;
    }

    if (!devmode_enabled) {
        if (!is_signed || (pkg->manifest_json && strstr(pkg->manifest_json, "\"execution_ring\":\"devmode_sandbox\""))) {
            return -102;
        }
    }

    return 0;
}
