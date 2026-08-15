/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

/* Fuzz target do parser HTTP (Fase 2 gate): nunca deve crashar/leak
   com entrada arbitrária (libFuzzer + ASan/UBSan). */

#include <stddef.h>
#include <stdint.h>

#include "arwn_http.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    arwn_http_req_t req;
    int rc = arwn_http_parse(&req, (const char *)data, size);
    (void)rc;

    /* percorre os headers (exercita o id do hash) */
    for (int i = 0; i < req.header_count; i++) {
        (void)req.headers[i].id;
    }
    (void)arwn_http_header(&req, ARWN_HTTP_H_CONTENT_LENGTH, NULL);
    (void)arwn_http_header(&req, ARWN_HTTP_H_CONNECTION, NULL);
    (void)arwn_http_method_is(&req, "GET");
    return 0;
}