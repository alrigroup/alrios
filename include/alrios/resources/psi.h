/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_RESOURCES_PSI_H
#define ALRIOS_RESOURCES_PSI_H

#include <stddef.h>
#include <stdint.h>

#define ALRIOS_PSI_MEMORY_PATH "/proc/pressure/memory"
#define ALRIOS_PSI_MAX_SOURCE_BYTES 4096U
#define ALRIOS_PSI_WARNING_PERCENT 80.0
#define ALRIOS_PSI_SHEDDING_PERCENT 90.0

typedef enum alrios_psi_status {
    ALRIOS_PSI_OK = 0,
    ALRIOS_PSI_ERR_ARGUMENT = -1,
    ALRIOS_PSI_ERR_IO = -2,
    ALRIOS_PSI_ERR_FORMAT = -3,
    ALRIOS_PSI_ERR_TOO_LARGE = -4,
    ALRIOS_PSI_ERR_UNSUPPORTED = -5,
    ALRIOS_PSI_ERR_NO_CANDIDATE = -6,
    ALRIOS_PSI_ERR_NOTIFY = -7
} alrios_psi_status_t;

typedef struct alrios_psi_values {
    double avg10;
    double avg60;
    double avg300;
    uint64_t total_us;
} alrios_psi_values_t;

typedef struct alrios_psi_memory_sample {
    alrios_psi_values_t some;
    alrios_psi_values_t full;
} alrios_psi_memory_sample_t;

typedef int (*alrios_psi_read_fn)(void *context, char *buffer,
                                  size_t capacity, size_t *out_length);

typedef struct alrios_psi_source {
    alrios_psi_read_fn read;
    void *context;
} alrios_psi_source_t;

typedef enum alrios_psi_pressure_level {
    ALRIOS_PSI_PRESSURE_NORMAL = 0,
    ALRIOS_PSI_PRESSURE_WARNING = 1,
    ALRIOS_PSI_PRESSURE_SHEDDING = 2
} alrios_psi_pressure_level_t;

typedef enum alrios_psi_tier {
    ALRIOS_PSI_TIER_CORE = 0,
    ALRIOS_PSI_TIER_WORKER = 1,
    ALRIOS_PSI_TIER_BEST_EFFORT = 2
} alrios_psi_tier_t;

typedef struct alrios_psi_candidate {
    const char *name;
    int process_id;
    alrios_psi_tier_t tier;
    int allow_shedding;
    int deny_shedding;
    int protected_process;
} alrios_psi_candidate_t;

typedef enum alrios_psi_notification_type {
    ALRIOS_PSI_NOTIFY_WARNING = 1,
    ALRIOS_PSI_NOTIFY_SHED = 2
} alrios_psi_notification_type_t;

typedef struct alrios_psi_notification {
    alrios_psi_notification_type_t type;
    alrios_psi_pressure_level_t level;
    double pressure_percent;
    const alrios_psi_candidate_t *candidate;
} alrios_psi_notification_t;

typedef int (*alrios_psi_notify_fn)(void *context,
                                    const alrios_psi_notification_t *event);

int alrios_psi_parse_memory(const char *data, size_t length,
                            alrios_psi_memory_sample_t *out_sample);
int alrios_psi_read_source(const alrios_psi_source_t *source,
                           alrios_psi_memory_sample_t *out_sample);
int alrios_psi_read_memory_path(const char *path,
                                alrios_psi_memory_sample_t *out_sample);
int alrios_psi_read_memory(alrios_psi_memory_sample_t *out_sample);

double alrios_psi_memory_pressure_percent(
    const alrios_psi_memory_sample_t *sample);
alrios_psi_pressure_level_t alrios_psi_classify_memory(
    const alrios_psi_memory_sample_t *sample);

int alrios_psi_is_core_daemon(const char *name);
int alrios_psi_select_candidate(const alrios_psi_candidate_t *candidates,
                                size_t candidate_count,
                                size_t *out_candidate_index);
int alrios_psi_evaluate(const alrios_psi_memory_sample_t *sample,
                        const alrios_psi_candidate_t *candidates,
                        size_t candidate_count,
                        alrios_psi_notify_fn notify,
                        void *notify_context,
                        size_t *out_candidate_index);

const char *alrios_psi_status_string(int status);

#endif
