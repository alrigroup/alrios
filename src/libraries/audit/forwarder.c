/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "alrios/audit/forwarder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/select.h>

int alrios_audit_create_checkpoint(const char *audit_log_path, alrios_audit_checkpoint_t *out_checkpoint) {
    if (!audit_log_path || !out_checkpoint) {
        return ALRIOS_AUDIT_ERR_INVALID_ARGUMENT;
    }

    alrios_audit_verify_result_t res;
    memset(&res, 0, sizeof(res));

    int rc = alrios_audit_verify(audit_log_path, &res);
    if (rc != ALRIOS_AUDIT_OK) {
        return rc;
    }

    memset(out_checkpoint, 0, sizeof(*out_checkpoint));
    out_checkpoint->sequence = res.block_count > 0 ? res.block_count - 1 : 0;
    out_checkpoint->timestamp = (uint64_t)time(NULL);
    memcpy(out_checkpoint->root_hash, res.last_hash, ALRIOS_AUDIT_HASH_SIZE);
    // Simple deterministic signing / tagging placeholder (e.g., zero or test tag)
    memset(out_checkpoint->signature, 0xAA, sizeof(out_checkpoint->signature));

    return ALRIOS_AUDIT_OK;
}

int alrios_audit_forward_checkpoint(int socket_fd, const alrios_audit_checkpoint_t *checkpoint, int timeout_ms) {
    if (socket_fd < 0 || !checkpoint) {
        return ALRIOS_AUDIT_ERR_INVALID_ARGUMENT;
    }

    // Set non-blocking or use select for backpressure timeout
    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(socket_fd, &wfds);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int sel = select(socket_fd + 1, NULL, &wfds, NULL, timeout_ms >= 0 ? &tv : NULL);
    if (sel < 0) {
        if (errno == EINTR) {
            return ALRIOS_AUDIT_OK; // Non-fatal interruption
        }
        return ALRIOS_AUDIT_ERR_IO;
    }
    if (sel == 0) {
        // Timeout / Backpressure: return success (non-blocking drop/deferred queue) so supervisor is not blocked
        return ALRIOS_AUDIT_OK;
    }

    ssize_t written = send(socket_fd, checkpoint, sizeof(*checkpoint), MSG_NOSIGNAL);
    if (written < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ENOBUFS) {
            // Backpressure active: drop or queue without blocking
            return ALRIOS_AUDIT_OK;
        }
        return ALRIOS_AUDIT_ERR_IO;
    }

    return ALRIOS_AUDIT_OK;
}
