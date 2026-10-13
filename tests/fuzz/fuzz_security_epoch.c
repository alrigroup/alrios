/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/policy/security_epoch.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static const uint8_t fuzz_key[ALRIOS_EPOCH_KEY_SIZE] = {
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
        0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f
    };

    if (size >= ALRIOS_EPOCH_WIRE_SIZE) {
        alrios_epoch_state_t state;
        memset(&state, 0, sizeof(state));

        /* Fuzz wire decoding with HMAC verification */
        int rc = alrios_epoch_state_decode(data, fuzz_key, &state);
        if (rc == ALRIOS_EPOCH_OK) {
            uint8_t out_wire[ALRIOS_EPOCH_WIRE_SIZE];
            if (alrios_epoch_state_encode(&state, fuzz_key, out_wire) == ALRIOS_EPOCH_OK) {
                alrios_epoch_state_t roundtrip;
                if (alrios_epoch_state_decode(out_wire, fuzz_key, &roundtrip) != ALRIOS_EPOCH_OK) {
                    abort();
                }
            }
        }
    }

    if (size >= sizeof(uint64_t) * 3) {
        uint64_t step_threshold;
        uint64_t realtime_ns;
        uint64_t monotonic_ns;
        memcpy(&step_threshold, data, sizeof(uint64_t));
        memcpy(&realtime_ns, data + sizeof(uint64_t), sizeof(uint64_t));
        memcpy(&monotonic_ns, data + sizeof(uint64_t) * 2, sizeof(uint64_t));

        alrios_time_monitor_t mon;
        memset(&mon, 0, sizeof(mon));
        if (alrios_time_monitor_init(&mon, step_threshold) == ALRIOS_EPOCH_OK) {
            (void)alrios_time_monitor_update(&mon, realtime_ns, monotonic_ns);
            (void)alrios_time_monitor_sample(&mon);
        }
    }

    return 0;
}
