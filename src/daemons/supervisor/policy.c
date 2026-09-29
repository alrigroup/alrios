/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/profiles.h"
#include "alrios/package/arapp_v2.h"
#include <string.h>
#include <stdio.h>

int supervisor_policy_validate(const alrios_node_policy_t *node, const arapp_v2_package_t *pkg, char *missing_cap_buf, size_t buf_len) {
    if (!node || !pkg) {
        return -1;
    }
    if (missing_cap_buf && buf_len > 0) {
        missing_cap_buf[0] = '\0';
    }
    if ((pkg->container_flags & ARAPP_V2_FLAG_PROFILE_SOVEREIGN) && node->profile != PROFILE_SOVEREIGN_MAX) {
        if (missing_cap_buf && buf_len > 0) {
            int ret = snprintf(missing_cap_buf, buf_len, "%s", "PROFILE_SOVEREIGN_MAX");
            if (ret < 0 || (size_t)ret >= buf_len) {
                return -1;
            }
        }
        (void)fprintf(stderr, "SECURITY FAIL: Sovereign app rejected on lower tier node (missing capability: PROFILE_SOVEREIGN_MAX)\n");
        return -2;
    }
    if ((pkg->container_flags & ARAPP_V2_FLAG_PQC_HYBRID_SIG) && !node->require_pqc) {
        if (missing_cap_buf && buf_len > 0) {
            int ret = snprintf(missing_cap_buf, buf_len, "%s", "PQC_HYBRID_SIGNATURE");
            if (ret < 0 || (size_t)ret >= buf_len) {
                return -1;
            }
        }
        (void)fprintf(stderr, "SECURITY FAIL: PQC hybrid signature app rejected (missing capability: PQC_HYBRID_SIGNATURE)\n");
        return -3;
    }
    if ((pkg->container_flags & ARAPP_V2_FLAG_PROFILE_ENTERPRISE) && node->profile == PROFILE_EDGE_PERF) {
        if (missing_cap_buf && buf_len > 0) {
            int ret = snprintf(missing_cap_buf, buf_len, "%s", "PROFILE_ENTERPRISE_BAL");
            if (ret < 0 || (size_t)ret >= buf_len) {
                return -1;
            }
        }
        (void)fprintf(stderr, "SECURITY FAIL: Enterprise app rejected on edge tier (missing capability: PROFILE_ENTERPRISE_BAL)\n");
        return -4;
    }
    if (pkg->format_version < ARAPP_V2_FORMAT_VERSION) {
        if (missing_cap_buf && buf_len > 0) {
            int ret = snprintf(missing_cap_buf, buf_len, "%s", "RUNTIME_VERSION_COMPATIBILITY_DOWNGRADE");
            if (ret < 0 || (size_t)ret >= buf_len) {
                return -1;
            }
        }
        (void)fprintf(stderr, "SECURITY FAIL: Runtime version downgrade detected\n");
        return -5;
    }
    return 0;
}
