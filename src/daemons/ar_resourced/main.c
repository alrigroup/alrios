/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/resources/psi.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define AR_RESOURCED_DEFAULT_INTERVAL_MS 1000U
#define AR_RESOURCED_MAX_INTERVAL_MS 60000U

static volatile sig_atomic_t keep_running = 1;

static void handle_signal(int signal_number) {
    (void)signal_number;
    keep_running = 0;
}

static int parse_interval(const char *value, unsigned int *out_interval_ms) {
    char *end = NULL;
    unsigned long parsed;

    if (value == NULL || out_interval_ms == NULL || value[0] == '\0' ||
        value[0] == '-') {
        return -1;
    }
    errno = 0;
    parsed = strtoul(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed == 0UL ||
        parsed > AR_RESOURCED_MAX_INTERVAL_MS) {
        return -1;
    }
    *out_interval_ms = (unsigned int)parsed;
    return 0;
}

static int sleep_interval(unsigned int interval_ms) {
    struct timespec requested;
    struct timespec remaining;

    requested.tv_sec = (time_t)(interval_ms / 1000U);
    requested.tv_nsec = (long)((interval_ms % 1000U) * 1000000U);
    while (keep_running && nanosleep(&requested, &remaining) != 0) {
        if (errno != EINTR) {
            return -1;
        }
        requested = remaining;
    }
    return 0;
}

static int emit_notification(void *context,
                             const alrios_psi_notification_t *event) {
    FILE *output = context;

    if (output == NULL || event == NULL) {
        return -1;
    }
    if (event->type == ALRIOS_PSI_NOTIFY_WARNING) {
        if (event->candidate != NULL) {
            return fprintf(output,
                           "ar_resourced: memory pressure warning %.2f%% "
                           "(threshold %.0f%%) dispatched to %s pid=%d tier=%d\n",
                           event->pressure_percent,
                           ALRIOS_PSI_WARNING_PERCENT,
                           event->candidate->name,
                           event->candidate->process_id,
                           (int)event->candidate->tier) < 0 ? -1 : 0;
        }
        return fprintf(output,
                       "ar_resourced: memory pressure warning %.2f%% "
                       "(threshold %.0f%%)\n",
                       event->pressure_percent,
                       ALRIOS_PSI_WARNING_PERCENT) < 0 ? -1 : 0;
    }
    if (event->type == ALRIOS_PSI_NOTIFY_SHED && event->candidate != NULL) {
        return fprintf(output,
                       "ar_resourced: selective shedding requested for "
                       "%s pid=%d tier=%d at %.2f%% pressure\n",
                       event->candidate->name,
                       event->candidate->process_id,
                       (int)event->candidate->tier,
                       event->pressure_percent) < 0 ? -1 : 0;
    }
    return -1;
}

static void print_usage(const char *program) {
    fprintf(stderr,
            "Usage: %s [--once] [--interval-ms N] [--psi-file PATH]\n",
            program);
}

int main(int argc, char **argv) {
    const char *psi_path = ALRIOS_PSI_MEMORY_PATH;
    unsigned int interval_ms = AR_RESOURCED_DEFAULT_INTERVAL_MS;
    alrios_psi_pressure_level_t previous_level = ALRIOS_PSI_PRESSURE_NORMAL;
    int run_once = 0;
    int index;

#ifndef __linux__
    (void)argc;
    (void)argv;
    fprintf(stderr, "ar_resourced: Linux PSI support is required\n");
    return EXIT_FAILURE;
#endif

    for (index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--once") == 0) {
            run_once = 1;
        } else if (strcmp(argv[index], "--interval-ms") == 0) {
            if (++index >= argc || parse_interval(argv[index], &interval_ms) != 0) {
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[index], "--psi-file") == 0) {
            if (++index >= argc || argv[index][0] == '\0') {
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
            psi_path = argv[index];
        } else {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (signal(SIGINT, handle_signal) == SIG_ERR ||
        signal(SIGTERM, handle_signal) == SIG_ERR) {
        fprintf(stderr, "ar_resourced: failed to install signal handlers\n");
        return EXIT_FAILURE;
    }

    while (keep_running) {
        alrios_psi_memory_sample_t sample;
        alrios_psi_pressure_level_t level;
        int status = alrios_psi_read_memory_path(psi_path, &sample);

        if (status != ALRIOS_PSI_OK) {
            fprintf(stderr, "ar_resourced: cannot monitor %s: %s\n",
                    psi_path, alrios_psi_status_string(status));
            return EXIT_FAILURE;
        }
        level = alrios_psi_classify_memory(&sample);
        if (level != previous_level && level != ALRIOS_PSI_PRESSURE_NORMAL) {
            status = alrios_psi_evaluate(&sample, NULL, 0U,
                                         emit_notification, stderr, NULL);
            if (status != ALRIOS_PSI_OK &&
                status != ALRIOS_PSI_ERR_NO_CANDIDATE) {
                fprintf(stderr, "ar_resourced: %s\n",
                        alrios_psi_status_string(status));
                return EXIT_FAILURE;
            }
            if (status == ALRIOS_PSI_ERR_NO_CANDIDATE) {
                fprintf(stderr,
                        "ar_resourced: shedding threshold reached, but no "
                        "eligible process inventory was supplied\n");
            }
        }
        previous_level = level;
        if (run_once) {
            break;
        }
        if (sleep_interval(interval_ms) != 0) {
            fprintf(stderr, "ar_resourced: monitor sleep failed: %s\n",
                    strerror(errno));
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
