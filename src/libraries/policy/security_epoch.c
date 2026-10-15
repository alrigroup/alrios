/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#define _POSIX_C_SOURCE 200809L

#include "alrios/policy/security_epoch.h"
#include "alrios/crypto_verify.h"
#include <alrios/platform/fs_compat.h>

#include <errno.h>
#include <fcntl.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/params.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/file.h>
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

static const uint8_t g_epoch_magic[ALRIOS_EPOCH_MAGIC_SIZE] = {
    'A', 'L', 'R', 'I', 'E', 'P', 'O', 'C'
};

static const uint8_t g_epoch_domain[] = "ALRIOS-SECURITY-EPOCH-V1";

static void store_u16_be(uint8_t *output, uint16_t value) {
    output[0] = (uint8_t)(value >> 8U);
    output[1] = (uint8_t)value;
}

static void store_u64_be(uint8_t *output, uint64_t value) {
    size_t i;
    for (i = 8U; i > 0U; --i) {
        output[i - 1U] = (uint8_t)value;
        value >>= 8U;
    }
}

static uint16_t load_u16_be(const uint8_t *input) {
    return (uint16_t)(((uint16_t)input[0] << 8U) | (uint16_t)input[1]);
}

static uint64_t load_u64_be(const uint8_t *input) {
    uint64_t value = 0U;
    size_t i;
    for (i = 0U; i < 8U; ++i) {
        value = (value << 8U) | (uint64_t)input[i];
    }
    return value;
}

static int get_current_clocks(uint64_t *out_realtime_ns, uint64_t *out_monotonic_ns) {
    struct timespec ts_real;
    struct timespec ts_mono;

    if (clock_gettime(CLOCK_REALTIME, &ts_real) != 0) {
        return ALRIOS_EPOCH_ERR_IO;
    }
    if (clock_gettime(CLOCK_MONOTONIC, &ts_mono) != 0) {
        return ALRIOS_EPOCH_ERR_IO;
    }

    if (out_realtime_ns) {
        *out_realtime_ns = (uint64_t)ts_real.tv_sec * 1000000000ULL + (uint64_t)ts_real.tv_nsec;
    }
    if (out_monotonic_ns) {
        *out_monotonic_ns = (uint64_t)ts_mono.tv_sec * 1000000000ULL + (uint64_t)ts_mono.tv_nsec;
    }
    return ALRIOS_EPOCH_OK;
}

static int compute_integrity_tag(
    const uint8_t *body,
    size_t body_len,
    const uint8_t key[ALRIOS_EPOCH_KEY_SIZE],
    uint8_t out_tag[ALRIOS_EPOCH_HASH_SIZE]
) {
    EVP_MAC *mac = NULL;
    EVP_MAC_CTX *ctx = NULL;
    OSSL_PARAM params[2];
    size_t mac_len = 0U;
    int status = ALRIOS_EPOCH_ERR_INTEGRITY;

    if (!body || body_len == 0U || !out_tag) {
        return ALRIOS_EPOCH_ERR_INVALID_ARGUMENT;
    }

    if (key != NULL) {
        mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
        if (!mac) {
            return ALRIOS_EPOCH_ERR_INTEGRITY;
        }
        ctx = EVP_MAC_CTX_new(mac);
        if (!ctx) {
            EVP_MAC_free(mac);
            return ALRIOS_EPOCH_ERR_INTEGRITY;
        }

        params[0] = OSSL_PARAM_construct_utf8_string("digest", "SHA512", 0U);
        params[1] = OSSL_PARAM_construct_end();

        if (EVP_MAC_init(ctx, key, ALRIOS_EPOCH_KEY_SIZE, params) == 1 &&
            EVP_MAC_update(ctx, g_epoch_domain, sizeof(g_epoch_domain) - 1U) == 1 &&
            EVP_MAC_update(ctx, body, body_len) == 1 &&
            EVP_MAC_final(ctx, out_tag, &mac_len, ALRIOS_EPOCH_HASH_SIZE) == 1 &&
            mac_len == ALRIOS_EPOCH_HASH_SIZE) {
            status = ALRIOS_EPOCH_OK;
        }

        EVP_MAC_CTX_free(ctx);
        EVP_MAC_free(mac);
    } else {
        /* Domain-separated SHA-512 when no secret key is provided */
        alrios_sha512_ctx_t sha_ctx;
        if (alrios_sha512_init(&sha_ctx) == ALRIOS_CRYPTO_OK &&
            alrios_sha512_update(&sha_ctx, g_epoch_domain, sizeof(g_epoch_domain) - 1U) == ALRIOS_CRYPTO_OK &&
            alrios_sha512_update(&sha_ctx, body, body_len) == ALRIOS_CRYPTO_OK &&
            alrios_sha512_final(&sha_ctx, out_tag) == ALRIOS_CRYPTO_OK) {
            status = ALRIOS_EPOCH_OK;
        }
    }

    if (status != ALRIOS_EPOCH_OK) {
        alrios_explicit_zeroize(out_tag, ALRIOS_EPOCH_HASH_SIZE);
    }
    return status;
}

static int build_body(const alrios_epoch_state_t *state, uint8_t body[ALRIOS_EPOCH_BODY_SIZE]) {
    if (!state || !body) {
        return ALRIOS_EPOCH_ERR_INVALID_ARGUMENT;
    }
    (void)memset(body, 0, ALRIOS_EPOCH_BODY_SIZE);
    (void)memcpy(body, g_epoch_magic, ALRIOS_EPOCH_MAGIC_SIZE);
    store_u16_be(body + 8U, state->version);
    store_u16_be(body + 10U, state->flags);
    store_u64_be(body + 12U, state->epoch);
    store_u64_be(body + 20U, state->updated_at_ns);
    store_u64_be(body + 28U, state->monotonic_anchor_ns);
    /* Remaining 28 bytes [36..63] are 0 reserved padding */
    return ALRIOS_EPOCH_OK;
}

int alrios_epoch_state_init(
    alrios_epoch_state_t *state,
    uint64_t initial_epoch,
    const uint8_t key[ALRIOS_EPOCH_KEY_SIZE]
) {
    uint8_t body[ALRIOS_EPOCH_BODY_SIZE];
    uint64_t rt_ns = 0U;
    uint64_t mono_ns = 0U;
    int status;

    if (!state) {
        return ALRIOS_EPOCH_ERR_INVALID_ARGUMENT;
    }

    (void)memset(state, 0, sizeof(*state));
    state->version = ALRIOS_EPOCH_VERSION_1;
    state->flags = 0U;
    state->epoch = initial_epoch;

    status = get_current_clocks(&rt_ns, &mono_ns);
    if (status != ALRIOS_EPOCH_OK) {
        return status;
    }
    state->updated_at_ns = rt_ns;
    state->monotonic_anchor_ns = mono_ns;

    status = build_body(state, body);
    if (status != ALRIOS_EPOCH_OK) {
        return status;
    }

    status = compute_integrity_tag(body, sizeof(body), key, state->integrity_tag);
    alrios_explicit_zeroize(body, sizeof(body));
    return status;
}

int alrios_epoch_state_advance(
    alrios_epoch_state_t *state,
    uint64_t next_epoch,
    const uint8_t key[ALRIOS_EPOCH_KEY_SIZE]
) {
    uint8_t body[ALRIOS_EPOCH_BODY_SIZE];
    uint64_t rt_ns = 0U;
    uint64_t mono_ns = 0U;
    int status;

    if (!state) {
        return ALRIOS_EPOCH_ERR_INVALID_ARGUMENT;
    }

    /* Verify current integrity before advancing */
    status = alrios_epoch_state_verify_integrity(state, key);
    if (status != ALRIOS_EPOCH_OK) {
        return status;
    }

    /* Monotonic epoch check: strictly non-decreasing */
    if (next_epoch < state->epoch) {
        return ALRIOS_EPOCH_ERR_ROLLBACK;
    }

    status = get_current_clocks(&rt_ns, &mono_ns);
    if (status != ALRIOS_EPOCH_OK) {
        return status;
    }

    /* Reject backward system time update against previous state timestamp */
    if (state->updated_at_ns > 0U && rt_ns < state->updated_at_ns) {
        return ALRIOS_EPOCH_ERR_TIME_BACKWARD;
    }

    state->epoch = next_epoch;
    state->updated_at_ns = rt_ns;
    state->monotonic_anchor_ns = mono_ns;

    status = build_body(state, body);
    if (status != ALRIOS_EPOCH_OK) {
        return status;
    }

    status = compute_integrity_tag(body, sizeof(body), key, state->integrity_tag);
    alrios_explicit_zeroize(body, sizeof(body));
    return status;
}

int alrios_epoch_state_verify_integrity(
    const alrios_epoch_state_t *state,
    const uint8_t key[ALRIOS_EPOCH_KEY_SIZE]
) {
    uint8_t body[ALRIOS_EPOCH_BODY_SIZE];
    uint8_t expected_tag[ALRIOS_EPOCH_HASH_SIZE];
    int status;

    if (!state) {
        return ALRIOS_EPOCH_ERR_INVALID_ARGUMENT;
    }

    if (state->version != ALRIOS_EPOCH_VERSION_1) {
        return ALRIOS_EPOCH_ERR_FORMAT;
    }

    status = build_body(state, body);
    if (status != ALRIOS_EPOCH_OK) {
        return status;
    }

    status = compute_integrity_tag(body, sizeof(body), key, expected_tag);
    alrios_explicit_zeroize(body, sizeof(body));
    if (status != ALRIOS_EPOCH_OK) {
        return status;
    }

    if (CRYPTO_memcmp(state->integrity_tag, expected_tag, ALRIOS_EPOCH_HASH_SIZE) != 0) {
        alrios_explicit_zeroize(expected_tag, sizeof(expected_tag));
        return ALRIOS_EPOCH_ERR_INTEGRITY;
    }

    alrios_explicit_zeroize(expected_tag, sizeof(expected_tag));
    return ALRIOS_EPOCH_OK;
}

int alrios_epoch_state_encode(
    const alrios_epoch_state_t *state,
    const uint8_t key[ALRIOS_EPOCH_KEY_SIZE],
    uint8_t out_wire[ALRIOS_EPOCH_WIRE_SIZE]
) {
    int status;

    if (!state || !out_wire) {
        return ALRIOS_EPOCH_ERR_INVALID_ARGUMENT;
    }

    status = alrios_epoch_state_verify_integrity(state, key);
    if (status != ALRIOS_EPOCH_OK) {
        return status;
    }

    status = build_body(state, out_wire);
    if (status != ALRIOS_EPOCH_OK) {
        return status;
    }

    (void)memcpy(out_wire + ALRIOS_EPOCH_BODY_SIZE, state->integrity_tag, ALRIOS_EPOCH_HASH_SIZE);
    return ALRIOS_EPOCH_OK;
}

int alrios_epoch_state_decode(
    const uint8_t in_wire[ALRIOS_EPOCH_WIRE_SIZE],
    const uint8_t key[ALRIOS_EPOCH_KEY_SIZE],
    alrios_epoch_state_t *out_state
) {
    alrios_epoch_state_t state;
    int status;

    if (!in_wire || !out_state) {
        return ALRIOS_EPOCH_ERR_INVALID_ARGUMENT;
    }

    if (CRYPTO_memcmp(in_wire, g_epoch_magic, ALRIOS_EPOCH_MAGIC_SIZE) != 0) {
        return ALRIOS_EPOCH_ERR_FORMAT;
    }

    (void)memset(&state, 0, sizeof(state));
    state.version = load_u16_be(in_wire + 8U);
    state.flags = load_u16_be(in_wire + 10U);
    state.epoch = load_u64_be(in_wire + 12U);
    state.updated_at_ns = load_u64_be(in_wire + 20U);
    state.monotonic_anchor_ns = load_u64_be(in_wire + 28U);
    (void)memcpy(state.integrity_tag, in_wire + ALRIOS_EPOCH_BODY_SIZE, ALRIOS_EPOCH_HASH_SIZE);

    status = alrios_epoch_state_verify_integrity(&state, key);
    if (status != ALRIOS_EPOCH_OK) {
        return status;
    }

    *out_state = state;
    return ALRIOS_EPOCH_OK;
}

static int read_exact(int fd, uint8_t *buf, size_t len) {
    size_t total = 0U;

    while (total < len) {
        ssize_t n = read(fd, buf + total, len - total);
        if (n > 0) {
            total += (size_t)n;
        } else if (n == 0) {
            return ALRIOS_EPOCH_ERR_IO;
        } else if (errno != EINTR) {
            return ALRIOS_EPOCH_ERR_IO;
        }
    }
    return ALRIOS_EPOCH_OK;
}

static int write_exact(int fd, const uint8_t *buf, size_t len) {
    size_t total = 0U;

    while (total < len) {
        ssize_t n = write(fd, buf + total, len - total);
        if (n > 0) {
            total += (size_t)n;
        } else if (errno != EINTR) {
            return ALRIOS_EPOCH_ERR_IO;
        }
    }
    return ALRIOS_EPOCH_OK;
}

int alrios_epoch_state_read_fd(
    int fd,
    const uint8_t key[ALRIOS_EPOCH_KEY_SIZE],
    alrios_epoch_state_t *out_state
) {
    uint8_t wire[ALRIOS_EPOCH_WIRE_SIZE];
    int status;

    if (fd < 0 || !out_state) {
        return ALRIOS_EPOCH_ERR_INVALID_ARGUMENT;
    }

    if (lseek(fd, 0, SEEK_SET) == (off_t)-1) {
        return ALRIOS_EPOCH_ERR_IO;
    }

    status = read_exact(fd, wire, sizeof(wire));
    if (status != ALRIOS_EPOCH_OK) {
        return status;
    }

    status = alrios_epoch_state_decode(wire, key, out_state);
    alrios_explicit_zeroize(wire, sizeof(wire));
    return status;
}

int alrios_epoch_state_write_fd(
    int fd,
    const uint8_t key[ALRIOS_EPOCH_KEY_SIZE],
    const alrios_epoch_state_t *state
) {
    uint8_t wire[ALRIOS_EPOCH_WIRE_SIZE];
    int status;

    if (fd < 0 || !state) {
        return ALRIOS_EPOCH_ERR_INVALID_ARGUMENT;
    }

    status = alrios_epoch_state_encode(state, key, wire);
    if (status != ALRIOS_EPOCH_OK) {
        return status;
    }

    if (lseek(fd, 0, SEEK_SET) == (off_t)-1) {
        alrios_explicit_zeroize(wire, sizeof(wire));
        return ALRIOS_EPOCH_ERR_IO;
    }

    status = write_exact(fd, wire, sizeof(wire));
    alrios_explicit_zeroize(wire, sizeof(wire));
    if (status != ALRIOS_EPOCH_OK) {
        return status;
    }

    if (ftruncate(fd, (off_t)ALRIOS_EPOCH_WIRE_SIZE) != 0) {
        return ALRIOS_EPOCH_ERR_IO;
    }

    if (fsync(fd) != 0) {
        return ALRIOS_EPOCH_ERR_IO;
    }

    return ALRIOS_EPOCH_OK;
}

int alrios_epoch_state_load_file(
    const char *path,
    const uint8_t key[ALRIOS_EPOCH_KEY_SIZE],
    alrios_epoch_state_t *out_state
) {
    int fd;
    int status;

    if (!path || !out_state) {
        return ALRIOS_EPOCH_ERR_INVALID_ARGUMENT;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        return ALRIOS_EPOCH_ERR_IO;
    }

    status = alrios_epoch_state_read_fd(fd, key, out_state);
    (void)close(fd);
    return status;
}

int alrios_epoch_state_save_file(
    const char *path,
    const uint8_t key[ALRIOS_EPOCH_KEY_SIZE],
    const alrios_epoch_state_t *state
) {
    int fd;
    int status;

    if (!path || !state) {
        return ALRIOS_EPOCH_ERR_INVALID_ARGUMENT;
    }

    fd = open(path, O_RDWR | O_CREAT, 0600);
    if (fd < 0) {
        return ALRIOS_EPOCH_ERR_IO;
    }

    status = alrios_epoch_state_write_fd(fd, key, state);
    (void)close(fd);
    return status;
}

int alrios_epoch_file_advance(
    const char *path,
    uint64_t next_epoch,
    const uint8_t key[ALRIOS_EPOCH_KEY_SIZE],
    alrios_epoch_state_t *out_state
) {
    alrios_epoch_state_t state;
    int fd;
    int status;
    struct stat st;

    if (!path) {
        return ALRIOS_EPOCH_ERR_INVALID_ARGUMENT;
    }

    fd = open(path, O_RDWR | O_CREAT, 0600);
    if (fd < 0) {
        return ALRIOS_EPOCH_ERR_IO;
    }

    if (flock(fd, LOCK_EX) != 0) {
        (void)close(fd);
        return ALRIOS_EPOCH_ERR_IO;
    }

    if (fstat(fd, &st) != 0) {
        (void)flock(fd, LOCK_UN);
        (void)close(fd);
        return ALRIOS_EPOCH_ERR_IO;
    }

    if (st.st_size == 0) {
        status = alrios_epoch_state_init(&state, next_epoch, key);
    } else {
        status = alrios_epoch_state_read_fd(fd, key, &state);
        if (status == ALRIOS_EPOCH_OK) {
            status = alrios_epoch_state_advance(&state, next_epoch, key);
        }
    }

    if (status == ALRIOS_EPOCH_OK) {
        status = alrios_epoch_state_write_fd(fd, key, &state);
    }

    if (status == ALRIOS_EPOCH_OK && out_state) {
        *out_state = state;
    }

    (void)flock(fd, LOCK_UN);
    (void)close(fd);
    return status;
}

/* Time Monitor */
int alrios_time_monitor_init(
    alrios_time_monitor_t *mon,
    uint64_t backward_step_threshold_ns
) {
    if (!mon) {
        return ALRIOS_EPOCH_ERR_INVALID_ARGUMENT;
    }
    (void)memset(mon, 0, sizeof(*mon));
    mon->backward_step_threshold_ns = backward_step_threshold_ns;
    return ALRIOS_EPOCH_OK;
}

int alrios_time_monitor_update(
    alrios_time_monitor_t *mon,
    uint64_t now_realtime_ns,
    uint64_t now_monotonic_ns
) {
    if (!mon) {
        return ALRIOS_EPOCH_ERR_INVALID_ARGUMENT;
    }

    /* First observation */
    if (mon->last_realtime_ns == 0U && mon->last_monotonic_ns == 0U) {
        mon->last_realtime_ns = now_realtime_ns;
        mon->last_monotonic_ns = now_monotonic_ns;
        return ALRIOS_EPOCH_OK;
    }

    /* Monotonic clock regression check */
    if (now_monotonic_ns < mon->last_monotonic_ns) {
        mon->tamper_detected = 1;
        mon->violations_count++;
        mon->last_violation_delta_ns = mon->last_monotonic_ns - now_monotonic_ns;
        return ALRIOS_EPOCH_ERR_TIME_BACKWARD;
    }

    /* Realtime backward jump check */
    if (now_realtime_ns < mon->last_realtime_ns) {
        uint64_t backward_delta = mon->last_realtime_ns - now_realtime_ns;
        if (backward_delta > mon->backward_step_threshold_ns) {
            mon->tamper_detected = 1;
            mon->violations_count++;
            mon->last_violation_delta_ns = backward_delta;
            return ALRIOS_EPOCH_ERR_TIME_BACKWARD;
        }
    }

    mon->last_realtime_ns = now_realtime_ns;
    mon->last_monotonic_ns = now_monotonic_ns;
    return ALRIOS_EPOCH_OK;
}

int alrios_time_monitor_sample(
    alrios_time_monitor_t *mon
) {
    uint64_t rt_ns = 0U;
    uint64_t mono_ns = 0U;
    int status;

    if (!mon) {
        return ALRIOS_EPOCH_ERR_INVALID_ARGUMENT;
    }

    status = get_current_clocks(&rt_ns, &mono_ns);
    if (status != ALRIOS_EPOCH_OK) {
        return status;
    }

    return alrios_time_monitor_update(mon, rt_ns, mono_ns);
}

int alrios_time_monitor_verify_against_anchor(
    const alrios_time_monitor_t *mon,
    const alrios_epoch_state_t *anchor
) {
    if (!mon || !anchor) {
        return ALRIOS_EPOCH_ERR_INVALID_ARGUMENT;
    }

    if (anchor->updated_at_ns > 0U && mon->last_realtime_ns > 0U) {
        if (mon->last_realtime_ns < anchor->updated_at_ns) {
            return ALRIOS_EPOCH_ERR_TIME_BACKWARD;
        }
    }
    return ALRIOS_EPOCH_OK;
}

/* Crypto Policy Gating */
int alrios_security_epoch_gate_policy(
    const alrios_epoch_state_t *state,
    const alrios_crypto_policy_t *policy
) {
    if (!state || !policy) {
        return ALRIOS_EPOCH_ERR_INVALID_ARGUMENT;
    }

    /* Policy epoch anti-rollback: policy epoch must be >= current system epoch */
    if (policy->policy_epoch < state->epoch) {
        return ALRIOS_EPOCH_ERR_POLICY_OUTDATED;
    }

    /* System epoch must meet minimum epoch requirement declared by policy */
    if (state->epoch < policy->min_epoch) {
        return ALRIOS_EPOCH_ERR_ROLLBACK;
    }

    /* Check validity window if specified */
    if (policy->valid_until_ns > 0U && state->updated_at_ns > policy->valid_until_ns) {
        return ALRIOS_EPOCH_ERR_EXPIRED;
    }

    return ALRIOS_EPOCH_OK;
}

int alrios_security_epoch_gate_algorithm(
    const alrios_epoch_state_t *state,
    alrios_crypto_algo_t algo
) {
    if (!state) {
        return ALRIOS_EPOCH_ERR_INVALID_ARGUMENT;
    }

    switch (algo) {
    case ALRIOS_ALGO_ED25519_CLASSIC:
        /* Classical Ed25519 revoked starting at epoch ALRIOS_EPOCH_CRYPTO_HYBRID_PQC */
        if (state->epoch >= ALRIOS_EPOCH_CRYPTO_HYBRID_PQC) {
            return ALRIOS_EPOCH_ERR_POLICY_OUTDATED;
        }
        return ALRIOS_EPOCH_OK;

    case ALRIOS_ALGO_ML_DSA_65:
    case ALRIOS_ALGO_HYBRID_ED25519_ML_DSA:
    case ALRIOS_ALGO_ML_KEM_768:
    case ALRIOS_ALGO_AES_256_GCM:
        return ALRIOS_EPOCH_OK;

    default:
        return ALRIOS_EPOCH_ERR_INVALID_ARGUMENT;
    }
}

const char *alrios_crypto_algo_name(
    alrios_crypto_algo_t algo
) {
    switch (algo) {
    case ALRIOS_ALGO_ED25519_CLASSIC:
        return "Ed25519 (RFC 8032 / Classic)";
    case ALRIOS_ALGO_ML_DSA_65:
        return "ML-DSA-65 (FIPS 204)";
    case ALRIOS_ALGO_HYBRID_ED25519_ML_DSA:
        return "Ed25519 + ML-DSA-65 Hybrid";
    case ALRIOS_ALGO_ML_KEM_768:
        return "ML-KEM-768 (FIPS 203)";
    case ALRIOS_ALGO_AES_256_GCM:
        return "AES-256-GCM";
    default:
        return "Unknown Algorithm";
    }
}

const char *alrios_epoch_strerror(int err) {
    switch (err) {
    case ALRIOS_EPOCH_OK:
        return "Success";
    case ALRIOS_EPOCH_ERR_INVALID_ARGUMENT:
        return "Invalid argument";
    case ALRIOS_EPOCH_ERR_ROLLBACK:
        return "Rollback attempt rejected";
    case ALRIOS_EPOCH_ERR_INTEGRITY:
        return "Security epoch integrity check failed";
    case ALRIOS_EPOCH_ERR_IO:
        return "I/O error";
    case ALRIOS_EPOCH_ERR_TIME_BACKWARD:
        return "Backward time jump or clock rollback detected";
    case ALRIOS_EPOCH_ERR_POLICY_OUTDATED:
        return "Outdated or revoked crypto policy";
    case ALRIOS_EPOCH_ERR_FORMAT:
        return "Invalid epoch record format";
    case ALRIOS_EPOCH_ERR_EXPIRED:
        return "Policy expired";
    case ALRIOS_EPOCH_ERR_OVERFLOW:
        return "Integer overflow";
    default:
        return "Unknown error";
    }
}
