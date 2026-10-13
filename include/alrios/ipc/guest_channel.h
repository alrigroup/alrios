/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_IPC_GUEST_CHANNEL_H
#define ALRIOS_IPC_GUEST_CHANNEL_H

#include <stddef.h>
#include <stdint.h>

#define AR_GUEST_WIRE_MAGIC              0x41524743U
#define AR_GUEST_WIRE_VERSION            1U
#define AR_GUEST_WIRE_HEADER_SIZE        24U
#define AR_GUEST_HARD_MAX_FRAME_SIZE     65536U
#define AR_GUEST_HARD_MAX_PAYLOAD_SIZE   \
    (AR_GUEST_HARD_MAX_FRAME_SIZE - AR_GUEST_WIRE_HEADER_SIZE)

#define AR_GUEST_FLAG_RESPONSE           0x00000001U
#define AR_GUEST_FLAG_CONTROL            0x00000002U
#define AR_GUEST_FLAGS_ALLOWED           \
    (AR_GUEST_FLAG_RESPONSE | AR_GUEST_FLAG_CONTROL)

#define AR_GUEST_MATCH_PID               0x00000001U
#define AR_GUEST_MATCH_UID               0x00000002U
#define AR_GUEST_MATCH_GID               0x00000004U
#define AR_GUEST_MATCH_ALL               \
    (AR_GUEST_MATCH_PID | AR_GUEST_MATCH_UID | AR_GUEST_MATCH_GID)

#define AR_GUEST_DEFAULT_IO_TIMEOUT_MS   1000

typedef struct ar_guest_channel ar_guest_channel_t;

typedef enum ar_guest_status {
    AR_GUEST_OK = 0,
    AR_GUEST_ERR_INVALID = -5001,
    AR_GUEST_ERR_UNSUPPORTED = -5002,
    AR_GUEST_ERR_SYSTEM = -5003,
    AR_GUEST_ERR_AUTH = -5004,
    AR_GUEST_ERR_PROTOCOL = -5005,
    AR_GUEST_ERR_LIMIT = -5006,
    AR_GUEST_ERR_TIMEOUT = -5007,
    AR_GUEST_ERR_WOULD_BLOCK = -5008,
    AR_GUEST_ERR_CLOSED = -5009,
    AR_GUEST_ERR_BUFFER_TOO_SMALL = -5010,
    AR_GUEST_ERR_BACKPRESSURE = -5011,
    AR_GUEST_ERR_NOT_FOUND = -5012,
    AR_GUEST_ERR_EXISTS = -5013,
    AR_GUEST_ERR_NOMEM = -5014
} ar_guest_status_t;

typedef struct ar_guest_peer_identity {
    uint32_t pid;
    uint32_t uid;
    uint32_t gid;
} ar_guest_peer_identity_t;

typedef struct ar_guest_identity_policy {
    uint32_t match_flags;
    ar_guest_peer_identity_t expected;
} ar_guest_identity_policy_t;

typedef struct ar_guest_channel_config {
    uint32_t max_payload_size;
    int io_timeout_ms;
    ar_guest_identity_policy_t peer_policy;
} ar_guest_channel_config_t;

typedef struct ar_guest_frame_header {
    uint16_t message_type;
    uint32_t flags;
    uint64_t request_id;
    uint32_t payload_length;
} ar_guest_frame_header_t;

/* Creates a Linux AF_UNIX/SOCK_SEQPACKET pair with CLOEXEC and NONBLOCK set. */
int ar_guest_channel_socketpair(int out_fds[2]);

/* On success the channel owns fd. Authentication is kernel-derived with
 * SO_PEERCRED and every received datagram must carry matching SCM_CREDENTIALS. */
int ar_guest_channel_attach(
    int fd,
    const ar_guest_channel_config_t *config,
    ar_guest_channel_t **out_channel);

void ar_guest_channel_close(ar_guest_channel_t *channel);
int ar_guest_channel_fd(const ar_guest_channel_t *channel);
int ar_guest_channel_peer(
    const ar_guest_channel_t *channel,
    ar_guest_peer_identity_t *out_peer);

int ar_guest_channel_send(
    ar_guest_channel_t *channel,
    uint16_t message_type,
    uint32_t flags,
    uint64_t request_id,
    const void *payload,
    uint32_t payload_length);

/* One call consumes at most one complete SOCK_SEQPACKET datagram. On
 * BUFFER_TOO_SMALL, out_header->payload_length reports the required size. */
int ar_guest_channel_receive(
    ar_guest_channel_t *channel,
    ar_guest_frame_header_t *out_header,
    void *out_payload,
    uint32_t payload_capacity);

#endif /* ALRIOS_IPC_GUEST_CHANNEL_H */
