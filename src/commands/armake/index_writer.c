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

#include "alrios/package/index.h"
#include <stdio.h>
#include <stdlib.h>

int alrios_armake_write_index(const char *source_dir, const char *output_index_path) {
    if (!source_dir || !output_index_path) {
        return -1;
    }

    alrios_package_index_t index;
    int rc = alrios_index_generate(source_dir, &index);
    if (rc != ALRIOS_INDEX_OK) {
        return rc;
    }

    rc = alrios_index_write(output_index_path, &index);
    if (index.entries) {
        free(index.entries);
    }
    return rc;
}
