/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_ARMAKE_PACKAGE_V2_H
#define ALRIOS_ARMAKE_PACKAGE_V2_H

#include <stdint.h>

#define ALRIOS_ARMAKE_V2_OK                   0
#define ALRIOS_ARMAKE_V2_ERR_ARGUMENT        -3201
#define ALRIOS_ARMAKE_V2_ERR_IO              -3202
#define ALRIOS_ARMAKE_V2_ERR_MANIFEST        -3203
#define ALRIOS_ARMAKE_V2_ERR_MUTABLE_FILE    -3204
#define ALRIOS_ARMAKE_V2_ERR_POLICY          -3205
#define ALRIOS_ARMAKE_V2_ERR_CAPACITY        -3206
#define ALRIOS_ARMAKE_V2_ERR_OUTPUT_EXISTS   -3207

#define ALRIOS_ARMAKE_V2_DEFAULT_ISSUED_AT 1ULL
#define ALRIOS_ARMAKE_V2_DEFAULT_EXPIRES_AT UINT64_MAX

typedef struct alrios_armake_v2_options {
    uint16_t target_arch;
    uint32_t container_flags;
    uint64_t issued_at;
    uint64_t expires_at;
} alrios_armake_v2_options_t;

int alrios_armake_package_v2(
    const char *manifest_path,
    const char *payload_path,
    const char *output_path,
    const alrios_armake_v2_options_t *options
);

const char *alrios_armake_v2_error_string(int status);

#endif /* ALRIOS_ARMAKE_PACKAGE_V2_H */
