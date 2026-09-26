/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/pki/certificate.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    alrios_certificate_t certificate;
    alrios_certificate_t decoded_again;
    uint8_t wire[ALRIOS_CERTIFICATE_WIRE_MAX];
    size_t wire_len = 0U;
    int status;

    status = alrios_certificate_decode(data, size, &certificate);
    if (status != ALRIOS_CERTIFICATE_OK) {
        return 0;
    }
    if (alrios_certificate_encode(&certificate, wire, sizeof(wire), &wire_len) !=
        ALRIOS_CERTIFICATE_OK) {
        abort();
    }
    if (wire_len != size || memcmp(wire, data, size) != 0) {
        abort();
    }
    if (alrios_certificate_decode(wire, wire_len, &decoded_again) !=
        ALRIOS_CERTIFICATE_OK) {
        abort();
    }
    if (decoded_again.version != certificate.version ||
        decoded_again.role != certificate.role ||
        decoded_again.public_key_algorithm != certificate.public_key_algorithm ||
        decoded_again.signature_algorithm != certificate.signature_algorithm ||
        decoded_again.subject_len != certificate.subject_len ||
        decoded_again.public_key_len != certificate.public_key_len ||
        decoded_again.signature_len != certificate.signature_len) {
        abort();
    }
    return 0;
}
