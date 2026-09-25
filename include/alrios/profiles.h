/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#ifndef ALRIOS_PROFILES_H
#define ALRIOS_PROFILES_H

#include <stdint.h>
#include <stddef.h>

/* Execution profile identifier */
typedef enum alrios_profile {
    PROFILE_SOVEREIGN_MAX   = 1,
    PROFILE_ENTERPRISE_BAL  = 2,
    PROFILE_EDGE_PERF       = 3
} alrios_profile_t;

/* IPC communication mechanism */
typedef enum alrios_ipc_mode {
    ALRIOS_IPC_ENCRYPTED_FRAME = 1, /* Encrypted envelopes + frame validation */
    ALRIOS_IPC_UNIX_CRC32      = 2, /* UNIX domain sockets with CRC32 */
    ALRIOS_IPC_LOCKFREE_RING   = 3  /* Shared memory lock-free ring buffer */
} alrios_ipc_mode_t;

/* Envelope cryptographic packaging */
typedef enum alrios_envelope_crypto {
    ALRIOS_CRYPTO_SPLIT_KEY_RAM = 1, /* AES-256-GCM + split-key in-RAM */
    ALRIOS_CRYPTO_LOCAL_KEY     = 2, /* AES-256-GCM with local node key */
    ALRIOS_CRYPTO_PLAINTEXT_LZ4 = 3  /* Fast-pack plain binary / LZ4 */
} alrios_envelope_crypto_t;

/* Signature enforcement requirements */
typedef enum alrios_sig_enforcement {
    ALRIOS_SIG_HYBRID_PQC      = 1, /* Ed25519 + ML-DSA-65 (FIPS 204) mandatory */
    ALRIOS_SIG_CLASSIC_ED25519 = 2, /* Ed25519 (RFC 8032) only */
    ALRIOS_SIG_OPTIONAL_CRC32  = 3  /* CRC32 / SHA-256 checksum optional */
} alrios_sig_enforcement_t;

/* Binary execution storage policy */
typedef enum alrios_exec_mode {
    ALRIOS_EXEC_MEMFD_SEALED   = 1, /* Zero-disk pure: memfd_create + F_ADD_SEALS */
    ALRIOS_EXEC_MEMFD_BASIC    = 2, /* Zero-disk pure: memfd_create */
    ALRIOS_EXEC_DIRECT_MMAP    = 3  /* Direct disk execution: mmap / Direct I/O */
} alrios_exec_mode_t;

/* Process isolation & sandboxing tier */
typedef enum alrios_isolation_mode {
    ALRIOS_ISOLATION_STRICT_SECCOMP_NAMESPACES = 1, /* Namespaces (PID/NET/NS) + strict seccomp-BPF */
    ALRIOS_ISOLATION_LIGHT_NAMESPACES_CGROUPS   = 2, /* Light namespaces + Cgroups v2 */
    ALRIOS_ISOLATION_NATIVE_DIRECT             = 3  /* Native direct C11 process (no sandbox overhead) */
} alrios_isolation_mode_t;

/* Node governance policy matrix */
typedef struct alrios_node_policy {
    alrios_profile_t          profile;
    int                       require_pqc;
    int                       require_zero_disk;
    int                       seccomp_strict;
    int                       log_hashchain;
    int                       guard_pages_enabled;
    int                       net_timeout_sec;
    alrios_ipc_mode_t         ipc_mode;
    alrios_envelope_crypto_t  envelope_crypto;
    alrios_sig_enforcement_t  sig_enforcement;
    alrios_exec_mode_t        exec_mode;
    alrios_isolation_mode_t   isolation_mode;
} alrios_node_policy_t;

/* App compatibility flags matching ALRIOS Specifications */
#define ALRIOS_APP_FLAG_PROFILE_SOVEREIGN    (1U << 0)
#define ALRIOS_APP_FLAG_PROFILE_ENTERPRISE   (1U << 1)
#define ALRIOS_APP_FLAG_PROFILE_PERFORMANCE  (1U << 2)
#define ALRIOS_APP_FLAG_RING_SOVEREIGN       (1U << 3)
#define ALRIOS_APP_FLAG_RING_DEVMODE         (1U << 4)
#define ALRIOS_APP_FLAG_ENCRYPTED_GCM        (1U << 5)
#define ALRIOS_APP_FLAG_PQC_HYBRID_SIG       (1U << 6)
#define ALRIOS_APP_FLAG_STRIPPED_SYMBOLS     (1U << 7)

#define ALRIOS_PROFILE_ERR_INVALID     -1
#define ALRIOS_PROFILE_ERR_UNAVAILABLE -2

/* Core Profile API */
int alrios_profile_init(alrios_node_policy_t *p, alrios_profile_t choice);
int alrios_profile_parse(const char *name_or_flag, alrios_profile_t *out_profile);
const char *alrios_profile_name(alrios_profile_t profile);
int alrios_profile_validate_app_compat(const alrios_node_policy_t *node, int app_flags);

/* Inspection helpers */
int alrios_profile_is_zero_disk(const alrios_node_policy_t *p);
int alrios_profile_requires_pqc(const alrios_node_policy_t *p);
int alrios_profile_has_guard_pages(const alrios_node_policy_t *p);
int alrios_profile_get_timeout(const alrios_node_policy_t *p);

#endif /* ALRIOS_PROFILES_H */
