/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/ipc/guest_channel.h"

struct ar_guest_channel {
    int unused;
};

int ar_guest_channel_socketpair(int out_fds[2]) {
    if (out_fds) {
        out_fds[0] = -1;
        out_fds[1] = -1;
    }
    return AR_GUEST_ERR_UNSUPPORTED;
}

int ar_guest_channel_attach(int fd,
                            const ar_guest_channel_config_t *config,
                            ar_guest_channel_t **out_channel) {
    (void)fd;
    (void)config;
    if (out_channel) *out_channel = NULL;
    return AR_GUEST_ERR_UNSUPPORTED;
}

void ar_guest_channel_close(ar_guest_channel_t *channel) {
    (void)channel;
}

int ar_guest_channel_fd(const ar_guest_channel_t *channel) {
    (void)channel;
    return -1;
}

int ar_guest_channel_peer(const ar_guest_channel_t *channel,
                          ar_guest_peer_identity_t *out_peer) {
    (void)channel;
    (void)out_peer;
    return AR_GUEST_ERR_UNSUPPORTED;
}

int ar_guest_channel_send(ar_guest_channel_t *channel,
                          uint16_t message_type,
                          uint32_t flags,
                          uint64_t request_id,
                          const void *payload,
                          uint32_t payload_length) {
    (void)channel;
    (void)message_type;
    (void)flags;
    (void)request_id;
    (void)payload;
    (void)payload_length;
    return AR_GUEST_ERR_UNSUPPORTED;
}

int ar_guest_channel_receive(ar_guest_channel_t *channel,
                             ar_guest_frame_header_t *out_header,
                             void *out_payload,
                             uint32_t payload_capacity) {
    (void)channel;
    (void)out_header;
    (void)out_payload;
    (void)payload_capacity;
    return AR_GUEST_ERR_UNSUPPORTED;
}
