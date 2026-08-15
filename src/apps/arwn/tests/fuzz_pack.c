/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

/* Fuzz target do contêiner .arweb (libFuzzer). */

#include <stddef.h>
#include <stdint.h>

#include "arwn_pack.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    arwn_pack_validate(data, size);
    return 0;
}