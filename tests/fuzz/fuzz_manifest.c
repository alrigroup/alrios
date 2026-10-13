/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "arapp_parser.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size == 0 || size > 65536) {
        return 0;
    }

    char *json = (char *)malloc(size + 1U);
    if (!json) {
        return 0;
    }
    memcpy(json, data, size);
    json[size] = '\0';

    ar_app_manifest_t manifest;
    memset(&manifest, 0, sizeof(manifest));
    int rc = ar_manifest_parse(json, &manifest);
    if (rc == 0) {
        char out_entry[AR_ENTRY_MAX];
        memset(out_entry, 0, sizeof(out_entry));
        (void)ar_manifest_get_platform_entry(&manifest, "linux-x86_64", out_entry, sizeof(out_entry));
        (void)ar_manifest_get_platform_entry(&manifest, "windows-x86_64", out_entry, sizeof(out_entry));
    }

    free(json);
    return 0;
}
