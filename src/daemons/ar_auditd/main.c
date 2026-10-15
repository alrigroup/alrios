/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#define _POSIX_C_SOURCE 200809L

#include "alrios/audit/block.h"
#include "alrios/audit/forwarder.h"
#include "alrios/crypto_verify.h"
#include <alrios/platform/aros_hal.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define AUDITD_DEFAULT_QUEUE_SIZE 128U

static volatile sig_atomic_t g_keep_running = 1;

static void handle_signal(int sig) {
    (void)sig;
    g_keep_running = 0;
}

static void print_usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s --audit-file <path> --state-file <path> --key-file <path> "
            "[--once] [--drain-existing]\n",
            prog);
}

static int read_key_file(const char *path,
                         uint8_t out_key[ALRIOS_FORWARDER_KEY_SIZE]) {
    int fd;
    ssize_t bytes_read;

    if (!path || !out_key) {
        return -1;
    }
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    bytes_read = read(fd, out_key, ALRIOS_FORWARDER_KEY_SIZE);
    close(fd);
    if (bytes_read != (ssize_t)ALRIOS_FORWARDER_KEY_SIZE) {
        alrios_explicit_zeroize(out_key, ALRIOS_FORWARDER_KEY_SIZE);
        return -1;
    }
    return 0;
}

/* Local collector simulation callback when running standalone without remote */
static int local_loopback_collector(
    const uint8_t *checkpoint_wire,
    size_t wire_len,
    uint8_t *receipt_buf,
    size_t receipt_buf_capacity,
    size_t *receipt_received_len,
    void *user_ctx) {
    const uint8_t *key = user_ctx;
    alrios_audit_checkpoint_t cp;
    uint8_t nonce[ALRIOS_FORWARDER_RECEIPT_NONCE_SIZE];
    int status;

    if (!checkpoint_wire || wire_len != ALRIOS_CHECKPOINT_WIRE_SIZE ||
        !receipt_buf || receipt_buf_capacity < ALRIOS_RECEIPT_WIRE_SIZE ||
        !receipt_received_len || !key) {
        return ALRIOS_FORWARDER_ERR_INVALID_ARGUMENT;
    }
    status = alrios_checkpoint_decode_and_verify(checkpoint_wire, key, &cp);
    if (status != ALRIOS_FORWARDER_OK) {
        return status;
    }
    memset(nonce, 0x42, sizeof(nonce));
    status = alrios_receipt_encode(cp.sequence, cp.block_hash, nonce, key,
                                   receipt_buf);
    if (status == ALRIOS_FORWARDER_OK) {
        *receipt_received_len = ALRIOS_RECEIPT_WIRE_SIZE;
    }
    return status;
}

int main(int argc, char **argv) {
    const char *audit_path = NULL;
    const char *state_path = NULL;
    const char *key_path = NULL;
    int run_once = 0;
    int drain_existing = 0;
    int audit_fd = -1;
    int state_fd = -1;
    int exit_code = EXIT_FAILURE;
    int index;
    uint8_t auth_key[ALRIOS_FORWARDER_KEY_SIZE];
    alrios_audit_checkpoint_t queue_buffer[AUDITD_DEFAULT_QUEUE_SIZE];
    alrios_audit_forwarder_t forwarder;
    alrios_audit_chain_result_t chain_result;
    alrios_forwarder_state_t persisted_state;
    int fwd_initialized = 0;

    for (index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--audit-file") == 0) {
            if (++index >= argc) {
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
            audit_path = argv[index];
        } else if (strcmp(argv[index], "--state-file") == 0) {
            if (++index >= argc) {
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
            state_path = argv[index];
        } else if (strcmp(argv[index], "--key-file") == 0) {
            if (++index >= argc) {
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
            key_path = argv[index];
        } else if (strcmp(argv[index], "--once") == 0) {
            run_once = 1;
        } else if (strcmp(argv[index], "--drain-existing") == 0) {
            drain_existing = 1;
        } else {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (!audit_path || !state_path || !key_path) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (signal(SIGINT, handle_signal) == SIG_ERR ||
        signal(SIGTERM, handle_signal) == SIG_ERR) {
        fprintf(stderr, "ar_auditd: failed to install signal handlers\n");
        return EXIT_FAILURE;
    }

    if (read_key_file(key_path, auth_key) != 0) {
        fprintf(stderr, "ar_auditd: failed to read key file\n");
        return EXIT_FAILURE;
    }

    audit_fd = open(audit_path, O_RDWR);
    if (audit_fd < 0) {
        fprintf(stderr, "ar_auditd: failed to open audit file\n");
        goto cleanup;
    }

    state_fd = open(state_path, O_RDWR | O_CREAT, 0600);
    if (state_fd < 0) {
        fprintf(stderr, "ar_auditd: failed to open state file\n");
        goto cleanup;
    }

    /* Check rollback against persisted state before any processing */
    if (alrios_forwarder_state_read(state_fd, auth_key, &persisted_state) !=
        ALRIOS_FORWARDER_OK) {
        fprintf(stderr, "ar_auditd: failed to read or verify state file\n");
        goto cleanup;
    }

    if (alrios_forwarder_verify_chain_not_rolled_back(audit_fd,
                                                      &persisted_state) !=
        ALRIOS_FORWARDER_OK) {
        fprintf(stderr, "ar_auditd: FAIL-CLOSED: audit log rollback or tampering detected!\n");
        goto cleanup;
    }

    if (alrios_forwarder_init(&forwarder, queue_buffer, AUDITD_DEFAULT_QUEUE_SIZE,
                              auth_key, state_fd, local_loopback_collector,
                              auth_key) != ALRIOS_FORWARDER_OK) {
        fprintf(stderr, "ar_auditd: failed to initialize forwarder\n");
        goto cleanup;
    }
    fwd_initialized = 1;

    if (drain_existing) {
        if (alrios_audit_chain_verify_fd(audit_fd, &chain_result) == ALRIOS_AUDIT_OK &&
            chain_result.block_count > 0U &&
            chain_result.last_sequence > persisted_state.ack_sequence) {
            int status = alrios_forwarder_enqueue(&forwarder, chain_result.last_sequence,
                                                   0U, chain_result.last_hash);
            if (status != ALRIOS_FORWARDER_OK) {
                fprintf(stderr, "ar_auditd: forwarder enqueue failed (%d)\n", status);
                goto cleanup;
            }
            status = alrios_forwarder_flush(&forwarder);
            if (status != ALRIOS_FORWARDER_OK) {
                fprintf(stderr, "ar_auditd: forwarder flush failed (%d)\n", status);
                goto cleanup;
            }
        }
    }

    while (g_keep_running) {
        /* Monitor chain tip and forward when new blocks appear */
        if (alrios_audit_chain_verify_fd(audit_fd, &chain_result) == ALRIOS_AUDIT_OK) {
            if (chain_result.block_count > 0U &&
                chain_result.last_sequence > forwarder.persisted_state.ack_sequence) {
                if (alrios_forwarder_enqueue(&forwarder, chain_result.last_sequence,
                                             0U, chain_result.last_hash) == ALRIOS_FORWARDER_OK) {
                    (void)alrios_forwarder_flush(&forwarder);
                }
            }
        } else {
            fprintf(stderr, "ar_auditd: audit chain verification failed\n");
            goto cleanup;
        }

        if (run_once) {
            break;
        }
        ar_sleep_ms(100U);
    }

    exit_code = EXIT_SUCCESS;

cleanup:
    if (fwd_initialized) {
        alrios_forwarder_destroy(&forwarder);
    }
    alrios_explicit_zeroize(auth_key, sizeof(auth_key));
    if (audit_fd >= 0) {
        close(audit_fd);
    }
    if (state_fd >= 0) {
        close(state_fd);
    }
    return exit_code;
}
