/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/crash_report.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static const uint8_t fuzz_key[ALRIOS_CRASH_KEY_LEN] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20
    };

    alrios_crash_state_t decrypted_state;
    memset(&decrypted_state, 0, sizeof(decrypted_state));

    /* Fuzz decrypting untrusted container */
    int rc = alrios_crash_report_decrypt(data, size, fuzz_key, &decrypted_state);
    if (rc == 0) {
        /* If decrypt succeeded, test re-encryption */
        uint8_t *re_container = NULL;
        size_t re_len = 0;
        if (alrios_crash_report_encrypt(&decrypted_state, fuzz_key, &re_container, &re_len) == 0) {
            free(re_container);
        }
    }

    return 0;
}
