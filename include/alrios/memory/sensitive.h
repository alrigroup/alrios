/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_MEMORY_SENSITIVE_H
#define ALRIOS_MEMORY_SENSITIVE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALRIOS_SENSITIVE_OK                   0
#define ALRIOS_SENSITIVE_ERR_INVALID_ARGUMENT -9001
#define ALRIOS_SENSITIVE_ERR_CAPACITY         -9002
#define ALRIOS_SENSITIVE_ERR_NOT_FOUND        -9003
#define ALRIOS_SENSITIVE_ERR_SYSTEM           -9004

#define ALRIOS_SENSITIVE_MAX_REGISTRATIONS    256U

typedef struct alrios_sensitive_range {
    void *ptr;
    size_t size;
} alrios_sensitive_range_t;

/*
 * Registers a sensitive memory range.
 * On Linux, applies madvise(MADV_DONTDUMP) to the enclosing pages so that
 * standard core dumps exclude this memory.
 * Records the range in a thread-safe registry.
 */
int alrios_sensitive_register(void *ptr, size_t size);

/*
 * Unregisters a sensitive memory range.
 * Explicitly zeroizes the memory using alrios_explicit_zeroize before removing.
 * If Linux madvise(MADV_DONTDUMP) was applied, attempts madvise(MADV_DODUMP).
 */
int alrios_sensitive_unregister(void *ptr, size_t size);

/*
 * Checks if a pointer/size range overlaps with any registered sensitive range.
 * Returns true if an overlap is found, false otherwise.
 */
bool alrios_sensitive_is_registered(const void *ptr, size_t size);

/*
 * Queries registered sensitive ranges.
 * Copies up to max_ranges entries into out_ranges.
 * If out_count is non-NULL, sets it to total registered ranges.
 */
int alrios_sensitive_get_ranges(alrios_sensitive_range_t *out_ranges,
                                size_t max_ranges,
                                size_t *out_count);

/*
 * Scrubs (zeroizes) all currently registered sensitive memory ranges in the
 * calling process.
 */
void alrios_sensitive_scrub_all(void);

/*
 * Clears and zeroizes all registered sensitive ranges from the registry.
 */
void alrios_sensitive_clear_all(void);

#ifdef __cplusplus
}
#endif

#endif /* ALRIOS_MEMORY_SENSITIVE_H */
