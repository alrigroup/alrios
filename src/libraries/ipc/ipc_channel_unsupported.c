/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/ipc_channel.h"

int alri_ipc_send(int socket_fd, alri_ipc_msg_type_t type, const void *payload, uint32_t payload_len) {
    (void)socket_fd;
    (void)type;
    (void)payload;
    (void)payload_len;
    return -1;
}

int alri_ipc_recv(int socket_fd, alri_ipc_frame_hdr_t *out_hdr, void *out_buf, uint32_t max_buf_len) {
    (void)socket_fd;
    (void)out_hdr;
    (void)out_buf;
    (void)max_buf_len;
    return -1;
}
