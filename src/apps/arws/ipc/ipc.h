/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARGW_IPC_H
#define ARGW_IPC_H

#define IPC_REGISTER    1
#define IPC_UNREGISTER  2
#define IPC_REQUEST     3
#define IPC_RESPONSE    4
#define IPC_HEARTBEAT   5
#define IPC_ACK         6
#define IPC_ERROR       7

typedef struct {
    int type;
    int length;
    unsigned char *data;
} IpcFrame;

int ipc_send_frame(int fd, int type, const unsigned char *data, int length);
int ipc_recv_frame(int fd, IpcFrame *frame);
void ipc_free_frame(IpcFrame *frame);

int ipc_send_raw(int fd, const unsigned char *data, int length);
int ipc_recv_raw(int fd, unsigned char *buf, int maxlen);

#endif
