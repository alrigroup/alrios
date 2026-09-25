/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#ifndef ALRIOS_FD_PASS_H
#define ALRIOS_FD_PASS_H

int alrios_send_live_fd(int unix_sock, int fd_to_send);
int alrios_recv_live_fd(int unix_sock);

#endif /* ALRIOS_FD_PASS_H */
