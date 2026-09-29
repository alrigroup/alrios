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
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern int supervisor_policy_validate(const alrios_node_policy_t *node, const arapp_v2_package_t *pkg, char *missing_cap_buf, size_t buf_len);

int main(void) {
    alrios_node_policy_t edge_node;
    int init_rc = alrios_profile_init(&edge_node, PROFILE_EDGE_PERF);
    assert(init_rc == 0);
    (void)init_rc;

    arapp_v2_package_t sov_pkg;
    memset(&sov_pkg, 0, sizeof(sov_pkg));
    sov_pkg.container_flags = ARAPP_V2_FLAG_PROFILE_SOVEREIGN | ARAPP_V2_FLAG_PQC_HYBRID_SIG;
    sov_pkg.format_version = ARAPP_V2_FORMAT_VERSION;

    char missing[256];
    memset(missing, 0, sizeof(missing));

    int rc = supervisor_policy_validate(&edge_node, &sov_pkg, missing, sizeof(missing));
    assert(rc != 0);
    assert(strlen(missing) > 0);
    (void)rc;
    (void)printf("[PASS] MP-010: Sovereign app rejected on lower tier with missing capability logged: %s\n", missing);

    arapp_v2_package_t ent_pkg;
    memset(&ent_pkg, 0, sizeof(ent_pkg));
    ent_pkg.container_flags = ARAPP_V2_FLAG_PROFILE_ENTERPRISE;
    ent_pkg.format_version = ARAPP_V2_FORMAT_VERSION;

    memset(missing, 0, sizeof(missing));
    rc = supervisor_policy_validate(&edge_node, &ent_pkg, missing, sizeof(missing));
    assert(rc != 0);
    assert(strstr(missing, "PROFILE_ENTERPRISE_BAL") != NULL);
    (void)printf("[PASS] MP-010: Enterprise app rejected on edge tier successfully\n");

    alrios_node_policy_t sov_node;
    init_rc = alrios_profile_init(&sov_node, PROFILE_SOVEREIGN_MAX);
    assert(init_rc == 0);

    memset(missing, 0, sizeof(missing));
    rc = supervisor_policy_validate(&sov_node, &sov_pkg, missing, sizeof(missing));
    assert(rc == 0);
    assert(strlen(missing) == 0);
    (void)printf("[PASS] MP-010: Sovereign app validated successfully on sovereign tier node\n");

    arapp_v2_package_t old_pkg;
    memset(&old_pkg, 0, sizeof(old_pkg));
    old_pkg.container_flags = 0;
    old_pkg.format_version = ARAPP_V2_FORMAT_VERSION - 1;

    memset(missing, 0, sizeof(missing));
    rc = supervisor_policy_validate(&edge_node, &old_pkg, missing, sizeof(missing));
    assert(rc != 0);
    assert(strstr(missing, "RUNTIME_VERSION_COMPATIBILITY_DOWNGRADE") != NULL);
    (void)printf("[PASS] MP-010: Runtime version downgrade rejected successfully\n");

    return 0;
}
