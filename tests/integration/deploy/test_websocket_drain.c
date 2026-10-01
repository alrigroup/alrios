/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/deploy_drain.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>

extern int alrios_supervisor_handover_listener(int control_socket, int listener_fd);
extern int alrios_supervisor_receive_listener(int control_socket);

int main(void) {
    /* Listener inheritance: new process receives same listener atomically. */
    int control[2] = {-1, -1};
    int stream[2] = {-1, -1};
    int sp1 = socketpair(AF_UNIX, SOCK_SEQPACKET, 0, control);
    int sp2 = socketpair(AF_UNIX, SOCK_STREAM, 0, stream);
    (void)sp1; (void)sp2;
    assert(sp1 == 0 && sp2 == 0);
    int ho = alrios_supervisor_handover_listener(control[0], stream[0]);
    (void)ho;
    assert(ho == 0);
    int inherited = alrios_supervisor_receive_listener(control[1]);
    (void)inherited;
    assert(inherited >= 0);

    /* Existing long-lived stream survives listener handover. */
    const char msg[] = "websocket-still-alive";
    ssize_t wr = write(stream[0], msg, sizeof(msg));
    (void)wr;
    assert(wr == (ssize_t)sizeof(msg));
    char buf[sizeof(msg)];
    ssize_t rd = read(stream[1], buf, sizeof(buf));
    (void)rd;
    assert(rd == (ssize_t)sizeof(buf));

    alrios_drain_tracker_t tracker;
    int di = alrios_drain_init(&tracker);
    (void)di;
    assert(di == 0);
    int dco = alrios_drain_connection_open(&tracker);
    (void)dco;
    assert(dco == 0);
    alrios_drain_config_t config = {.drain_timeout_ms = 150, .poll_interval_ms = 10};
    /* Configurable timeout, explicitly greater than 100ms; no blind SIGKILL. */
    int dw1 = alrios_drain_wait(&tracker, &config);
    (void)dw1;
    assert(dw1 == 1);
    assert(tracker.active_connections == 1);
    int dcc = alrios_drain_connection_close(&tracker);
    (void)dcc;
    assert(dcc == 0);
    int dw2 = alrios_drain_wait(&tracker, &config);
    (void)dw2;
    assert(dw2 == 0);

    close(inherited);
    close(control[0]); close(control[1]); close(stream[0]); close(stream[1]);
    printf("TEST_WEBSOCKET_DRAIN: PASS\n");
    return 0;
}
