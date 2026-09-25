/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */

#ifndef ALRIOS_IPC_CHANNEL_H
#define ALRIOS_IPC_CHANNEL_H

#include <stdint.h>
#include <stddef.h>

#define ALRIOS_IPC_MAGIC 0x41524950U /* 'ARIP' */
#define ALRIOS_IPC_MAX_PAYLOAD 65524U /* 65536 max buffer - 12 bytes header */

typedef enum alri_ipc_msg_type {
    ALRI_IPC_MSG_HTTP_REQ   = 0x0010,
    ALRI_IPC_MSG_HTTP_RESP  = 0x0011,
    ALRI_IPC_MSG_STREAM_CHK = 0x0020,
    ALRI_IPC_MSG_SHUTDOWN   = 0x00E0,
    ALRI_IPC_MSG_HEARTBEAT  = 0x00F0
} alri_ipc_msg_type_t;

#pragma pack(push, 1)

typedef struct alri_ipc_frame_hdr {
    uint32_t magic;
    uint16_t msg_type;
    uint16_t flags;
    uint32_t payload_len;
} alri_ipc_frame_hdr_t;

#pragma pack(pop)

_Static_assert(sizeof(alri_ipc_frame_hdr_t) == 12, "alri_ipc_frame_hdr_t must be exactly 12 bytes fixed");
_Static_assert(offsetof(alri_ipc_frame_hdr_t, magic) == 0, "magic offset must be 0");
_Static_assert(offsetof(alri_ipc_frame_hdr_t, msg_type) == 4, "msg_type offset must be 4");
_Static_assert(offsetof(alri_ipc_frame_hdr_t, flags) == 6, "flags offset must be 6");
_Static_assert(offsetof(alri_ipc_frame_hdr_t, payload_len) == 8, "payload_len offset must be 8");

int alri_ipc_send(int socket_fd, alri_ipc_msg_type_t type, const void *payload, uint32_t payload_len);
int alri_ipc_recv(int socket_fd, alri_ipc_frame_hdr_t *out_hdr, void *out_buf, uint32_t max_buf_len);

/* Non-blocking epoll multiplexer API for guest socketpairs */
int alrios_ipc_set_nonblocking(int fd);
int alrios_ipc_create_epoll_channel(int socket_fd);
int alrios_ipc_epoll_wait(int epfd, int *ready_fd, uint32_t *events, int timeout_ms);
int alrios_ipc_epoll_add(int epfd, int socket_fd, uint32_t events);
int alrios_ipc_epoll_mod(int epfd, int socket_fd, uint32_t events);
int alrios_ipc_epoll_del(int epfd, int socket_fd);

/* Binary HTTP request/response serialization over IPC */
#define ALRIOS_HTTP_MAX_METHOD_LEN 16
#define ALRIOS_HTTP_MAX_PATH_LEN   4096
#define ALRIOS_HTTP_MAX_STATUS_LEN 64
#define ALRIOS_HTTP_MAX_HEADERS    32
#define ALRIOS_HTTP_MAX_HEADER_KEY 128
#define ALRIOS_HTTP_MAX_HEADER_VAL 1024

#define ALRIOS_HTTP_REQ_MAGIC  0x41525155U /* 'ARQU' */
#define ALRIOS_HTTP_RESP_MAGIC 0x41525350U /* 'ARSP' */

#pragma pack(push, 1)
typedef struct alrios_ipc_bin_req_hdr {
    uint32_t magic;
    uint16_t method_len;
    uint16_t path_len;
    uint16_t header_count;
    uint16_t reserved;
    uint32_t body_len;
} alrios_ipc_bin_req_hdr_t;

typedef struct alrios_ipc_bin_resp_hdr {
    uint32_t magic;
    uint16_t status_code;
    uint16_t status_text_len;
    uint16_t header_count;
    uint16_t reserved;
    uint32_t body_len;
} alrios_ipc_bin_resp_hdr_t;

typedef struct alrios_ipc_bin_hdr_pair {
    uint16_t key_len;
    uint16_t val_len;
} alrios_ipc_bin_hdr_pair_t;
#pragma pack(pop)

typedef struct alrios_http_header {
    char key[ALRIOS_HTTP_MAX_HEADER_KEY];
    char value[ALRIOS_HTTP_MAX_HEADER_VAL];
} alrios_http_header_t;

typedef struct alrios_http_request {
    char method[ALRIOS_HTTP_MAX_METHOD_LEN];
    char path[ALRIOS_HTTP_MAX_PATH_LEN];
    alrios_http_header_t headers[ALRIOS_HTTP_MAX_HEADERS];
    size_t header_count;
    const uint8_t *body;
    size_t body_len;
} alrios_http_request_t;

typedef struct alrios_http_response {
    int status_code;
    char status_text[ALRIOS_HTTP_MAX_STATUS_LEN];
    alrios_http_header_t headers[ALRIOS_HTTP_MAX_HEADERS];
    size_t header_count;
    const uint8_t *body;
    size_t body_len;
} alrios_http_response_t;

int alrios_ipc_serialize_http_req(char *buf, size_t max, const char *method, const char *path);
int alrios_ipc_serialize_http_resp(char *buf, size_t max, int status_code, const char *reason_phrase, const char *body, size_t body_len);
int alrios_ipc_parse_http_req(const char *raw, size_t raw_len, char *method, size_t max_method, char *path, size_t max_path);
int alrios_ipc_parse_http_resp(const char *raw, size_t raw_len, int *status_code, char *reason, size_t max_reason);

/* Full structured binary serialization over IPC */
int alrios_ipc_pack_http_req(const alrios_http_request_t *req, uint8_t *out_buf, size_t max_len, size_t *out_len);
int alrios_ipc_unpack_http_req(const uint8_t *in_buf, size_t in_len, alrios_http_request_t *req, uint8_t *body_buf, size_t max_body_len);
int alrios_ipc_pack_http_resp(const alrios_http_response_t *resp, uint8_t *out_buf, size_t max_len, size_t *out_len);
int alrios_ipc_unpack_http_resp(const uint8_t *in_buf, size_t in_len, alrios_http_response_t *resp, uint8_t *body_buf, size_t max_body_len);

#endif /* ALRIOS_IPC_CHANNEL_H */
