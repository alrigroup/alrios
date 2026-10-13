/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/resources/psi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond) do { \
    if (!(cond)) { \
        (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                      __FILE__, __LINE__, #cond); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

typedef struct test_source {
    const char *data;
    size_t length;
    int result;
} test_source_t;

typedef struct notification_log {
    unsigned int calls;
    alrios_psi_notification_t last;
} notification_log_t;

static int inject_source(void *context, char *buffer, size_t capacity,
                         size_t *out_length) {
    test_source_t *source = context;

    if (source->result != ALRIOS_PSI_OK) {
        return source->result;
    }
    if (source->length > capacity) {
        return ALRIOS_PSI_ERR_TOO_LARGE;
    }
    memcpy(buffer, source->data, source->length);
    *out_length = source->length;
    return ALRIOS_PSI_OK;
}

static int record_notification(void *context,
                               const alrios_psi_notification_t *event) {
    notification_log_t *log = context;
    ++log->calls;
    log->last = *event;
    return 0;
}

static alrios_psi_memory_sample_t sample_with_pressure(double percent) {
    alrios_psi_memory_sample_t sample;
    memset(&sample, 0, sizeof(sample));
    sample.some.avg10 = percent;
    return sample;
}

static void test_strict_parser_and_source_injection(void) {
    static const char valid[] =
        "some avg10=80.00 avg60=22.50 avg300=7.25 total=18446744073709551615\n"
        "full avg10=10.00 avg60=2.50 avg300=1.25 total=42\n";
    static const char malformed_missing[] =
        "some avg10=80.00 avg60=22.50 total=1\n"
        "full avg10=10.00 avg60=2.50 avg300=1.25 total=42\n";
    static const char malformed_extra[] =
        "some avg10=80.00 avg60=22.50 avg300=7.25 total=1 surprise=2\n"
        "full avg10=10.00 avg60=2.50 avg300=1.25 total=42\n";
    static const char malformed_range[] =
        "some avg10=100.01 avg60=22.50 avg300=7.25 total=1\n"
        "full avg10=10.00 avg60=2.50 avg300=1.25 total=42\n";
    alrios_psi_memory_sample_t sample;
    test_source_t injected = {valid, sizeof(valid) - 1U, ALRIOS_PSI_OK};
    alrios_psi_source_t source = {inject_source, &injected};

    CHECK(alrios_psi_read_source(&source, &sample) == ALRIOS_PSI_OK);
    CHECK(sample.some.avg10 == 80.0);
    CHECK(sample.some.avg60 == 22.5);
    CHECK(sample.some.avg300 == 7.25);
    CHECK(sample.some.total_us == UINT64_MAX);
    CHECK(sample.full.total_us == 42U);

    CHECK(alrios_psi_parse_memory(malformed_missing,
                                   sizeof(malformed_missing) - 1U,
                                   &sample) == ALRIOS_PSI_ERR_FORMAT);
    CHECK(alrios_psi_parse_memory(malformed_extra,
                                   sizeof(malformed_extra) - 1U,
                                   &sample) == ALRIOS_PSI_ERR_FORMAT);
    CHECK(alrios_psi_parse_memory(malformed_range,
                                   sizeof(malformed_range) - 1U,
                                   &sample) == ALRIOS_PSI_ERR_FORMAT);
    CHECK(alrios_psi_parse_memory(valid, 0U, &sample) ==
           ALRIOS_PSI_ERR_FORMAT);

    injected.result = ALRIOS_PSI_ERR_IO;
    CHECK(alrios_psi_read_source(&source, &sample) == ALRIOS_PSI_ERR_IO);
}

static void test_thresholds_and_notifications(void) {
    alrios_psi_memory_sample_t sample;
    notification_log_t log;
    int status;

    memset(&log, 0, sizeof(log));
    sample = sample_with_pressure(79.99);
    CHECK(alrios_psi_classify_memory(&sample) == ALRIOS_PSI_PRESSURE_NORMAL);
    status = alrios_psi_evaluate(&sample, NULL, 0U, record_notification,
                                 &log, NULL);
    CHECK(status == ALRIOS_PSI_OK);
    CHECK(log.calls == 0U);

    sample = sample_with_pressure(80.0);
    CHECK(alrios_psi_classify_memory(&sample) == ALRIOS_PSI_PRESSURE_WARNING);
    status = alrios_psi_evaluate(&sample, NULL, 0U, record_notification,
                                 &log, NULL);
    CHECK(status == ALRIOS_PSI_OK);
    CHECK(log.calls == 1U);
    CHECK(log.last.type == ALRIOS_PSI_NOTIFY_WARNING);
    CHECK(log.last.candidate == NULL);

    sample = sample_with_pressure(89.99);
    CHECK(alrios_psi_classify_memory(&sample) == ALRIOS_PSI_PRESSURE_WARNING);
    sample = sample_with_pressure(90.0);
    CHECK(alrios_psi_classify_memory(&sample) == ALRIOS_PSI_PRESSURE_SHEDDING);
}

static void test_deterministic_selective_shedding(void) {
    const alrios_psi_candidate_t candidates[] = {
        {"arcore", 1, ALRIOS_PSI_TIER_CORE, 1, 0, 0},
        {"arkernel", 2, ALRIOS_PSI_TIER_BEST_EFFORT, 1, 0, 0},
        {"denied-app", 300, ALRIOS_PSI_TIER_BEST_EFFORT, 1, 1, 0},
        {"protected-app", 301, ALRIOS_PSI_TIER_BEST_EFFORT, 1, 0, 1},
        {"not-allowed", 302, ALRIOS_PSI_TIER_BEST_EFFORT, 0, 0, 0},
        {"worker-a", 200, ALRIOS_PSI_TIER_WORKER, 1, 0, 0},
        {"guest-z", 500, ALRIOS_PSI_TIER_BEST_EFFORT, 1, 0, 0},
        {"guest-a", 501, ALRIOS_PSI_TIER_BEST_EFFORT, 1, 0, 0},
        {"guest-a", 499, ALRIOS_PSI_TIER_BEST_EFFORT, 1, 0, 0}
    };
    const alrios_psi_candidate_t protected_only[] = {
        {"arcore", 1, ALRIOS_PSI_TIER_CORE, 1, 0, 0},
        {"arauth", 2, ALRIOS_PSI_TIER_BEST_EFFORT, 1, 0, 0},
        {"ordinary", 3, ALRIOS_PSI_TIER_BEST_EFFORT, 1, 0, 1}
    };
    alrios_psi_memory_sample_t sample = sample_with_pressure(95.0);
    notification_log_t log;
    size_t selected = 99U;
    int status;

    memset(&log, 0, sizeof(log));
    CHECK(alrios_psi_is_core_daemon("arcore") == 1);
    CHECK(alrios_psi_is_core_daemon("arkernel") == 1);
    CHECK(alrios_psi_is_core_daemon("arauth") == 1);
    CHECK(alrios_psi_is_core_daemon("ardb") == 1);
    CHECK(alrios_psi_is_core_daemon("guest-a") == 0);

    status = alrios_psi_select_candidate(
        candidates, sizeof(candidates) / sizeof(candidates[0]), &selected);
    CHECK(status == ALRIOS_PSI_OK);
    CHECK(selected == 8U);

    status = alrios_psi_evaluate(
        &sample, candidates, sizeof(candidates) / sizeof(candidates[0]),
        record_notification, &log, &selected);
    CHECK(status == ALRIOS_PSI_OK);
    CHECK(log.calls == 1U);
    CHECK(log.last.type == ALRIOS_PSI_NOTIFY_SHED);
    CHECK(log.last.candidate == &candidates[8]);
    CHECK(selected == 8U);

    status = alrios_psi_select_candidate(
        protected_only,
        sizeof(protected_only) / sizeof(protected_only[0]), &selected);
    CHECK(status == ALRIOS_PSI_ERR_NO_CANDIDATE);
}

int main(void) {
    test_strict_parser_and_source_injection();
    test_thresholds_and_notifications();
    test_deterministic_selective_shedding();
    printf("MP-017 memory pressure governance: PASS\n");
    return 0;
}
