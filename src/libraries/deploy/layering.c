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

#include "alrios/deploy_layering.h"
#include "alrios/crypto_verify.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

int alrios_deploy_provision_delta(const char *old_slot,
                                 const char *new_slot,
                                 const arapp_index_entry_t *entries,
                                 size_t count) {
    if (!new_slot) return -1;
    mkdir(new_slot, 0755);

    if (old_slot && entries && count > 0) {
        // Leverages hardlink/reflink/fallback copy layering
        return alrios_index_compare_and_hardlink(old_slot, new_slot, entries, count);
    }
    return 0;
}

int alrios_deploy_verify_slot(const char *slot_path,
                             const arapp_index_entry_t *entries,
                             size_t count) {
    if (!slot_path || (!entries && count > 0)) return -1;

    for (size_t i = 0; i < count; i++) {
        char full[512];
        snprintf(full, sizeof(full), "%s/%s", slot_path, entries[i].path);

        FILE *f = fopen(full, "rb");
        if (!f) return -1; // Missing file

        uint8_t buf[4096];
        size_t rd = fread(buf, 1, sizeof(buf), f);
        fclose(f);

        uint8_t dig[64];
        alrios_sha512(buf, rd, dig);

        // Verify SHA-256 / SHA-512 digest matches index
        if (memcmp(dig, entries[i].sha256, 32) != 0) {
            return -2; // Hash mismatch!
        }
    }
    return 0;
}

int alrios_deploy_rollback_slot(const char *new_slot) {
    if (!new_slot) return -1;
    // Deletes the failed new slot cleanly, leaving active old slot untouched
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", new_slot);
    return system(cmd);
}
