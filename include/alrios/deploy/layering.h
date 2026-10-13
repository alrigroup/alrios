/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_DEPLOY_LAYERING_H
#define ALRIOS_DEPLOY_LAYERING_H

#include "alrios/package/index.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALRIOS_LAYERING_OK 0
#define ALRIOS_LAYERING_ERR_ARGUMENT -5001
#define ALRIOS_LAYERING_ERR_IO       -5002
#define ALRIOS_LAYERING_ERR_PATH     -5003
#define ALRIOS_LAYERING_ERR_HASH     -5004
#define ALRIOS_LAYERING_ERR_EXISTS   -5005
#define ALRIOS_LAYERING_ERR_ROLLBACK -5006

typedef struct alrios_layering_stats {
    uint64_t hardlinked_files;
    uint64_t reflinked_files;
    uint64_t copied_files;
    uint64_t installed_bytes;
} alrios_layering_stats_t;

/*
 * Provisions target_directory from payload_directory in a sibling staging
 * directory and publishes it only after every indexed file has been verified.
 * Files matching existing_index are sourced from existing_directory through
 * hardlink, reflink, then buffered-copy fallbacks. A NULL existing_index makes
 * every indexed file come from payload_directory. Parsed indexes must already
 * have passed signature verification.
 */
int alrios_layering_install(const char *existing_directory,
                            const alrios_package_index_t *existing_index,
                            const char *payload_directory,
                            const alrios_package_index_t *target_index,
                            const char *target_directory,
                            alrios_layering_stats_t *out_stats);

const char *alrios_layering_error_string(int code);

#ifdef __cplusplus
}
#endif

#endif /* ALRIOS_DEPLOY_LAYERING_H */
