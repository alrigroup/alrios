/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_POLICY_SECURITY_EPOCH_H
#define ALRIOS_POLICY_SECURITY_EPOCH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Error codes */
#define ALRIOS_EPOCH_OK                          0
#define ALRIOS_EPOCH_ERR_INVALID_ARGUMENT    -3601
#define ALRIOS_EPOCH_ERR_ROLLBACK            -3602
#define ALRIOS_EPOCH_ERR_INTEGRITY           -3603
#define ALRIOS_EPOCH_ERR_IO                  -3604
#define ALRIOS_EPOCH_ERR_TIME_BACKWARD       -3605
#define ALRIOS_EPOCH_ERR_POLICY_OUTDATED     -3606
#define ALRIOS_EPOCH_ERR_FORMAT              -3607
#define ALRIOS_EPOCH_ERR_EXPIRED             -3608
#define ALRIOS_EPOCH_ERR_OVERFLOW            -3609

/* Size and format constants */
#define ALRIOS_EPOCH_KEY_SIZE               32U
#define ALRIOS_EPOCH_HASH_SIZE              64U
#define ALRIOS_EPOCH_MAGIC_SIZE              8U
#define ALRIOS_EPOCH_BODY_SIZE              64U
#define ALRIOS_EPOCH_WIRE_SIZE             128U
#define ALRIOS_EPOCH_VERSION_1               1U

/* Transition epoch levels */
#define ALRIOS_EPOCH_CRYPTO_LEGACY_ED25519   1U
#define ALRIOS_EPOCH_CRYPTO_HYBRID_PQC       2U
#define ALRIOS_EPOCH_CRYONT_PQC_STRICT       3U
#define ALRIOS_EPOCH_CRYPTO_PQC_STRICT       3U

/* Cryptographic algorithm identifiers */
typedef enum alrios_crypto_algo {
    ALRIOS_ALGO_NONE                    = 0,
    ALRIOS_ALGO_ED25519_CLASSIC         = 1,
    ALRIOS_ALGO_ML_DSA_65               = 2,
    ALRIOS_ALGO_HYBRID_ED25519_ML_DSA   = 3,
    ALRIOS_ALGO_ML_KEM_768              = 4,
    ALRIOS_ALGO_AES_256_GCM             = 5
} alrios_crypto_algo_t;

/* Security Epoch State */
typedef struct alrios_epoch_state {
    uint16_t version;
    uint16_t flags;
    uint64_t epoch;
    uint64_t updated_at_ns;
    uint64_t monotonic_anchor_ns;
    uint8_t  integrity_tag[ALRIOS_EPOCH_HASH_SIZE];
} alrios_epoch_state_t;

/* Time Monitor */
typedef struct alrios_time_monitor {
    uint64_t last_realtime_ns;
    uint64_t last_monotonic_ns;
    uint64_t backward_step_threshold_ns;
    uint64_t violations_count;
    uint64_t last_violation_delta_ns;
    int      tamper_detected;
} alrios_time_monitor_t;

/* Crypto Policy */
typedef struct alrios_crypto_policy {
    uint64_t min_epoch;
    uint64_t policy_epoch;
    uint32_t allowed_algorithms_mask;
    uint32_t required_capabilities;
    int      require_pqc;
    uint64_t valid_from_ns;
    uint64_t valid_until_ns;
} alrios_crypto_policy_t;

/* Security Epoch State Management */
int alrios_epoch_state_init(
    alrios_epoch_state_t *state,
    uint64_t initial_epoch,
    const uint8_t key[ALRIOS_EPOCH_KEY_SIZE]
);

int alrios_epoch_state_advance(
    alrios_epoch_state_t *state,
    uint64_t next_epoch,
    const uint8_t key[ALRIOS_EPOCH_KEY_SIZE]
);

int alrios_epoch_state_verify_integrity(
    const alrios_epoch_state_t *state,
    const uint8_t key[ALRIOS_EPOCH_KEY_SIZE]
);

int alrios_epoch_state_encode(
    const alrios_epoch_state_t *state,
    const uint8_t key[ALRIOS_EPOCH_KEY_SIZE],
    uint8_t out_wire[ALRIOS_EPOCH_WIRE_SIZE]
);

int alrios_epoch_state_decode(
    const uint8_t in_wire[ALRIOS_EPOCH_WIRE_SIZE],
    const uint8_t key[ALRIOS_EPOCH_KEY_SIZE],
    alrios_epoch_state_t *out_state
);

int alrios_epoch_state_read_fd(
    int fd,
    const uint8_t key[ALRIOS_EPOCH_KEY_SIZE],
    alrios_epoch_state_t *out_state
);

int alrios_epoch_state_write_fd(
    int fd,
    const uint8_t key[ALRIOS_EPOCH_KEY_SIZE],
    const alrios_epoch_state_t *state
);

int alrios_epoch_state_load_file(
    const char *path,
    const uint8_t key[ALRIOS_EPOCH_KEY_SIZE],
    alrios_epoch_state_t *out_state
);

int alrios_epoch_state_save_file(
    const char *path,
    const uint8_t key[ALRIOS_EPOCH_KEY_SIZE],
    const alrios_epoch_state_t *state
);

int alrios_epoch_file_advance(
    const char *path,
    uint64_t next_epoch,
    const uint8_t key[ALRIOS_EPOCH_KEY_SIZE],
    alrios_epoch_state_t *out_state
);

/* Time Monitor API */
int alrios_time_monitor_init(
    alrios_time_monitor_t *mon,
    uint64_t backward_step_threshold_ns
);

int alrios_time_monitor_update(
    alrios_time_monitor_t *mon,
    uint64_t now_realtime_ns,
    uint64_t now_monotonic_ns
);

int alrios_time_monitor_sample(
    alrios_time_monitor_t *mon
);

int alrios_time_monitor_verify_against_anchor(
    const alrios_time_monitor_t *mon,
    const alrios_epoch_state_t *anchor
);

/* Crypto Policy Gating API */
int alrios_security_epoch_gate_policy(
    const alrios_epoch_state_t *state,
    const alrios_crypto_policy_t *policy
);

int alrios_security_epoch_gate_algorithm(
    const alrios_epoch_state_t *state,
    alrios_crypto_algo_t algo
);

const char *alrios_crypto_algo_name(
    alrios_crypto_algo_t algo
);

const char *alrios_epoch_strerror(
    int err
);

#ifdef __cplusplus
}
#endif

#endif /* ALRIOS_POLICY_SECURITY_EPOCH_H */
