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

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    alrios_arapp_v2_view_t view;
    int status;

    status = alrios_arapp_v2_validate(
        data,
        size,
        UINT64_C(1770000000),
        ALRIOS_ARAPP_V2_MAX_CLOCK_SKEW_SECONDS,
        &view
    );
    if (status == ALRIOS_ARAPP_V2_OK) {
        if (view.prefix != data ||
            view.prefix_len != ALRIOS_ARAPP_V2_PREFIX_SIZE ||
            view.header_len != ALRIOS_ARAPP_V2_HEADER_SIZE ||
            view.manifest != data + view.header_len ||
            view.manifest_len == 0U ||
            view.manifest_len > size - view.header_len ||
            view.ciphertext != view.manifest + view.manifest_len ||
            view.ciphertext_len != size - view.header_len - view.manifest_len) {
            abort();
        }
    }
    return 0;
}
