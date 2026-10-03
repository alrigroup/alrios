/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_PACKAGE_INDEX_H
#define ALRIOS_PACKAGE_INDEX_H

#include "alrios/arapp_index.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALRIOS_INDEX_OK                0
#define ALRIOS_INDEX_ERR_INVALID_ARG  -8001
#define ALRIOS_INDEX_ERR_MUTABLE_FILE -8002
#define ALRIOS_INDEX_ERR_IO           -8003
#define ALRIOS_INDEX_ERR_SIG_FAIL     -8004
#define ALRIOS_INDEX_ERR_FORMAT       -8005

typedef struct alrios_package_index {
    arapp_index_header_t header;
    arapp_index_entry_t *entries;
    uint8_t signature[64]; /* Ed25519 signature over header + entries */
} alrios_package_index_t;

int alrios_index_is_mutable_extension(const char *path);
int alrios_index_generate(const char *dir_path, alrios_package_index_t *out_index);
int alrios_index_write(const char *file_path, const alrios_package_index_t *index);
int alrios_index_verify(const char *file_path, const uint8_t pubkey[32]);

#ifdef __cplusplus
}
#endif

#endif
