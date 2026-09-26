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
    int sv[2] = {-1, -1};
    int sp_rc = socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sv);
    assert(sp_rc == 0);
    (void)sp_rc;

    const char *msg = "PING_PAYLOAD";
    uint32_t len = (uint32_t)strlen(msg);

    int send_rc = alri_ipc_send(sv[0], ALRI_IPC_MSG_HEARTBEAT, msg, len);
    assert(send_rc == 0);
    (void)send_rc;

    alri_ipc_frame_hdr_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    char recv_buf[128];
    memset(recv_buf, 0, sizeof(recv_buf));
    int recv_rc = alri_ipc_recv(sv[1], &hdr, recv_buf, sizeof(recv_buf));
    assert(recv_rc == 0);
    (void)recv_rc;
    assert(hdr.magic == ALRIOS_IPC_MAGIC);
    assert(hdr.msg_type == ALRI_IPC_MSG_HEARTBEAT);
    assert(hdr.payload_len == len);
    assert(memcmp(recv_buf, msg, len) == 0);

    close(sv[0]);
    close(sv[1]);

    printf("TASK-003 (IPC Channel Framing): PASS\n");
    return 0;
}
