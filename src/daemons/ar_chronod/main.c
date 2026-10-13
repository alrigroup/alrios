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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define AR_CHRONOD_DEFAULT_INTERVAL_MS 1000U
#define AR_CHRONOD_DEFAULT_THRESHOLD_MS 0U

static volatile sig_atomic_t g_keep_running = 1;

static void handle_signal(int sig) {
    (void)sig;
    g_keep_running = 0;
}

static void print_usage(const char *prog) {
    (void)fprintf(stderr,
        "Usage: %s --state-file <path> [options]\n"
        "Options:\n"
        "  --key-file <path>       Path to 32-byte authentication key file\n"
        "  --interval-ms <N>       Sampling interval in milliseconds (default %u)\n"
        "  --threshold-ms <N>      Backward step threshold in milliseconds (default %u)\n"
        "  --set-epoch <N>         Advance/set security epoch to N\n"
        "  --once                  Perform single health check and exit\n"
        "  --help                  Show this usage message\n",
        prog, AR_CHRONOD_DEFAULT_INTERVAL_MS, AR_CHRONOD_DEFAULT_THRESHOLD_MS);
}

static int read_key_file(const char *path, uint8_t out_key[ALRIOS_EPOCH_KEY_SIZE]) {
    int fd;
    ssize_t bytes_read;

    if (!path || !out_key) {
        return -1;
    }
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    bytes_read = read(fd, out_key, ALRIOS_EPOCH_KEY_SIZE);
    (void)close(fd);
    if (bytes_read != (ssize_t)ALRIOS_EPOCH_KEY_SIZE) {
        alrios_explicit_zeroize(out_key, ALRIOS_EPOCH_KEY_SIZE);
        return -1;
    }
    return 0;
}

static int sleep_interval(unsigned int interval_ms) {
    struct timespec requested;
    struct timespec remaining;

    requested.tv_sec = (time_t)(interval_ms / 1000U);
    requested.tv_nsec = (long)((interval_ms % 1000U) * 1000000U);
    while (g_keep_running && nanosleep(&requested, &remaining) != 0) {
        if (errno != EINTR) {
            return -1;
        }
        requested = remaining;
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *state_path = NULL;
    const char *key_path = NULL;
    uint8_t auth_key[ALRIOS_EPOCH_KEY_SIZE];
    const uint8_t *key_ptr = NULL;
    unsigned int interval_ms = AR_CHRONOD_DEFAULT_INTERVAL_MS;
    unsigned int threshold_ms = AR_CHRONOD_DEFAULT_THRESHOLD_MS;
    int run_once = 0;
    int set_epoch_flag = 0;
    uint64_t new_epoch = 0U;
    alrios_epoch_state_t state;
    alrios_time_monitor_t monitor;
    int status;
    int i;
    unsigned int tick_counter = 0U;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--state-file") == 0) {
            if (++i >= argc) {
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
            state_path = argv[i];
        } else if (strcmp(argv[i], "--key-file") == 0) {
            if (++i >= argc) {
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
            key_path = argv[i];
        } else if (strcmp(argv[i], "--interval-ms") == 0) {
            if (++i >= argc) {
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
            interval_ms = (unsigned int)strtoul(argv[i], NULL, 10);
        } else if (strcmp(argv[i], "--threshold-ms") == 0) {
            if (++i >= argc) {
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
            threshold_ms = (unsigned int)strtoul(argv[i], NULL, 10);
        } else if (strcmp(argv[i], "--set-epoch") == 0) {
            if (++i >= argc) {
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
            set_epoch_flag = 1;
            new_epoch = (uint64_t)strtoull(argv[i], NULL, 10);
        } else if (strcmp(argv[i], "--once") == 0) {
            run_once = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        } else {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (!state_path) {
        (void)fprintf(stderr, "ar_chronod: --state-file is required\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (key_path) {
        if (read_key_file(key_path, auth_key) != 0) {
            (void)fprintf(stderr, "ar_chronod: failed to read key file: %s\n", key_path);
            return EXIT_FAILURE;
        }
        key_ptr = auth_key;
    }

    if (set_epoch_flag) {
        status = alrios_epoch_file_advance(state_path, new_epoch, key_ptr, &state);
        if (key_ptr) {
            alrios_explicit_zeroize(auth_key, sizeof(auth_key));
        }
        if (status == ALRIOS_EPOCH_ERR_ROLLBACK) {
            (void)fprintf(stderr,
                "ar_chronod: FAIL-CLOSED: anti-rollback security epoch downgrade rejected (target=%" PRIu64 ")\n",
                new_epoch);
            return EXIT_FAILURE;
        }
        if (status != ALRIOS_EPOCH_OK) {
            (void)fprintf(stderr, "ar_chronod: failed to set epoch (%s)\n",
                          alrios_epoch_strerror(status));
            return EXIT_FAILURE;
        }
        (void)fprintf(stdout, "ar_chronod: epoch advanced successfully to %" PRIu64 "\n", state.epoch);
        return EXIT_SUCCESS;
    }

    /* Initialize or load state file */
    status = alrios_epoch_state_load_file(state_path, key_ptr, &state);
    if (status != ALRIOS_EPOCH_OK) {
        /* If file does not exist, initialize epoch 1 */
        if (status == ALRIOS_EPOCH_ERR_IO) {
            status = alrios_epoch_state_init(&state, 1U, key_ptr);
            if (status == ALRIOS_EPOCH_OK) {
                status = alrios_epoch_state_save_file(state_path, key_ptr, &state);
            }
        }
        if (status != ALRIOS_EPOCH_OK) {
            if (key_ptr) {
                alrios_explicit_zeroize(auth_key, sizeof(auth_key));
            }
            (void)fprintf(stderr, "ar_chronod: FAIL-CLOSED: state file initialization failed (%s)\n",
                          alrios_epoch_strerror(status));
            return EXIT_FAILURE;
        }
    }

    /* Setup time monitor */
    status = alrios_time_monitor_init(&monitor, (uint64_t)threshold_ms * 1000000ULL);
    if (status != ALRIOS_EPOCH_OK) {
        if (key_ptr) {
            alrios_explicit_zeroize(auth_key, sizeof(auth_key));
        }
        return EXIT_FAILURE;
    }

    status = alrios_time_monitor_sample(&monitor);
    if (status != ALRIOS_EPOCH_OK) {
        if (key_ptr) {
            alrios_explicit_zeroize(auth_key, sizeof(auth_key));
        }
        (void)fprintf(stderr, "ar_chronod: FAIL-CLOSED: initial clock sampling failed (%s)\n",
                      alrios_epoch_strerror(status));
        return EXIT_FAILURE;
    }

    status = alrios_time_monitor_verify_against_anchor(&monitor, &state);
    if (status != ALRIOS_EPOCH_OK) {
        if (key_ptr) {
            alrios_explicit_zeroize(auth_key, sizeof(auth_key));
        }
        (void)fprintf(stderr,
            "ar_chronod: FAIL-CLOSED: backward time jump or clock rollback detected against security anchor!\n");
        return EXIT_FAILURE;
    }

    if (run_once) {
        if (key_ptr) {
            alrios_explicit_zeroize(auth_key, sizeof(auth_key));
        }
        (void)fprintf(stdout,
            "ar_chronod: secure time monitor check passed (epoch=%" PRIu64 ")\n", state.epoch);
        return EXIT_SUCCESS;
    }

    if (signal(SIGINT, handle_signal) == SIG_ERR ||
        signal(SIGTERM, handle_signal) == SIG_ERR) {
        if (key_ptr) {
            alrios_explicit_zeroize(auth_key, sizeof(auth_key));
        }
        (void)fprintf(stderr, "ar_chronod: failed to install signal handlers\n");
        return EXIT_FAILURE;
    }

    while (g_keep_running) {
        if (sleep_interval(interval_ms) != 0) {
            break;
        }
        if (!g_keep_running) {
            break;
        }

        status = alrios_time_monitor_sample(&monitor);
        if (status != ALRIOS_EPOCH_OK) {
            (void)fprintf(stderr, "ar_chronod: FAIL-CLOSED: clock anomaly detected: %s\n",
                          alrios_epoch_strerror(status));
            if (key_ptr) {
                alrios_explicit_zeroize(auth_key, sizeof(auth_key));
            }
            return EXIT_FAILURE;
        }

        status = alrios_time_monitor_verify_against_anchor(&monitor, &state);
        if (status != ALRIOS_EPOCH_OK) {
            (void)fprintf(stderr, "ar_chronod: FAIL-CLOSED: backward time jump detected against anchor!\n");
            if (key_ptr) {
                alrios_explicit_zeroize(auth_key, sizeof(auth_key));
            }
            return EXIT_FAILURE;
        }

        /* Update anchor periodically every 10 ticks */
        if (++tick_counter >= 10U) {
            tick_counter = 0U;
            state.updated_at_ns = monitor.last_realtime_ns;
            state.monotonic_anchor_ns = monitor.last_monotonic_ns;
            (void)alrios_epoch_state_save_file(state_path, key_ptr, &state);
        }
    }

    if (key_ptr) {
        alrios_explicit_zeroize(auth_key, sizeof(auth_key));
    }
    (void)fprintf(stdout, "ar_chronod: shutdown cleanly\n");
    return EXIT_SUCCESS;
}
