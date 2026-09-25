/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#include "alrios/profiles.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    alrios_node_policy_t p;

    /* 1. Sovereign Profile Initializer & Matrix Invariants */
    assert(alrios_profile_init(&p, PROFILE_SOVEREIGN_MAX) == 0);
    assert(p.profile == PROFILE_SOVEREIGN_MAX);
    assert(p.require_pqc == 1);
    assert(p.require_zero_disk == 1);
    assert(p.seccomp_strict == 1);
    assert(p.log_hashchain == 1);
    assert(p.guard_pages_enabled == 1);
    assert(p.net_timeout_sec == 5);
    assert(p.ipc_mode == ALRIOS_IPC_ENCRYPTED_FRAME);
    assert(p.envelope_crypto == ALRIOS_CRYPTO_SPLIT_KEY_RAM);
    assert(p.sig_enforcement == ALRIOS_SIG_HYBRID_PQC);
    assert(p.exec_mode == ALRIOS_EXEC_MEMFD_SEALED);
    assert(p.isolation_mode == ALRIOS_ISOLATION_STRICT_SECCOMP_NAMESPACES);

    /* Sovereign helper queries */
    assert(alrios_profile_is_zero_disk(&p) == 1);
    assert(alrios_profile_requires_pqc(&p) == 1);
    assert(alrios_profile_has_guard_pages(&p) == 1);
    assert(alrios_profile_get_timeout(&p) == 5);

    /* 2. Enterprise Profile Initializer & Matrix Invariants */
    assert(alrios_profile_init(&p, PROFILE_ENTERPRISE_BAL) == 0);
    assert(p.profile == PROFILE_ENTERPRISE_BAL);
    assert(p.require_pqc == 0);
    assert(p.require_zero_disk == 1);
    assert(p.seccomp_strict == 0);
    assert(p.log_hashchain == 0);
    assert(p.guard_pages_enabled == 1);
    assert(p.net_timeout_sec == 15);
    assert(p.ipc_mode == ALRIOS_IPC_UNIX_CRC32);
    assert(p.envelope_crypto == ALRIOS_CRYPTO_LOCAL_KEY);
    assert(p.sig_enforcement == ALRIOS_SIG_CLASSIC_ED25519);
    assert(p.exec_mode == ALRIOS_EXEC_MEMFD_BASIC);
    assert(p.isolation_mode == ALRIOS_ISOLATION_LIGHT_NAMESPACES_CGROUPS);

    /* Enterprise helper queries */
    assert(alrios_profile_is_zero_disk(&p) == 1);
    assert(alrios_profile_requires_pqc(&p) == 0);
    assert(alrios_profile_has_guard_pages(&p) == 1);
    assert(alrios_profile_get_timeout(&p) == 15);

    /* 3. Edge Performance Profile Initializer & Matrix Invariants */
    assert(alrios_profile_init(&p, PROFILE_EDGE_PERF) == 0);
    assert(p.profile == PROFILE_EDGE_PERF);
    assert(p.require_pqc == 0);
    assert(p.require_zero_disk == 0);
    assert(p.seccomp_strict == 0);
    assert(p.log_hashchain == 0);
    assert(p.guard_pages_enabled == 0);
    assert(p.net_timeout_sec == 0);
    assert(p.ipc_mode == ALRIOS_IPC_LOCKFREE_RING);
    assert(p.envelope_crypto == ALRIOS_CRYPTO_PLAINTEXT_LZ4);
    assert(p.sig_enforcement == ALRIOS_SIG_OPTIONAL_CRC32);
    assert(p.exec_mode == ALRIOS_EXEC_DIRECT_MMAP);
    assert(p.isolation_mode == ALRIOS_ISOLATION_NATIVE_DIRECT);

    /* Edge helper queries */
    assert(alrios_profile_is_zero_disk(&p) == 0);
    assert(alrios_profile_requires_pqc(&p) == 0);
    assert(alrios_profile_has_guard_pages(&p) == 0);
    assert(alrios_profile_get_timeout(&p) == 0);

    /* 4. Invalid arguments to init */
    assert(alrios_profile_init(NULL, PROFILE_SOVEREIGN_MAX) == -1);
    assert(alrios_profile_init(&p, (alrios_profile_t)99) == -1);

    /* 5. App Compatibility Validation */
    /* Edge node tests */
    assert(alrios_profile_init(&p, PROFILE_EDGE_PERF) == 0);
    assert(alrios_profile_validate_app_compat(&p, 0x01) == -2); /* Sovereign flag requires Sovereign node */
    assert(alrios_profile_validate_app_compat(&p, ALRIOS_APP_FLAG_PQC_HYBRID_SIG) == -3); /* PQC flag fails */
    assert(alrios_profile_validate_app_compat(&p, ALRIOS_APP_FLAG_PROFILE_ENTERPRISE) == -4); /* Enterprise app fails on Edge */
    assert(alrios_profile_validate_app_compat(&p, ALRIOS_APP_FLAG_PROFILE_PERFORMANCE) == 0); /* Edge app passes */
    assert(alrios_profile_validate_app_compat(&p, 0) == 0); /* Generic app passes */

    /* Enterprise node tests */
    assert(alrios_profile_init(&p, PROFILE_ENTERPRISE_BAL) == 0);
    assert(alrios_profile_validate_app_compat(&p, ALRIOS_APP_FLAG_PROFILE_SOVEREIGN) == -2);
    assert(alrios_profile_validate_app_compat(&p, ALRIOS_APP_FLAG_PQC_HYBRID_SIG) == -3);
    assert(alrios_profile_validate_app_compat(&p, ALRIOS_APP_FLAG_PROFILE_ENTERPRISE) == 0);
    assert(alrios_profile_validate_app_compat(&p, 0) == 0);

    /* Sovereign node tests */
    assert(alrios_profile_init(&p, PROFILE_SOVEREIGN_MAX) == 0);
    assert(alrios_profile_validate_app_compat(&p, ALRIOS_APP_FLAG_PROFILE_SOVEREIGN) == 0);
    assert(alrios_profile_validate_app_compat(&p, ALRIOS_APP_FLAG_PQC_HYBRID_SIG) == 0);
    assert(alrios_profile_validate_app_compat(&p, 0) == 0);

    /* NULL node check */
    assert(alrios_profile_validate_app_compat(NULL, 0) == -1);

    /* 6. Profile String Parsing */
    alrios_profile_t parsed;
    assert(alrios_profile_parse("--profile=sovereign", &parsed) == 0 && parsed == PROFILE_SOVEREIGN_MAX);
    assert(alrios_profile_parse("alrios.profile=sovereign", &parsed) == 0 && parsed == PROFILE_SOVEREIGN_MAX);
    assert(alrios_profile_parse("sovereign", &parsed) == 0 && parsed == PROFILE_SOVEREIGN_MAX);
    assert(alrios_profile_parse("SOVEREIGN-MAX", &parsed) == 0 && parsed == PROFILE_SOVEREIGN_MAX);

    assert(alrios_profile_parse("--profile=enterprise", &parsed) == 0 && parsed == PROFILE_ENTERPRISE_BAL);
    assert(alrios_profile_parse("alrios.profile=enterprise", &parsed) == 0 && parsed == PROFILE_ENTERPRISE_BAL);
    assert(alrios_profile_parse("enterprise", &parsed) == 0 && parsed == PROFILE_ENTERPRISE_BAL);

    assert(alrios_profile_parse("--profile=performance", &parsed) == 0 && parsed == PROFILE_EDGE_PERF);
    assert(alrios_profile_parse("--profile=fast", &parsed) == 0 && parsed == PROFILE_EDGE_PERF);
    assert(alrios_profile_parse("alrios.profile=edge_performance", &parsed) == 0 && parsed == PROFILE_EDGE_PERF);
    assert(alrios_profile_parse("edge", &parsed) == 0 && parsed == PROFILE_EDGE_PERF);

    /* Parsing errors */
    assert(alrios_profile_parse(NULL, &parsed) == -1);
    assert(alrios_profile_parse("   ", &parsed) == -1);
    assert(alrios_profile_parse("unknown_mode", &parsed) == -1);
    assert(alrios_profile_parse("sovereign", NULL) == -1);

    /* 7. Profile Name Lookup */
    assert(strcmp(alrios_profile_name(PROFILE_SOVEREIGN_MAX), "SOVEREIGN-MAX") == 0);
    assert(strcmp(alrios_profile_name(PROFILE_ENTERPRISE_BAL), "ENTERPRISE-BALANCED") == 0);
    assert(strcmp(alrios_profile_name(PROFILE_EDGE_PERF), "EDGE-PERFORMANCE") == 0);
    assert(strcmp(alrios_profile_name((alrios_profile_t)999), "UNKNOWN") == 0);

    /* 8. Helper Null checks */
    assert(alrios_profile_is_zero_disk(NULL) == 0);
    assert(alrios_profile_requires_pqc(NULL) == 0);
    assert(alrios_profile_has_guard_pages(NULL) == 0);
    assert(alrios_profile_get_timeout(NULL) == -1);

    printf("TASK-011 (Multi-Profile Governance): PASS\n");
    return 0;
}
