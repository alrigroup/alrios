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
    int sv[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

    int dev_null = open("/dev/null", O_RDONLY);
    assert(dev_null >= 0);

    assert(alrios_send_live_fd(sv[0], dev_null) == 0);
    int received_fd = alrios_recv_live_fd(sv[1]);
    assert(received_fd >= 0);

    close(dev_null);
    close(received_fd);
    close(sv[0]);
    close(sv[1]);

    printf("TASK-008 (Live FD Passing): PASS\n");
    return 0;
}
