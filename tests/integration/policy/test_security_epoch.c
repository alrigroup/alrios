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

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(cond) do { \
    if (!(cond)) { \
        (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                      __FILE__, __LINE__, #cond); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void test_epoch_monotonicity_and_rollback(void) {
    alrios_epoch_state_t state;
    uint8_t key[ALRIOS_EPOCH_KEY_SIZE];
    int status;

    (void)printf("[1/5] Testing security epoch state monotonicity & anti-rollback...\n");

    (void)memset(key, 0x5a, sizeof(key));

    /* Initialize at epoch 1 */
    CHECK(alrios_epoch_state_init(&state, 1U, key) == ALRIOS_EPOCH_OK);
    CHECK(state.epoch == 1U);
    CHECK(alrios_epoch_state_verify_integrity(&state, key) == ALRIOS_EPOCH_OK);

    /* Advance to epoch 2 */
    CHECK(alrios_epoch_state_advance(&state, 2U, key) == ALRIOS_EPOCH_OK);
    CHECK(state.epoch == 2U);
    CHECK(alrios_epoch_state_verify_integrity(&state, key) == ALRIOS_EPOCH_OK);

    /* Advance to epoch 2 (non-decreasing is allowed) */
    CHECK(alrios_epoch_state_advance(&state, 2U, key) == ALRIOS_EPOCH_OK);
    CHECK(state.epoch == 2U);

    /* Advance to epoch 5 */
    CHECK(alrios_epoch_state_advance(&state, 5U, key) == ALRIOS_EPOCH_OK);
    CHECK(state.epoch == 5U);
    CHECK(alrios_epoch_state_verify_integrity(&state, key) == ALRIOS_EPOCH_OK);

    /* Attempt rollback to epoch 4: MUST FAIL with ALRIOS_EPOCH_ERR_ROLLBACK */
    status = alrios_epoch_state_advance(&state, 4U, key);
    CHECK(status == ALRIOS_EPOCH_ERR_ROLLBACK);
    CHECK(state.epoch == 5U);

    /* Attempt rollback to epoch 1: MUST FAIL with ALRIOS_EPOCH_ERR_ROLLBACK */
    status = alrios_epoch_state_advance(&state, 1U, key);
    CHECK(status == ALRIOS_EPOCH_ERR_ROLLBACK);
    CHECK(state.epoch == 5U);

    /* Attempt rollback to epoch 0: MUST FAIL with ALRIOS_EPOCH_ERR_ROLLBACK */
    status = alrios_epoch_state_advance(&state, 0U, key);
    CHECK(status == ALRIOS_EPOCH_ERR_ROLLBACK);
    CHECK(state.epoch == 5U);
}

static void test_epoch_integrity_and_tamper_detection(void) {
    alrios_epoch_state_t state;
    alrios_epoch_state_t decoded;
    uint8_t key[ALRIOS_EPOCH_KEY_SIZE];
    uint8_t wrong_key[ALRIOS_EPOCH_KEY_SIZE];
    uint8_t wire[ALRIOS_EPOCH_WIRE_SIZE];
    uint8_t tampered_wire[ALRIOS_EPOCH_WIRE_SIZE];
    char temp_path[] = "/tmp/alrios-epoch-test-XXXXXX";
    int fd;

    (void)printf("[2/5] Testing epoch wire integrity and tamper-evidence...\n");

    (void)memset(key, 0x42, sizeof(key));
    (void)memset(wrong_key, 0x99, sizeof(wrong_key));

    CHECK(alrios_epoch_state_init(&state, 10U, key) == ALRIOS_EPOCH_OK);
    CHECK(alrios_epoch_state_encode(&state, key, wire) == ALRIOS_EPOCH_OK);

    /* Decode with correct key */
    CHECK(alrios_epoch_state_decode(wire, key, &decoded) == ALRIOS_EPOCH_OK);
    CHECK(decoded.epoch == 10U);

    /* Decode with wrong key: MUST fail integrity check */
    CHECK(alrios_epoch_state_decode(wire, wrong_key, &decoded) == ALRIOS_EPOCH_ERR_INTEGRITY);

    /* Tamper with epoch counter field in wire body */
    (void)memcpy(tampered_wire, wire, sizeof(wire));
    tampered_wire[19] ^= 0x01U; /* flip bit in epoch integer */
    CHECK(alrios_epoch_state_decode(tampered_wire, key, &decoded) == ALRIOS_EPOCH_ERR_INTEGRITY);

    /* Tamper with timestamp field */
    (void)memcpy(tampered_wire, wire, sizeof(wire));
    tampered_wire[25] ^= 0x02U;
    CHECK(alrios_epoch_state_decode(tampered_wire, key, &decoded) == ALRIOS_EPOCH_ERR_INTEGRITY);

    /* Tamper with integrity tag itself */
    (void)memcpy(tampered_wire, wire, sizeof(wire));
    tampered_wire[ALRIOS_EPOCH_BODY_SIZE + 5U] ^= 0xaaU;
    CHECK(alrios_epoch_state_decode(tampered_wire, key, &decoded) == ALRIOS_EPOCH_ERR_INTEGRITY);

    /* Tamper with magic header */
    (void)memcpy(tampered_wire, wire, sizeof(wire));
    tampered_wire[0] = 'X';
    CHECK(alrios_epoch_state_decode(tampered_wire, key, &decoded) == ALRIOS_EPOCH_ERR_FORMAT);

    /* Test unkeyed (SHA-512 domain-separated) integrity protection */
    CHECK(alrios_epoch_state_init(&state, 20U, NULL) == ALRIOS_EPOCH_OK);
    CHECK(alrios_epoch_state_encode(&state, NULL, wire) == ALRIOS_EPOCH_OK);
    CHECK(alrios_epoch_state_decode(wire, NULL, &decoded) == ALRIOS_EPOCH_OK);
    CHECK(decoded.epoch == 20U);
    (void)memcpy(tampered_wire, wire, sizeof(wire));
    tampered_wire[19] ^= 0x01U;
    CHECK(alrios_epoch_state_decode(tampered_wire, NULL, &decoded) == ALRIOS_EPOCH_ERR_INTEGRITY);

    /* Test file persistence and atomic advancement */
    fd = mkstemp(temp_path);
    CHECK(fd >= 0);
    (void)close(fd);

    CHECK(alrios_epoch_file_advance(temp_path, 1U, key, &state) == ALRIOS_EPOCH_OK);
    CHECK(state.epoch == 1U);
    CHECK(alrios_epoch_file_advance(temp_path, 3U, key, &state) == ALRIOS_EPOCH_OK);
    CHECK(state.epoch == 3U);

    /* Attempt rollback in file: MUST FAIL */
    CHECK(alrios_epoch_file_advance(temp_path, 2U, key, &state) == ALRIOS_EPOCH_ERR_ROLLBACK);

    /* Verify file still holds epoch 3 */
    CHECK(alrios_epoch_state_load_file(temp_path, key, &state) == ALRIOS_EPOCH_OK);
    CHECK(state.epoch == 3U);

    (void)unlink(temp_path);
}

static void test_time_monitor_backward_detection(void) {
    alrios_time_monitor_t mon;
    alrios_epoch_state_t anchor;
    uint8_t key[ALRIOS_EPOCH_KEY_SIZE];
    uint64_t t_real = 1000000000000ULL; /* 1000s in ns */
    uint64_t t_mono = 100000000000ULL;  /* 100s in ns */

    (void)printf("[3/5] Testing secure time monitor backward time rejection...\n");

    (void)memset(key, 0x33, sizeof(key));

    /* Strict threshold = 0 */
    CHECK(alrios_time_monitor_init(&mon, 0U) == ALRIOS_EPOCH_OK);

    /* First sample: sets baseline */
    CHECK(alrios_time_monitor_update(&mon, t_real, t_mono) == ALRIOS_EPOCH_OK);

    /* Normal forward progress: +5s realtime, +5s monotonic */
    t_real += 5000000000ULL;
    t_mono += 5000000000ULL;
    CHECK(alrios_time_monitor_update(&mon, t_real, t_mono) == ALRIOS_EPOCH_OK);

    /* Monotonic regression (monotonic went backward by 1ns): MUST BE REJECTED */
    CHECK(alrios_time_monitor_update(&mon, t_real + 1000ULL, t_mono - 1ULL) ==
          ALRIOS_EPOCH_ERR_TIME_BACKWARD);
    CHECK(mon.tamper_detected == 1);

    /* Realtime regression: backward jump by 2 seconds */
    CHECK(alrios_time_monitor_init(&mon, 0U) == ALRIOS_EPOCH_OK);
    CHECK(alrios_time_monitor_update(&mon, t_real, t_mono) == ALRIOS_EPOCH_OK);
    CHECK(alrios_time_monitor_update(&mon, t_real - 2000000000ULL, t_mono + 1000000000ULL) ==
          ALRIOS_EPOCH_ERR_TIME_BACKWARD);
    CHECK(mon.tamper_detected == 1);

    /* With threshold: backward step exceeding threshold rejected */
    CHECK(alrios_time_monitor_init(&mon, 500000000ULL) == ALRIOS_EPOCH_OK); /* 500ms tolerance */
    CHECK(alrios_time_monitor_update(&mon, t_real, t_mono) == ALRIOS_EPOCH_OK);

    /* Small jitter within threshold (200ms backward) accepted */
    CHECK(alrios_time_monitor_update(&mon, t_real - 200000000ULL, t_mono + 100000000ULL) ==
          ALRIOS_EPOCH_OK);

    /* Jitter exceeding threshold (600ms backward) rejected */
    CHECK(alrios_time_monitor_update(&mon, t_real - 800000000ULL, t_mono + 200000000ULL) ==
          ALRIOS_EPOCH_ERR_TIME_BACKWARD);

    /* Test against anchor in security epoch state */
    CHECK(alrios_epoch_state_init(&anchor, 1U, key) == ALRIOS_EPOCH_OK);
    anchor.updated_at_ns = 2000000000000ULL; /* Anchor is at 2000s */

    CHECK(alrios_time_monitor_init(&mon, 0U) == ALRIOS_EPOCH_OK);
    /* Current time is 1900s (< anchor 2000s) */
    CHECK(alrios_time_monitor_update(&mon, 1900000000000ULL, 50000000000ULL) == ALRIOS_EPOCH_OK);

    /* Verify against anchor: system time is behind anchor -> MUST REJECT */
    CHECK(alrios_time_monitor_verify_against_anchor(&mon, &anchor) == ALRIOS_EPOCH_ERR_TIME_BACKWARD);

    /* Current time is 2005s (> anchor 2000s) -> ACCEPT */
    CHECK(alrios_time_monitor_update(&mon, 2005000000000ULL, 55000000000ULL) == ALRIOS_EPOCH_OK);
    CHECK(alrios_time_monitor_verify_against_anchor(&mon, &anchor) == ALRIOS_EPOCH_OK);

    /* Real hardware sample test */
    CHECK(alrios_time_monitor_init(&mon, 0U) == ALRIOS_EPOCH_OK);
    CHECK(alrios_time_monitor_sample(&mon) == ALRIOS_EPOCH_OK);
    CHECK(mon.last_realtime_ns > 0U);
    CHECK(mon.last_monotonic_ns > 0U);
}

static void test_crypto_policy_gating(void) {
    alrios_epoch_state_t state;
    alrios_crypto_policy_t policy;
    uint8_t key[ALRIOS_EPOCH_KEY_SIZE];

    (void)printf("[4/5] Testing crypto policy anti-rollback gating...\n");

    (void)memset(key, 0x11, sizeof(key));

    /* Initialize at epoch 1 (Classical Ed25519 permitted) */
    CHECK(alrios_epoch_state_init(&state, 1U, key) == ALRIOS_EPOCH_OK);

    /* Classical algorithm permitted at epoch 1 */
    CHECK(alrios_security_epoch_gate_algorithm(&state, ALRIOS_ALGO_ED25519_CLASSIC) ==
          ALRIOS_EPOCH_OK);
    CHECK(alrios_security_epoch_gate_algorithm(&state, ALRIOS_ALGO_HYBRID_ED25519_ML_DSA) ==
          ALRIOS_EPOCH_OK);
    CHECK(alrios_security_epoch_gate_algorithm(&state, ALRIOS_ALGO_ML_DSA_65) ==
          ALRIOS_EPOCH_OK);

    /* Construct policy with policy_epoch = 1 */
    (void)memset(&policy, 0, sizeof(policy));
    policy.policy_epoch = 1U;
    policy.min_epoch = 1U;
    policy.allowed_algorithms_mask = (1U << ALRIOS_ALGO_ED25519_CLASSIC);
    policy.require_pqc = 0;
    CHECK(alrios_security_epoch_gate_policy(&state, &policy) == ALRIOS_EPOCH_OK);

    /* Advance security epoch to 2 (ML-DSA Hybrid mandatory, Classical Ed25519 revoked) */
    CHECK(alrios_epoch_state_advance(&state, 2U, key) == ALRIOS_EPOCH_OK);

    /* Policy with policy_epoch = 1 (< current system epoch 2): MUST BE REJECTED */
    CHECK(alrios_security_epoch_gate_policy(&state, &policy) ==
          ALRIOS_EPOCH_ERR_POLICY_OUTDATED);

    /* Classical Ed25519 algorithm directly gated: MUST BE REJECTED at epoch 2 */
    CHECK(alrios_security_epoch_gate_algorithm(&state, ALRIOS_ALGO_ED25519_CLASSIC) ==
          ALRIOS_EPOCH_ERR_POLICY_OUTDATED);

    /* Hybrid and pure ML-DSA-65 algorithms: accepted */
    CHECK(alrios_security_epoch_gate_algorithm(&state, ALRIOS_ALGO_HYBRID_ED25519_ML_DSA) ==
          ALRIOS_EPOCH_OK);
    CHECK(alrios_security_epoch_gate_algorithm(&state, ALRIOS_ALGO_ML_DSA_65) ==
          ALRIOS_EPOCH_OK);

    /* Updated policy with policy_epoch = 2 but require_pqc = 0 and classic only */
    policy.policy_epoch = 2U;
    policy.min_epoch = 2U;
    /* Policy requesting higher system epoch than current */
    policy.min_epoch = 5U;
    CHECK(alrios_security_epoch_gate_policy(&state, &policy) == ALRIOS_EPOCH_ERR_ROLLBACK);

    policy.min_epoch = 2U;
    policy.allowed_algorithms_mask = (1U << ALRIOS_ALGO_HYBRID_ED25519_ML_DSA);
    policy.require_pqc = 1;
    CHECK(alrios_security_epoch_gate_policy(&state, &policy) == ALRIOS_EPOCH_OK);

    /* Test expiration gating */
    policy.valid_until_ns = state.updated_at_ns - 1000ULL; /* Expired policy */
    CHECK(alrios_security_epoch_gate_policy(&state, &policy) == ALRIOS_EPOCH_ERR_EXPIRED);
}

static int run_process(char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            (void)dup2(devnull, STDOUT_FILENO);
            (void)dup2(devnull, STDERR_FILENO);
            (void)close(devnull);
        }
        execv(argv[0], argv);
        _exit(127);
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            return -1;
        }
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}

static void test_ar_chronod_daemon_cli(void) {
    const char *chronod_bin = getenv("ALRIOS_CHRONOD_BIN");
    char temp_state[] = "/tmp/alrios-chronod-state-XXXXXX";
    int fd;
    int ret;

    (void)printf("[5/5] Testing ar_chronod daemon execution & fail-closed responses...\n");

    if (!chronod_bin) {
        chronod_bin = "../arcore/ar_chronod";
        if (access(chronod_bin, X_OK) != 0) {
            chronod_bin = "arcore/ar_chronod";
        }
    }

    if (access(chronod_bin, X_OK) != 0) {
        (void)printf("  Note: ar_chronod binary not directly accessible, skipping daemon CLI execution test\n");
        return;
    }

    fd = mkstemp(temp_state);
    CHECK(fd >= 0);
    (void)close(fd);
    (void)unlink(temp_state);

    /* 1. Advance epoch to 2 via ar_chronod */
    {
        char *const argv[] = {
            (char *)(uintptr_t)chronod_bin,
            (char *)"--state-file",
            temp_state,
            (char *)"--set-epoch",
            (char *)"2",
            NULL
        };
        ret = run_process(argv);
        CHECK(ret == 0);
    }

    /* 2. Run --once health check: should succeed */
    {
        char *const argv[] = {
            (char *)(uintptr_t)chronod_bin,
            (char *)"--state-file",
            temp_state,
            (char *)"--once",
            NULL
        };
        ret = run_process(argv);
        CHECK(ret == 0);
    }

    /* 3. Attempt rollback: --set-epoch 1 MUST FAIL-CLOSED with non-zero exit */
    {
        char *const argv[] = {
            (char *)(uintptr_t)chronod_bin,
            (char *)"--state-file",
            temp_state,
            (char *)"--set-epoch",
            (char *)"1",
            NULL
        };
        ret = run_process(argv);
        CHECK(ret != 0);
    }

    /* 4. Corrupt state file and verify ar_chronod fails closed */
    fd = open(temp_state, O_WRONLY);
    CHECK(fd >= 0);
    CHECK(write(fd, "GARBAGE_DATA_CORRUPTION", 23) == 23);
    (void)close(fd);

    {
        char *const argv[] = {
            (char *)(uintptr_t)chronod_bin,
            (char *)"--state-file",
            temp_state,
            (char *)"--once",
            NULL
        };
        ret = run_process(argv);
        CHECK(ret != 0);
    }

    (void)unlink(temp_state);
}

int main(void) {
    (void)printf("=== ALRIOS MP-026: Secure Time Monitor & Anti-Rollback Security Epoch Suite ===\n");

    test_epoch_monotonicity_and_rollback();
    test_epoch_integrity_and_tamper_detection();
    test_time_monitor_backward_detection();
    test_crypto_policy_gating();
    test_ar_chronod_daemon_cli();

    (void)printf("MP-026 (Secure Time Monitor & Anti-Rollback Security Epoch): PASS\n");
    return EXIT_SUCCESS;
}
