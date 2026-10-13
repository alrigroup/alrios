/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_AUDIT_FORWARDER_H
#define ALRIOS_AUDIT_FORWARDER_H

#include "alrios/audit/block.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALRIOS_FORWARDER_OK                        0
#define ALRIOS_FORWARDER_ERR_INVALID_ARGUMENT  -8001
#define ALRIOS_FORWARDER_ERR_QUEUE_FULL        -8002
#define ALRIOS_FORWARDER_ERR_TRANSPORT         -8003
#define ALRIOS_FORWARDER_ERR_AUTH              -8004
#define ALRIOS_FORWARDER_ERR_ROLLBACK_DETECTED -8005
#define ALRIOS_FORWARDER_ERR_IO                -8006
#define ALRIOS_FORWARDER_ERR_OVERFLOW          -8007
#define ALRIOS_FORWARDER_ERR_STATE_CORRUPT     -8008

#define ALRIOS_FORWARDER_KEY_SIZE              32U
#define ALRIOS_FORWARDER_MAC_SIZE              32U
#define ALRIOS_FORWARDER_RECEIPT_NONCE_SIZE    16U
#define ALRIOS_FORWARDER_MAX_QUEUE_ENTRIES     1024U

/*
 * Wire record for remote tip checkpoint:
 * magic:        "ALRICHKP" (8B)
 * version:      u16 (1)
 * flags:        u16 (0)
 * sequence:     u64
 * timestamp_ns: u64
 * block_hash:   32B
 * mac:          32B (HMAC-SHA256 over checkpoint body with domain separation)
 */
#define ALRIOS_CHECKPOINT_WIRE_SIZE            92U
#define ALRIOS_RECEIPT_WIRE_SIZE              100U
#define ALRIOS_FORWARDER_STATE_WIRE_SIZE       84U

typedef struct alrios_audit_checkpoint {
    uint64_t sequence;
    uint64_t timestamp_ns;
    uint8_t  block_hash[ALRIOS_AUDIT_BLOCK_HASH_SIZE];
    uint8_t  mac[ALRIOS_FORWARDER_MAC_SIZE];
} alrios_audit_checkpoint_t;

typedef struct alrios_audit_receipt {
    uint64_t ack_sequence;
    uint8_t  ack_hash[ALRIOS_AUDIT_BLOCK_HASH_SIZE];
    uint8_t  nonce[ALRIOS_FORWARDER_RECEIPT_NONCE_SIZE];
    uint8_t  mac[ALRIOS_FORWARDER_MAC_SIZE];
} alrios_audit_receipt_t;

/* Transport callback signature */
typedef int (*alrios_forwarder_send_fn)(
    const uint8_t *checkpoint_wire,
    size_t wire_len,
    uint8_t *receipt_buf,
    size_t receipt_buf_capacity,
    size_t *receipt_received_len,
    void *user_ctx
);

/* State stored on disk to track verified remote checkpoints */
typedef struct alrios_forwarder_state {
    uint64_t ack_sequence;
    uint8_t  ack_hash[ALRIOS_AUDIT_BLOCK_HASH_SIZE];
} alrios_forwarder_state_t;

typedef struct alrios_audit_forwarder {
    alrios_audit_checkpoint_t *queue;
    size_t queue_capacity;
    size_t queue_head;
    size_t queue_tail;
    size_t queue_count;

    uint8_t auth_key[ALRIOS_FORWARDER_KEY_SIZE];

    alrios_forwarder_state_t persisted_state;
    int state_fd; /* Optional open fd to persisted state file */

    alrios_forwarder_send_fn send_fn;
    void *transport_ctx;
} alrios_audit_forwarder_t;

/* Checkpoint serialization & authentication */
int alrios_checkpoint_encode(
    uint64_t sequence,
    uint64_t timestamp_ns,
    const uint8_t block_hash[ALRIOS_AUDIT_BLOCK_HASH_SIZE],
    const uint8_t key[ALRIOS_FORWARDER_KEY_SIZE],
    uint8_t out_wire[ALRIOS_CHECKPOINT_WIRE_SIZE]
);

int alrios_checkpoint_decode_and_verify(
    const uint8_t in_wire[ALRIOS_CHECKPOINT_WIRE_SIZE],
    const uint8_t key[ALRIOS_FORWARDER_KEY_SIZE],
    alrios_audit_checkpoint_t *out_checkpoint
);

/* Receipt creation & verification */
int alrios_receipt_encode(
    uint64_t ack_sequence,
    const uint8_t ack_hash[ALRIOS_AUDIT_BLOCK_HASH_SIZE],
    const uint8_t nonce[ALRIOS_FORWARDER_RECEIPT_NONCE_SIZE],
    const uint8_t key[ALRIOS_FORWARDER_KEY_SIZE],
    uint8_t out_wire[ALRIOS_RECEIPT_WIRE_SIZE]
);

int alrios_receipt_decode_and_verify(
    const uint8_t in_wire[ALRIOS_RECEIPT_WIRE_SIZE],
    const uint8_t key[ALRIOS_FORWARDER_KEY_SIZE],
    alrios_audit_receipt_t *out_receipt
);

/* State file persistence & rollback detection */
int alrios_forwarder_state_read(
    int fd,
    const uint8_t key[ALRIOS_FORWARDER_KEY_SIZE],
    alrios_forwarder_state_t *out_state
);

int alrios_forwarder_state_write(
    int fd,
    const uint8_t key[ALRIOS_FORWARDER_KEY_SIZE],
    const alrios_forwarder_state_t *state
);

int alrios_forwarder_verify_chain_not_rolled_back(
    int audit_fd,
    const alrios_forwarder_state_t *persisted_state
);

/* Network transport implementation over HAL non-blocking socket */
typedef struct alrios_forwarder_socket_transport {
    int socket_fd;
    int timeout_ms;
} alrios_forwarder_socket_transport_t;

int alrios_forwarder_socket_send(
    const uint8_t *checkpoint_wire,
    size_t wire_len,
    uint8_t *receipt_buf,
    size_t receipt_buf_capacity,
    size_t *receipt_received_len,
    void *user_ctx
);

/* Forwarder lifecycle & non-blocking execution */
int alrios_forwarder_init(
    alrios_audit_forwarder_t *fwd,
    alrios_audit_checkpoint_t *queue_buffer,
    size_t queue_capacity,
    const uint8_t key[ALRIOS_FORWARDER_KEY_SIZE],
    int state_fd,
    alrios_forwarder_send_fn send_fn,
    void *transport_ctx
);

/* Non-blocking enqueue from local audit chain / ring buffer */
int alrios_forwarder_enqueue(
    alrios_audit_forwarder_t *fwd,
    uint64_t sequence,
    uint64_t timestamp_ns,
    const uint8_t block_hash[ALRIOS_AUDIT_BLOCK_HASH_SIZE]
);

/* Drain queue via non-blocking / bounded transport */
int alrios_forwarder_flush(
    alrios_audit_forwarder_t *fwd
);

/* Clean up sensitive forwarder keys and queue */
void alrios_forwarder_destroy(alrios_audit_forwarder_t *fwd);

#ifdef __cplusplus
}
#endif

#endif /* ALRIOS_AUDIT_FORWARDER_H */
