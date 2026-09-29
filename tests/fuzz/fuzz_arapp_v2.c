/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/package/arapp_v2.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    arapp_v2_package_t pkg;
    arapp_v2_package_t roundtrip_pkg;
    uint8_t wire[65536];
    size_t wire_len = 0U;
    uint8_t digest[ARAPP_V2_SHA512_LEN];

    if (!data || size == 0U) {
        return 0;
    }

    (void)arapp_v2_read_header(data, size, &pkg);
    (void)arapp_v2_validate_offsets(data, size, &pkg);
    (void)arapp_v2_validate_anti_replay(&pkg, 1700000000ULL, 0U);
    (void)arapp_v2_validate_manifest(&pkg);

    int status = arapp_v2_read_and_validate(data, size, 1700000000ULL, &pkg);
    if (status != ARAPP_V2_OK) {
        return 0;
    }

    if (arapp_v2_compute_digest(&pkg, digest) != ARAPP_V2_OK) {
        abort();
    }

    if (size <= sizeof(wire)) {
        if (arapp_v2_build(&pkg.prefix, pkg.signatures, pkg.manifest_json, pkg.manifest_len,
                           pkg.ciphertext, pkg.ciphertext_len, wire, sizeof(wire), &wire_len) != ARAPP_V2_OK) {
            abort();
        }

        if (wire_len != size || memcmp(wire, data, size) != 0) {
            abort();
        }

        if (arapp_v2_read(wire, wire_len, &roundtrip_pkg) != ARAPP_V2_OK) {
            abort();
        }

        if (roundtrip_pkg.format_version != pkg.format_version ||
            roundtrip_pkg.target_arch != pkg.target_arch ||
            roundtrip_pkg.container_flags != pkg.container_flags ||
            roundtrip_pkg.timestamp_issued != pkg.timestamp_issued ||
            roundtrip_pkg.timestamp_expiry != pkg.timestamp_expiry ||
            roundtrip_pkg.header_size_bytes != pkg.header_size_bytes ||
            roundtrip_pkg.entitlements_json_length != pkg.entitlements_json_length ||
            roundtrip_pkg.ciphertext_size_bytes != pkg.ciphertext_size_bytes) {
            abort();
        }
    }

    return 0;
}
