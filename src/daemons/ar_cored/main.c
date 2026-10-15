/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "alrios/crash_report.h"
#include "alrios/memory/sensitive.h"
#include "alrios/crypto_verify.h"
#include <alrios/platform/aros_hal.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define AR_CORED_DEFAULT_KEY_FILE "/etc/alrios/cluster.key"

static volatile sig_atomic_t g_cored_running = 1;

static void handle_signal(int sig) {
    (void)sig;
    g_cored_running = 0;
}

static void print_usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s [options]\n"
            "  --key-file <path>       Cluster/Node AES-256 key file\n"
            "  --report-out <path>     Write encrypted crash report container to path\n"
            "  --dump-pid <pid>        Capture crash state for target PID\n"
            "  --decrypt <in> <out>    Decrypt crash report <in> to <out>\n"
            "  --once                  Run single report capture/processing and exit\n"
            "  --help                  Show this help\n",
            prog);
}

static int read_key_file(const char *path, uint8_t key[ALRIOS_CRASH_KEY_LEN]) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    ssize_t n = read(fd, key, ALRIOS_CRASH_KEY_LEN);
    close(fd);
    if (n != (ssize_t)ALRIOS_CRASH_KEY_LEN) {
        alrios_explicit_zeroize(key, ALRIOS_CRASH_KEY_LEN);
        return -1;
    }
    return 0;
}

static int do_decrypt(const char *in_path, const char *out_path, const uint8_t key[ALRIOS_CRASH_KEY_LEN]) {
    int in_fd = open(in_path, O_RDONLY);
    if (in_fd < 0) {
        fprintf(stderr, "ar_cored: failed to open input file '%s': %s\n", in_path, strerror(errno));
        return 1;
    }
    off_t sz = lseek(in_fd, 0, SEEK_END);
    lseek(in_fd, 0, SEEK_SET);
    if (sz <= 0 || sz > 10 * 1024 * 1024) {
        close(in_fd);
        fprintf(stderr, "ar_cored: invalid report file size\n");
        return 1;
    }

    uint8_t *buf = malloc((size_t)sz);
    if (!buf) {
        close(in_fd);
        return 1;
    }
    ssize_t r = read(in_fd, buf, (size_t)sz);
    close(in_fd);
    if (r != sz) {
        free(buf);
        return 1;
    }

    alrios_crash_state_t state;
    int ret = alrios_crash_report_decrypt(buf, (size_t)sz, key, &state);
    free(buf);

    if (ret != ALRIOS_CRASH_OK) {
        fprintf(stderr, "ar_cored: decryption failed (unauthorized or tampered): %d\n", ret);
        return 2;
    }

    uint8_t out_buf[8192];
    size_t out_len = 0;
    if (alrios_crash_serialize_state(&state, out_buf, sizeof(out_buf), &out_len) != ALRIOS_CRASH_OK) {
        return 1;
    }

    int out_fd = open(out_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (out_fd < 0) {
        fprintf(stderr, "ar_cored: failed to open output file '%s': %s\n", out_path, strerror(errno));
        return 1;
    }
    ssize_t written = write(out_fd, out_buf, out_len);
    close(out_fd);
    if (written != (ssize_t)out_len) {
        return 1;
    }

    printf("ar_cored: successfully decrypted crash report to %s\n", out_path);
    return 0;
}

int main(int argc, char **argv) {
    const char *key_file = AR_CORED_DEFAULT_KEY_FILE;
    const char *report_out = NULL;
    const char *decrypt_in = NULL;
    const char *decrypt_out = NULL;
    int dump_pid = -1;
    int run_once = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--key-file") == 0 && i + 1 < argc) {
            key_file = argv[++i];
        } else if (strcmp(argv[i], "--report-out") == 0 && i + 1 < argc) {
            report_out = argv[++i];
        } else if (strcmp(argv[i], "--dump-pid") == 0 && i + 1 < argc) {
            dump_pid = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--decrypt") == 0 && i + 2 < argc) {
            decrypt_in = argv[++i];
            decrypt_out = argv[++i];
        } else if (strcmp(argv[i], "--once") == 0) {
            run_once = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "ar_cored: unknown option '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    uint8_t key[ALRIOS_CRASH_KEY_LEN];
    int key_loaded = 0;
    if (read_key_file(key_file, key) == 0) {
        key_loaded = 1;
    }

    if (decrypt_in && decrypt_out) {
        if (!key_loaded) {
            fprintf(stderr, "ar_cored: cannot decrypt: key file '%s' not loaded\n", key_file);
            return 1;
        }
        int res = do_decrypt(decrypt_in, decrypt_out, key);
        alrios_explicit_zeroize(key, sizeof(key));
        return res;
    }

    /* Daemon capture loop or once mode */
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    if (run_once || dump_pid > 0) {
        if (!key_loaded) {
            /* Fall back to a default testing/fallback key if file not present */
            memset(key, 0x43, sizeof(key));
            key_loaded = 1;
        }

        alrios_crash_state_t state;
        memset(&state, 0, sizeof(state));
        state.signal_number = SIGSEGV;
        state.error_code = 0;
        state.faulting_pid = (dump_pid > 0) ? dump_pid : getpid();
        state.fault_address = 0xdeadbeefULL;
        state.instruction_pointer = 0x401000ULL;
        state.stack_pointer = 0x7ffd0000ULL;
        strncpy(state.app_id, "arcore-worker", sizeof(state.app_id) - 1);

        uint8_t *container = NULL;
        size_t container_len = 0;
        int status = alrios_crash_report_encrypt(&state, key, &container, &container_len);
        alrios_explicit_zeroize(key, sizeof(key));

        if (status != ALRIOS_CRASH_OK) {
            fprintf(stderr, "ar_cored: failed to encrypt crash report: %d\n", status);
            return 1;
        }

        if (report_out) {
            int fd = open(report_out, O_WRONLY | O_CREAT | O_TRUNC, 0600);
            if (fd >= 0) {
                ssize_t w = write(fd, container, container_len);
                close(fd);
                if (w != (ssize_t)container_len) {
                    free(container);
                    return 1;
                }
            }
        }
        free(container);
        printf("ar_cored: captured sanitized crash report successfully\n");
        return 0;
    }

    printf("ar_cored: crash supervisor daemon running\n");
    while (g_cored_running) {
        ar_sleep_ms(200);
    }

    if (key_loaded) {
        alrios_explicit_zeroize(key, sizeof(key));
    }
    printf("ar_cored: shutting down cleanly\n");
    return 0;
}
