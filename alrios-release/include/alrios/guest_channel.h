/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_GUEST_CHANNEL_H
#define ALRIOS_GUEST_CHANNEL_H

#include <stdint.h>
#include <sys/types.h>
#include "alrios/ipc_channel.h"

int alrios_guest_channel_create(int sv[2]);
int alrios_guest_channel_check_peer(int fd, uid_t expected_uid, pid_t *out_pid);
int alrios_guest_channel_send(int socket_fd, uint16_t msg_type, const void *payload, uint32_t payload_len);
int alrios_guest_channel_recv(int socket_fd, alri_ipc_frame_hdr_t *out_hdr, void *out_buf, uint32_t max_buf_len);

int alrios_guest_router_init(void);
int alrios_guest_router_register_guest(int guest_socket_fd, uint32_t guest_id);
int alrios_guest_router_poll_and_route(int epfd, int timeout_ms);
void alrios_guest_router_cleanup(void);

#endif /* ALRIOS_GUEST_CHANNEL_H */
