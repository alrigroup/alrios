/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */

#include "alrios/ipc_channel.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

static void test_legacy_http_req_serialization(void) {
    char buf[128];
    int len = alrios_ipc_serialize_http_req(buf, sizeof(buf), "GET", "/status");
    assert(len > 0);
    assert(strcmp(buf, "GET /status HTTP/1.1\r\n\r\n") == 0);

    /* Test edge cases and cybersec constraints */
    assert(alrios_ipc_serialize_http_req(NULL, 128, "GET", "/status") == -1);
    assert(alrios_ipc_serialize_http_req(buf, 0, "GET", "/status") == -1);
    assert(alrios_ipc_serialize_http_req(buf, sizeof(buf), NULL, "/status") == -1);
    assert(alrios_ipc_serialize_http_req(buf, sizeof(buf), "GET", NULL) == -1);

    /* Reject CRLF injection in path */
    assert(alrios_ipc_serialize_http_req(buf, sizeof(buf), "GET", "/test\r\nEvil: 1") == -1);
    /* Reject invalid method */
    assert(alrios_ipc_serialize_http_req(buf, sizeof(buf), "INVALID_METHOD", "/status") == -1);
    /* Buffer too small */
    assert(alrios_ipc_serialize_http_req(buf, 10, "GET", "/status") == -2);
}

static void test_http_resp_serialization(void) {
    char buf[256];
    const char *body = "{\"status\":\"healthy\"}";
    int len = alrios_ipc_serialize_http_resp(buf, sizeof(buf), 200, "OK", body, strlen(body));
    assert(len > 0);
    assert(strstr(buf, "HTTP/1.1 200 OK\r\n") != NULL);
    assert(strstr(buf, "Content-Length: 20\r\n\r\n{\"status\":\"healthy\"}") != NULL);

    /* Status code validation */
    assert(alrios_ipc_serialize_http_resp(buf, sizeof(buf), 99, "OK", NULL, 0) == -1);
    assert(alrios_ipc_serialize_http_resp(buf, sizeof(buf), 600, "OK", NULL, 0) == -1);
    assert(alrios_ipc_serialize_http_resp(NULL, sizeof(buf), 200, "OK", NULL, 0) == -1);
}

static void test_http_parsing(void) {
    const char *raw_req = "POST /api/v1/deploy HTTP/1.1\r\nHost: alrios.local\r\n\r\n";
    char method[16];
    char path[64];
    assert(alrios_ipc_parse_http_req(raw_req, strlen(raw_req), method, sizeof(method), path, sizeof(path)) == 0);
    assert(strcmp(method, "POST") == 0);
    assert(strcmp(path, "/api/v1/deploy") == 0);

    /* Malformed parsing */
    assert(alrios_ipc_parse_http_req(NULL, 10, method, sizeof(method), path, sizeof(path)) == -1);
    assert(alrios_ipc_parse_http_req("INVALID_NO_SPACES", 17, method, sizeof(method), path, sizeof(path)) == -1);

    const char *raw_resp = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
    int status = 0;
    char reason[32];
    assert(alrios_ipc_parse_http_resp(raw_resp, strlen(raw_resp), &status, reason, sizeof(reason)) == 0);
    assert(status == 404);
    assert(strcmp(reason, "Not Found") == 0);
}

static void test_structured_binary_req_roundtrip(void) {
    alrios_http_request_t req;
    memset(&req, 0, sizeof(req));
    strncpy(req.method, "POST", sizeof(req.method) - 1);
    strncpy(req.path, "/arapi/v1/auth", sizeof(req.path) - 1);

    strncpy(req.headers[0].key, "Content-Type", sizeof(req.headers[0].key) - 1);
    strncpy(req.headers[0].value, "application/json", sizeof(req.headers[0].value) - 1);

    strncpy(req.headers[1].key, "X-ALRI-Client", sizeof(req.headers[1].key) - 1);
    strncpy(req.headers[1].value, "Enterprise-Vault", sizeof(req.headers[1].value) - 1);

    req.header_count = 2;
    const char *payload = "{\"client_id\":\"sovereign_ring0\"}";
    req.body = (const uint8_t *)payload;
    req.body_len = strlen(payload);

    uint8_t bin_buf[1024];
    size_t packed_len = 0;
    assert(alrios_ipc_pack_http_req(&req, bin_buf, sizeof(bin_buf), &packed_len) == 0);
    assert(packed_len > sizeof(alrios_ipc_bin_req_hdr_t));

    alrios_http_request_t unpacked_req;
    uint8_t body_storage[256];
    assert(alrios_ipc_unpack_http_req(bin_buf, packed_len, &unpacked_req, body_storage, sizeof(body_storage)) == 0);

    assert(strcmp(unpacked_req.method, "POST") == 0);
    assert(strcmp(unpacked_req.path, "/arapi/v1/auth") == 0);
    assert(unpacked_req.header_count == 2);
    assert(strcmp(unpacked_req.headers[0].key, "Content-Type") == 0);
    assert(strcmp(unpacked_req.headers[0].value, "application/json") == 0);
    assert(strcmp(unpacked_req.headers[1].key, "X-ALRI-Client") == 0);
    assert(strcmp(unpacked_req.headers[1].value, "Enterprise-Vault") == 0);
    assert(unpacked_req.body_len == strlen(payload));
    assert(memcmp(unpacked_req.body, payload, unpacked_req.body_len) == 0);
}

static void test_structured_binary_resp_roundtrip(void) {
    alrios_http_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.status_code = 200;
    strncpy(resp.status_text, "OK", sizeof(resp.status_text) - 1);

    strncpy(resp.headers[0].key, "Server", sizeof(resp.headers[0].key) - 1);
    strncpy(resp.headers[0].value, "ALRIOS-Gateway/1.0", sizeof(resp.headers[0].value) - 1);
    resp.header_count = 1;

    const char *body_data = "AUTHENTICATED";
    resp.body = (const uint8_t *)body_data;
    resp.body_len = strlen(body_data);

    uint8_t bin_buf[512];
    size_t packed_len = 0;
    assert(alrios_ipc_pack_http_resp(&resp, bin_buf, sizeof(bin_buf), &packed_len) == 0);
    assert(packed_len > sizeof(alrios_ipc_bin_resp_hdr_t));

    alrios_http_response_t unpacked_resp;
    uint8_t body_storage[64];
    assert(alrios_ipc_unpack_http_resp(bin_buf, packed_len, &unpacked_resp, body_storage, sizeof(body_storage)) == 0);

    assert(unpacked_resp.status_code == 200);
    assert(strcmp(unpacked_resp.status_text, "OK") == 0);
    assert(unpacked_resp.header_count == 1);
    assert(strcmp(unpacked_resp.headers[0].key, "Server") == 0);
    assert(strcmp(unpacked_resp.headers[0].value, "ALRIOS-Gateway/1.0") == 0);
    assert(unpacked_resp.body_len == strlen(body_data));
    assert(memcmp(unpacked_resp.body, body_data, unpacked_resp.body_len) == 0);
}

int main(void) {
    test_legacy_http_req_serialization();
    test_http_resp_serialization();
    test_http_parsing();
    test_structured_binary_req_roundtrip();
    test_structured_binary_resp_roundtrip();

    printf("TASK-021 (Binary HTTP Serialization): PASS\n");
    return 0;
}
