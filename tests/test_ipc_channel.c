/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */

#include "alrios/ipc_channel.h"
#include <assert.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

int main(void) {
    int sv[2];
    assert(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sv) == 0);

    const char *msg = "PING_PAYLOAD";
    uint32_t len = (uint32_t)strlen(msg);

    assert(alri_ipc_send(sv[0], ALRI_IPC_MSG_HEARTBEAT, msg, len) == 0);

    alri_ipc_frame_hdr_t hdr;
    char recv_buf[128];
    assert(alri_ipc_recv(sv[1], &hdr, recv_buf, sizeof(recv_buf)) == 0);
    assert(hdr.magic == ALRIOS_IPC_MAGIC);
    assert(hdr.msg_type == ALRI_IPC_MSG_HEARTBEAT);
    assert(hdr.payload_len == len);
    assert(memcmp(recv_buf, msg, len) == 0);

    close(sv[0]);
    close(sv[1]);

    printf("TASK-003 (IPC Channel Framing): PASS\n");
    return 0;
}
