/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */

#include "alrios/ipc_channel.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void) {
    char buf[128];
    int len = alrios_ipc_serialize_http_req(buf, sizeof(buf), "GET", "/status");
    assert(len > 0);
    (void)len;
    assert(strcmp(buf, "GET /status HTTP/1.1\r\n\r\n") == 0);

    /* Test edge cases and cybersec constraints */
    assert(alrios_ipc_serialize_http_req(NULL, 128, "GET", "/status") == -1);
    assert(alrios_ipc_serialize_http_req(buf, 0, "GET", "/status") == -1);
    assert(alrios_ipc_serialize_http_req(buf, sizeof(buf), NULL, "/status") == -1);
    assert(alrios_ipc_serialize_http_req(buf, sizeof(buf), "GET", NULL) == -1);

    /* Buffer too small */
    assert(alrios_ipc_serialize_http_req(buf, 10, "GET", "/status") == -2);

    printf("TASK-021 (Binary HTTP Serialization): PASS\n");
    return 0;
}
