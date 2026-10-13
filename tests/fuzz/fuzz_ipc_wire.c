/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/ipc_channel.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size == 0) {
        return 0;
    }

    /* 1. HTTP request parser fuzzing */
    char method[32];
    char path[256];
    memset(method, 0, sizeof(method));
    memset(path, 0, sizeof(path));
    (void)alrios_ipc_parse_http_req((const char *)data, size, method, sizeof(method), path, sizeof(path));

    /* 2. HTTP response parser fuzzing */
    int status_code = 0;
    char reason[64];
    memset(reason, 0, sizeof(reason));
    (void)alrios_ipc_parse_http_resp((const char *)data, size, &status_code, reason, sizeof(reason));

    /* 3. HTTP packed request unpack fuzzing */
    alrios_http_request_t req;
    uint8_t req_body[4096];
    memset(&req, 0, sizeof(req));
    memset(req_body, 0, sizeof(req_body));
    (void)alrios_ipc_unpack_http_req(data, size, &req, req_body, sizeof(req_body));

    /* 4. HTTP packed response unpack fuzzing */
    alrios_http_response_t resp;
    uint8_t resp_body[4096];
    memset(&resp, 0, sizeof(resp));
    memset(resp_body, 0, sizeof(resp_body));
    (void)alrios_ipc_unpack_http_resp(data, size, &resp, resp_body, sizeof(resp_body));

    return 0;
}
