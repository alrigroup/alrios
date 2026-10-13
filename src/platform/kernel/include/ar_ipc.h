/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */
#ifndef AR_IPC_H
#define AR_IPC_H

#include <stdint.h>
#include "aros_hal.h"

#define IPC_REGISTER    1
#define IPC_UNREGISTER  2
#define IPC_REQUEST     3
#define IPC_RESPONSE    4
#define IPC_HEARTBEAT   5
#define IPC_ACK         6
#define IPC_ERROR       7
#define IPC_SHUTDOWN    8
#define IPC_QUERY       9
#define IPC_QUERY_RESP  10
#define IPC_RELOAD      11
#define IPC_CACHE_CLEAR 12

/* Core control channel (TCP 127.0.0.1:9600), served by arcore itself */
#define IPC_CTL_PING          13
#define IPC_CTL_START         14
#define IPC_CTL_STOP          15
#define IPC_CTL_RESTART       16
#define IPC_CTL_STATUS        17
#define IPC_CTL_LIST          18
#define IPC_CTL_POWER_OFF     19
#define IPC_CTL_POWER_RELOAD  20
#define IPC_CTL_REFRESH       21

/* Real-Time Zero-Downtime Hot-Reload Opcodes */
#define IPC_DEPLOY_SUBMIT     0x20
#define IPC_DEPLOY_STAGE      0x21
#define IPC_DEPLOY_SPAWN      0x22
#define IPC_DEPLOY_PROBE      0x23
#define IPC_ROUTE_SWAP        0x24
#define IPC_ROUTE_DRAIN       0x25
#define IPC_DEPLOY_ROLLBACK   0x26
#define IPC_FD_HANDOFF        0x27

#define AR_IPC_DEFAULT_PORT 9500
#define AR_CTL_PORT 9600
#define AR_IPC_MAX_CLIENTS 64
#define AR_IPC_BUF_SIZE 65536

int ar_ipc_send_frame(int fd, int type, const void *data, uint32_t len);
int ar_ipc_recv_frame(int fd, int *type, void *buf, uint32_t *len);
int ar_ipc_send_raw(int fd, const void *data, uint32_t len);
int ar_ipc_recv_raw(int fd, void *buf, uint32_t maxlen);

int ar_ipc_server_start(uint16_t port);
int ar_ipc_server_stop(int server_fd);

int ar_ipc_client_connect(const char *host, uint16_t port);

#endif
