/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/resources/psi.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALRIOS_PSI_FIELD_COUNT 4U

typedef struct alrios_psi_file_source {
    const char *path;
} alrios_psi_file_source_t;

static int parse_double_field(const char *value, size_t length,
                              double *out_value) {
    char temporary[64];
    char *end = NULL;
    double parsed;

    if (value == NULL || out_value == NULL || length == 0U ||
        length >= sizeof(temporary)) {
        return ALRIOS_PSI_ERR_FORMAT;
    }
    {
        size_t index;
        int decimal_seen = 0;
        for (index = 0U; index < length; ++index) {
            if (value[index] == '.') {
                if (decimal_seen || index == 0U || index + 1U == length) {
                    return ALRIOS_PSI_ERR_FORMAT;
                }
                decimal_seen = 1;
            } else if (!isdigit((unsigned char)value[index])) {
                return ALRIOS_PSI_ERR_FORMAT;
            }
        }
    }
    memcpy(temporary, value, length);
    temporary[length] = '\0';
    errno = 0;
    parsed = strtod(temporary, &end);
    if (errno != 0 || end != temporary + length || !isfinite(parsed) ||
        parsed < 0.0 || parsed > 100.0) {
        return ALRIOS_PSI_ERR_FORMAT;
    }
    *out_value = parsed;
    return ALRIOS_PSI_OK;
}

static int parse_total_field(const char *value, size_t length,
                             uint64_t *out_value) {
    uint64_t parsed = 0U;
    size_t index;

    if (value == NULL || out_value == NULL || length == 0U) {
        return ALRIOS_PSI_ERR_FORMAT;
    }
    for (index = 0U; index < length; ++index) {
        unsigned int digit;
        if (!isdigit((unsigned char)value[index])) {
            return ALRIOS_PSI_ERR_FORMAT;
        }
        digit = (unsigned int)(value[index] - '0');
        if (parsed > (UINT64_MAX - digit) / 10U) {
            return ALRIOS_PSI_ERR_FORMAT;
        }
        parsed = (parsed * 10U) + digit;
    }
    *out_value = parsed;
    return ALRIOS_PSI_OK;
}

static int parse_psi_line(const char *line, size_t length,
                          const char *expected_name,
                          alrios_psi_values_t *out_values) {
    static const char *const field_names[ALRIOS_PSI_FIELD_COUNT] = {
        "avg10", "avg60", "avg300", "total"
    };
    int seen[ALRIOS_PSI_FIELD_COUNT] = {0, 0, 0, 0};
    size_t expected_length;
    size_t position;

    if (line == NULL || expected_name == NULL || out_values == NULL) {
        return ALRIOS_PSI_ERR_ARGUMENT;
    }
    expected_length = strlen(expected_name);
    if (length <= expected_length ||
        memcmp(line, expected_name, expected_length) != 0 ||
        line[expected_length] != ' ') {
        return ALRIOS_PSI_ERR_FORMAT;
    }
    memset(out_values, 0, sizeof(*out_values));
    position = expected_length + 1U;

    while (position < length) {
        size_t token_start = position;
        size_t separator;
        size_t token_end;
        size_t field_index;
        int matched = 0;
        int status;

        if (line[position] == ' ') {
            return ALRIOS_PSI_ERR_FORMAT;
        }
        while (position < length && line[position] != ' ') {
            if (line[position] == '\t' || line[position] == '\r' ||
                line[position] == '\0') {
                return ALRIOS_PSI_ERR_FORMAT;
            }
            ++position;
        }
        token_end = position;
        separator = token_start;
        while (separator < token_end && line[separator] != '=') {
            ++separator;
        }
        if (separator == token_start || separator + 1U >= token_end) {
            return ALRIOS_PSI_ERR_FORMAT;
        }

        for (field_index = 0U; field_index < ALRIOS_PSI_FIELD_COUNT;
             ++field_index) {
            size_t field_length = strlen(field_names[field_index]);
            if (separator - token_start == field_length &&
                memcmp(line + token_start, field_names[field_index],
                       field_length) == 0) {
                if (seen[field_index]) {
                    return ALRIOS_PSI_ERR_FORMAT;
                }
                seen[field_index] = 1;
                matched = 1;
                if (field_index == 3U) {
                    status = parse_total_field(line + separator + 1U,
                                               token_end - separator - 1U,
                                               &out_values->total_us);
                } else {
                    double *target = field_index == 0U ? &out_values->avg10 :
                                     field_index == 1U ? &out_values->avg60 :
                                                        &out_values->avg300;
                    status = parse_double_field(line + separator + 1U,
                                                token_end - separator - 1U,
                                                target);
                }
                if (status != ALRIOS_PSI_OK) {
                    return status;
                }
                break;
            }
        }
        if (!matched) {
            return ALRIOS_PSI_ERR_FORMAT;
        }
        if (position < length) {
            ++position;
            if (position == length) {
                return ALRIOS_PSI_ERR_FORMAT;
            }
        }
    }

    for (position = 0U; position < ALRIOS_PSI_FIELD_COUNT; ++position) {
        if (!seen[position]) {
            return ALRIOS_PSI_ERR_FORMAT;
        }
    }
    return ALRIOS_PSI_OK;
}

int alrios_psi_parse_memory(const char *data, size_t length,
                            alrios_psi_memory_sample_t *out_sample) {
    size_t first_end = 0U;
    size_t second_start;
    size_t second_end;
    int status;

    if (data == NULL || out_sample == NULL) {
        return ALRIOS_PSI_ERR_ARGUMENT;
    }
    if (length == 0U || length > ALRIOS_PSI_MAX_SOURCE_BYTES) {
        return length > ALRIOS_PSI_MAX_SOURCE_BYTES ?
               ALRIOS_PSI_ERR_TOO_LARGE : ALRIOS_PSI_ERR_FORMAT;
    }
    if (memchr(data, '\0', length) != NULL) {
        return ALRIOS_PSI_ERR_FORMAT;
    }
    while (first_end < length && data[first_end] != '\n') {
        ++first_end;
    }
    if (first_end == length || first_end == 0U) {
        return ALRIOS_PSI_ERR_FORMAT;
    }
    second_start = first_end + 1U;
    second_end = second_start;
    while (second_end < length && data[second_end] != '\n') {
        ++second_end;
    }
    if (second_end == second_start) {
        return ALRIOS_PSI_ERR_FORMAT;
    }
    if (second_end < length) {
        if (second_end + 1U != length) {
            return ALRIOS_PSI_ERR_FORMAT;
        }
    } else if (second_end != length) {
        return ALRIOS_PSI_ERR_FORMAT;
    }

    memset(out_sample, 0, sizeof(*out_sample));
    status = parse_psi_line(data, first_end, "some", &out_sample->some);
    if (status != ALRIOS_PSI_OK) {
        return status;
    }
    status = parse_psi_line(data + second_start, second_end - second_start,
                            "full", &out_sample->full);
    if (status != ALRIOS_PSI_OK) {
        memset(out_sample, 0, sizeof(*out_sample));
    }
    return status;
}

static int read_file_source(void *context, char *buffer, size_t capacity,
                            size_t *out_length) {
    alrios_psi_file_source_t *source = context;
    FILE *file;
    size_t length;

    if (source == NULL || source->path == NULL || buffer == NULL ||
        out_length == NULL || capacity < 2U) {
        return ALRIOS_PSI_ERR_ARGUMENT;
    }
    file = fopen(source->path, "rb");
    if (file == NULL) {
#ifdef __linux__
        return errno == ENOENT ? ALRIOS_PSI_ERR_UNSUPPORTED : ALRIOS_PSI_ERR_IO;
#else
        return ALRIOS_PSI_ERR_UNSUPPORTED;
#endif
    }
    length = fread(buffer, 1U, capacity, file);
    if (ferror(file)) {
        (void)fclose(file);
        return ALRIOS_PSI_ERR_IO;
    }
    if (length == capacity) {
        int extra = fgetc(file);
        if (extra != EOF) {
            (void)fclose(file);
            return ALRIOS_PSI_ERR_TOO_LARGE;
        }
        if (ferror(file)) {
            (void)fclose(file);
            return ALRIOS_PSI_ERR_IO;
        }
    }
    if (fclose(file) != 0) {
        return ALRIOS_PSI_ERR_IO;
    }
    *out_length = length;
    return ALRIOS_PSI_OK;
}

int alrios_psi_read_source(const alrios_psi_source_t *source,
                           alrios_psi_memory_sample_t *out_sample) {
    char buffer[ALRIOS_PSI_MAX_SOURCE_BYTES];
    size_t length = 0U;
    int status;

    if (source == NULL || source->read == NULL || out_sample == NULL) {
        return ALRIOS_PSI_ERR_ARGUMENT;
    }
    status = source->read(source->context, buffer, sizeof(buffer), &length);
    if (status != ALRIOS_PSI_OK) {
        return status;
    }
    if (length > sizeof(buffer)) {
        return ALRIOS_PSI_ERR_TOO_LARGE;
    }
    return alrios_psi_parse_memory(buffer, length, out_sample);
}

int alrios_psi_read_memory_path(const char *path,
                                alrios_psi_memory_sample_t *out_sample) {
    alrios_psi_file_source_t file_source;
    alrios_psi_source_t source;

    if (path == NULL || out_sample == NULL) {
        return ALRIOS_PSI_ERR_ARGUMENT;
    }
    file_source.path = path;
    source.read = read_file_source;
    source.context = &file_source;
    return alrios_psi_read_source(&source, out_sample);
}

int alrios_psi_read_memory(alrios_psi_memory_sample_t *out_sample) {
#ifdef __linux__
    return alrios_psi_read_memory_path(ALRIOS_PSI_MEMORY_PATH, out_sample);
#else
    (void)out_sample;
    return ALRIOS_PSI_ERR_UNSUPPORTED;
#endif
}

double alrios_psi_memory_pressure_percent(
    const alrios_psi_memory_sample_t *sample) {
    if (sample == NULL) {
        return 0.0;
    }
    return sample->some.avg10;
}

alrios_psi_pressure_level_t alrios_psi_classify_memory(
    const alrios_psi_memory_sample_t *sample) {
    double pressure = alrios_psi_memory_pressure_percent(sample);
    if (pressure >= ALRIOS_PSI_SHEDDING_PERCENT) {
        return ALRIOS_PSI_PRESSURE_SHEDDING;
    }
    if (pressure >= ALRIOS_PSI_WARNING_PERCENT) {
        return ALRIOS_PSI_PRESSURE_WARNING;
    }
    return ALRIOS_PSI_PRESSURE_NORMAL;
}

static const char *extract_basename(const char *path) {
    const char *slash;
    if (path == NULL) {
        return NULL;
    }
    slash = strrchr(path, '/');
    return (slash != NULL) ? slash + 1 : path;
}

int alrios_psi_is_core_daemon(const char *name) {
    static const char *const protected_names[] = {
        "arcore", "arkernel", "arauth", "ardb", "ar_resourced",
        "supervisor", "router", "guest_router", "ar_supervisor", "ar_router",
        "ar_auditd", "ar_chronod", "ar_cored", "ar_eventd"
    };
    const char *base;
    size_t index;

    if (name == NULL || name[0] == '\0') {
        return 0;
    }
    base = extract_basename(name);
    for (index = 0U;
         index < sizeof(protected_names) / sizeof(protected_names[0]);
         ++index) {
        if (strcmp(base, protected_names[index]) == 0) {
            return 1;
        }
    }
    return 0;
}

static int candidate_is_eligible(const alrios_psi_candidate_t *candidate) {
    return candidate != NULL && candidate->name != NULL &&
           candidate->name[0] != '\0' && candidate->process_id > 0 &&
           candidate->allow_shedding && !candidate->deny_shedding &&
           !candidate->protected_process &&
           candidate->tier != ALRIOS_PSI_TIER_CORE &&
           !alrios_psi_is_core_daemon(candidate->name);
}

static int candidate_is_worker(const alrios_psi_candidate_t *candidate) {
    return candidate != NULL && candidate->name != NULL &&
           candidate->name[0] != '\0' && candidate->process_id > 0 &&
           !candidate->protected_process &&
           candidate->tier != ALRIOS_PSI_TIER_CORE &&
           !alrios_psi_is_core_daemon(candidate->name);
}

static int compare_candidate_priority(const alrios_psi_candidate_t *a,
                                      const alrios_psi_candidate_t *b) {
    if (a->tier > b->tier) {
        return -1;
    }
    if (a->tier < b->tier) {
        return 1;
    }
    {
        int name_cmp = strcmp(a->name, b->name);
        if (name_cmp != 0) {
            return name_cmp;
        }
    }
    if (a->process_id < b->process_id) {
        return -1;
    }
    if (a->process_id > b->process_id) {
        return 1;
    }
    return 0;
}

int alrios_psi_select_candidate(const alrios_psi_candidate_t *candidates,
                                size_t candidate_count,
                                size_t *out_candidate_index) {
    size_t best_index = 0U;
    int have_best = 0;
    size_t index;

    if (candidates == NULL || candidate_count == 0U ||
        out_candidate_index == NULL) {
        return ALRIOS_PSI_ERR_ARGUMENT;
    }
    for (index = 0U; index < candidate_count; ++index) {
        const alrios_psi_candidate_t *candidate = &candidates[index];
        const alrios_psi_candidate_t *best;
        if (!candidate_is_eligible(candidate)) {
            continue;
        }
        if (!have_best) {
            best_index = index;
            have_best = 1;
            continue;
        }
        best = &candidates[best_index];
        if (compare_candidate_priority(candidate, best) < 0) {
            best_index = index;
        }
    }
    if (!have_best) {
        return ALRIOS_PSI_ERR_NO_CANDIDATE;
    }
    *out_candidate_index = best_index;
    return ALRIOS_PSI_OK;
}

int alrios_psi_evaluate(const alrios_psi_memory_sample_t *sample,
                        const alrios_psi_candidate_t *candidates,
                        size_t candidate_count,
                        alrios_psi_notify_fn notify,
                        void *notify_context,
                        size_t *out_candidate_index) {
    alrios_psi_notification_t event;
    alrios_psi_pressure_level_t level;
    size_t candidate_index = 0U;
    int status;

    if (sample == NULL || notify == NULL) {
        return ALRIOS_PSI_ERR_ARGUMENT;
    }
    level = alrios_psi_classify_memory(sample);
    if (level == ALRIOS_PSI_PRESSURE_NORMAL) {
        return ALRIOS_PSI_OK;
    }
    event.type = level == ALRIOS_PSI_PRESSURE_SHEDDING ?
                 ALRIOS_PSI_NOTIFY_SHED : ALRIOS_PSI_NOTIFY_WARNING;
    event.level = level;
    event.pressure_percent = alrios_psi_memory_pressure_percent(sample);
    event.candidate = NULL;

    if (level == ALRIOS_PSI_PRESSURE_SHEDDING) {
        status = alrios_psi_select_candidate(candidates, candidate_count,
                                             &candidate_index);
        if (status != ALRIOS_PSI_OK) {
            return status;
        }
        event.candidate = &candidates[candidate_index];
        if (out_candidate_index != NULL) {
            *out_candidate_index = candidate_index;
        }
        if (notify(notify_context, &event) != 0) {
            return ALRIOS_PSI_ERR_NOTIFY;
        }
        return ALRIOS_PSI_OK;
    }

    if (candidates != NULL && candidate_count > 0U) {
        size_t stack_indices[64];
        size_t *worker_indices = stack_indices;
        size_t worker_count = 0U;
        size_t i;

        if (candidate_count > sizeof(stack_indices) / sizeof(stack_indices[0])) {
            worker_indices = (size_t *)malloc(candidate_count * sizeof(size_t));
            if (worker_indices == NULL) {
                return ALRIOS_PSI_ERR_IO;
            }
        }

        for (i = 0U; i < candidate_count; ++i) {
            const alrios_psi_candidate_t *c = &candidates[i];
            if (candidate_is_worker(c)) {
                worker_indices[worker_count++] = i;
            }
        }

        if (worker_count > 0U) {
            for (i = 1U; i < worker_count; ++i) {
                size_t key_idx = worker_indices[i];
                const alrios_psi_candidate_t *key = &candidates[key_idx];
                size_t j = i;
                while (j > 0U) {
                    const alrios_psi_candidate_t *prev =
                        &candidates[worker_indices[j - 1U]];
                    if (compare_candidate_priority(key, prev) < 0) {
                        worker_indices[j] = worker_indices[j - 1U];
                        --j;
                    } else {
                        break;
                    }
                }
                worker_indices[j] = key_idx;
            }

            if (out_candidate_index != NULL) {
                *out_candidate_index = worker_indices[0];
            }

            for (i = 0U; i < worker_count; ++i) {
                event.candidate = &candidates[worker_indices[i]];
                if (notify(notify_context, &event) != 0) {
                    if (worker_indices != stack_indices) {
                        free(worker_indices);
                    }
                    return ALRIOS_PSI_ERR_NOTIFY;
                }
            }

            if (worker_indices != stack_indices) {
                free(worker_indices);
            }
            return ALRIOS_PSI_OK;
        }

        if (worker_indices != stack_indices) {
            free(worker_indices);
        }
    }

    if (notify(notify_context, &event) != 0) {
        return ALRIOS_PSI_ERR_NOTIFY;
    }
    return ALRIOS_PSI_OK;
}

const char *alrios_psi_status_string(int status) {
    switch (status) {
    case ALRIOS_PSI_OK:
        return "ok";
    case ALRIOS_PSI_ERR_ARGUMENT:
        return "invalid argument";
    case ALRIOS_PSI_ERR_IO:
        return "I/O failure";
    case ALRIOS_PSI_ERR_FORMAT:
        return "malformed PSI source";
    case ALRIOS_PSI_ERR_TOO_LARGE:
        return "PSI source exceeds limit";
    case ALRIOS_PSI_ERR_UNSUPPORTED:
        return "Linux memory PSI is unavailable";
    case ALRIOS_PSI_ERR_NO_CANDIDATE:
        return "no eligible shedding candidate";
    case ALRIOS_PSI_ERR_NOTIFY:
        return "pressure notification failed";
    default:
        return "unknown PSI error";
    }
}
