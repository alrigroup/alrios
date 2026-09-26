/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#include "alrios/fd_pass.h"
#include <assert.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

int main(void) {
    int sv[2] = {-1, -1};
    int sp_rc = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    (void)sp_rc;
    assert(sp_rc == 0);

    int dev_null = open("/dev/null", O_RDONLY);
    (void)dev_null;
    assert(dev_null >= 0);

    int send_rc = alrios_send_live_fd(sv[0], dev_null);
    (void)send_rc;
    assert(send_rc == 0);

    int received_fd = alrios_recv_live_fd(sv[1]);
    (void)received_fd;
    assert(received_fd >= 0);

    close(dev_null);
    close(received_fd);
    close(sv[0]);
    close(sv[1]);

    printf("TASK-008 (Live FD Passing): PASS\n");
    return 0;
}
