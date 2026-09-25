/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */

#include "alrios/profiles.h"
#include "alrios/crypto_verify.h"
#include <string.h>
#include <ctype.h>

int alrios_profile_init(alrios_node_policy_t *p, alrios_profile_t choice) {
    if (!p) {
        return -1;
    }

    p->profile = choice;

    switch (choice) {
    case PROFILE_SOVEREIGN_MAX:
        if (!alrios_crypto_has_capability(ALRIOS_CRYPTO_CAP_ED25519 |
                                          ALRIOS_CRYPTO_CAP_AES_256_GCM |
                                          ALRIOS_CRYPTO_CAP_ML_DSA_65)) {
            memset(p, 0, sizeof(*p));
            return -2;
        }
        p->require_pqc          = 1;
        p->require_zero_disk     = 1;
        p->seccomp_strict       = 1;
        p->log_hashchain        = 1;
        p->guard_pages_enabled  = 1;
        p->net_timeout_sec      = 5;
        p->ipc_mode             = ALRIOS_IPC_ENCRYPTED_FRAME;
        p->envelope_crypto      = ALRIOS_CRYPTO_SPLIT_KEY_RAM;
        p->sig_enforcement      = ALRIOS_SIG_HYBRID_PQC;
        p->exec_mode            = ALRIOS_EXEC_MEMFD_SEALED;
        p->isolation_mode       = ALRIOS_ISOLATION_STRICT_SECCOMP_NAMESPACES;
        return 0;

    case PROFILE_ENTERPRISE_BAL:
        p->require_pqc          = 0;
        p->require_zero_disk     = 1;
        p->seccomp_strict       = 0;
        p->log_hashchain        = 0;
        p->guard_pages_enabled  = 1;
        p->net_timeout_sec      = 15;
        p->ipc_mode             = ALRIOS_IPC_UNIX_CRC32;
        p->envelope_crypto      = ALRIOS_CRYPTO_LOCAL_KEY;
        p->sig_enforcement      = ALRIOS_SIG_CLASSIC_ED25519;
        p->exec_mode            = ALRIOS_EXEC_MEMFD_BASIC;
        p->isolation_mode       = ALRIOS_ISOLATION_LIGHT_NAMESPACES_CGROUPS;
        return 0;

    case PROFILE_EDGE_PERF:
        p->require_pqc          = 0;
        p->require_zero_disk     = 0;
        p->seccomp_strict       = 0;
        p->log_hashchain        = 0;
        p->guard_pages_enabled  = 0;
        p->net_timeout_sec      = 0;
        p->ipc_mode             = ALRIOS_IPC_LOCKFREE_RING;
        p->envelope_crypto      = ALRIOS_CRYPTO_PLAINTEXT_LZ4;
        p->sig_enforcement      = ALRIOS_SIG_OPTIONAL_CRC32;
        p->exec_mode            = ALRIOS_EXEC_DIRECT_MMAP;
        p->isolation_mode       = ALRIOS_ISOLATION_NATIVE_DIRECT;
        return 0;

    default:
        return -1;
    }
}

int alrios_profile_parse(const char *name_or_flag, alrios_profile_t *out_profile) {
    if (!name_or_flag || !out_profile) {
        return -1;
    }

    /* Skip leading whitespace */
    const char *s = name_or_flag;
    while (*s != '\0' && isspace((unsigned char)*s)) {
        s++;
    }

    if (*s == '\0') {
        return -1;
    }

    /* Strip common prefix if present */
    const char *prefix_cmd = "--profile=";
    size_t prefix_cmd_len = 10;
    if (strncmp(s, prefix_cmd, prefix_cmd_len) == 0) {
        s += prefix_cmd_len;
    } else {
        const char *prefix_boot = "alrios.profile=";
        size_t prefix_boot_len = 15;
        if (strncmp(s, prefix_boot, prefix_boot_len) == 0) {
            s += prefix_boot_len;
        }
    }

    /* Sovereign matches */
    if (strcmp(s, "sovereign") == 0 ||
        strcmp(s, "SOVEREIGN") == 0 ||
        strcmp(s, "SOVEREIGN-MAX") == 0 ||
        strcmp(s, "sovereign-max") == 0 ||
        strcmp(s, "1") == 0) {
        *out_profile = PROFILE_SOVEREIGN_MAX;
        return 0;
    }

    /* Enterprise matches */
    if (strcmp(s, "enterprise") == 0 ||
        strcmp(s, "ENTERPRISE") == 0 ||
        strcmp(s, "ENTERPRISE-BALANCED") == 0 ||
        strcmp(s, "enterprise-balanced") == 0 ||
        strcmp(s, "2") == 0) {
        *out_profile = PROFILE_ENTERPRISE_BAL;
        return 0;
    }

    /* Edge / Performance matches */
    if (strcmp(s, "edge_performance") == 0 ||
        strcmp(s, "edge-performance") == 0 ||
        strcmp(s, "EDGE-PERFORMANCE") == 0 ||
        strcmp(s, "edge") == 0 ||
        strcmp(s, "EDGE") == 0 ||
        strcmp(s, "performance") == 0 ||
        strcmp(s, "PERFORMANCE") == 0 ||
        strcmp(s, "fast") == 0 ||
        strcmp(s, "FAST") == 0 ||
        strcmp(s, "3") == 0) {
        *out_profile = PROFILE_EDGE_PERF;
        return 0;
    }

    return -1;
}

const char *alrios_profile_name(alrios_profile_t profile) {
    switch (profile) {
    case PROFILE_SOVEREIGN_MAX:
        return "SOVEREIGN-MAX";
    case PROFILE_ENTERPRISE_BAL:
        return "ENTERPRISE-BALANCED";
    case PROFILE_EDGE_PERF:
        return "EDGE-PERFORMANCE";
    default:
        return "UNKNOWN";
    }
}

int alrios_profile_validate_app_compat(const alrios_node_policy_t *node, int app_flags) {
    if (!node) {
        return -1;
    }

    /* Application requires Sovereign profile, node is not Sovereign */
    if ((app_flags & ALRIOS_APP_FLAG_PROFILE_SOVEREIGN) &&
        node->profile != PROFILE_SOVEREIGN_MAX) {
        return -2;
    }

    /* Application requires PQC hybrid signature, node does not enforce PQC */
    if ((app_flags & ALRIOS_APP_FLAG_PQC_HYBRID_SIG) &&
        !node->require_pqc) {
        return -3;
    }

    /* Application requires Enterprise profile, node is Edge */
    if ((app_flags & ALRIOS_APP_FLAG_PROFILE_ENTERPRISE) &&
        node->profile == PROFILE_EDGE_PERF) {
        return -4;
    }

    return 0;
}

int alrios_profile_is_zero_disk(const alrios_node_policy_t *p) {
    if (!p) {
        return 0;
    }
    return p->require_zero_disk;
}

int alrios_profile_requires_pqc(const alrios_node_policy_t *p) {
    if (!p) {
        return 0;
    }
    return p->require_pqc;
}

int alrios_profile_has_guard_pages(const alrios_node_policy_t *p) {
    if (!p) {
        return 0;
    }
    return p->guard_pages_enabled;
}

int alrios_profile_get_timeout(const alrios_node_policy_t *p) {
    if (!p) {
        return -1;
    }
    return p->net_timeout_sec;
}
