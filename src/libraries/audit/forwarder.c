/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/audit/forwarder.h"
#include "alrios/crypto_verify.h"
#include "aros_hal.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define CHECKPOINT_VERSION 1U
#define STATE_VERSION      1U
#define CHECKPOINT_BODY_SIZE 60U
#define RECEIPT_BODY_SIZE    68U
#define STATE_BODY_SIZE      52U

static const uint8_t checkpoint_magic[8] = {
    'A', 'L', 'R', 'I', 'C', 'H', 'K', 'P'
};
static const uint8_t receipt_magic[8] = {
    'A', 'L', 'R', 'I', 'R', 'C', 'P', 'T'
};
static const uint8_t state_magic[8] = {
    'A', 'L', 'R', 'I', 'A', 'C', 'K', 'S'
};
static const uint8_t checkpoint_domain[] = "ALRIOS-AUDIT-CHECKPOINT-V1";
static const uint8_t receipt_domain[] = "ALRIOS-AUDIT-RECEIPT-V1";
static const uint8_t state_domain[] = "ALRIOS-AUDIT-STATE-V1";

static void store_u16_be(uint8_t *output, uint16_t value) {
    output[0] = (uint8_t)(value >> 8U);
    output[1] = (uint8_t)value;
}

static void store_u32_be(uint8_t *output, uint32_t value) {
    output[0] = (uint8_t)(value >> 24U);
    output[1] = (uint8_t)(value >> 16U);
    output[2] = (uint8_t)(value >> 8U);
    output[3] = (uint8_t)value;
}

static void store_u64_be(uint8_t *output, uint64_t value) {
    size_t index;

    for (index = 8U; index > 0U; --index) {
        output[index - 1U] = (uint8_t)value;
        value >>= 8U;
    }
}

static uint16_t load_u16_be(const uint8_t *input) {
    return (uint16_t)(((uint16_t)input[0] << 8U) | input[1]);
}

static uint32_t load_u32_be(const uint8_t *input) {
    return ((uint32_t)input[0] << 24U) |
           ((uint32_t)input[1] << 16U) |
           ((uint32_t)input[2] << 8U) |
           (uint32_t)input[3];
}

static uint64_t load_u64_be(const uint8_t *input) {
    uint64_t value = 0U;
    size_t index;

    for (index = 0U; index < 8U; ++index) {
        value = (value << 8U) | input[index];
    }
    return value;
}

static int hmac_sha256(const uint8_t key[ALRIOS_FORWARDER_KEY_SIZE],
                       const uint8_t *domain, size_t domain_len,
                       const uint8_t *body, size_t body_len,
                       uint8_t out_mac[ALRIOS_FORWARDER_MAC_SIZE]) {
    EVP_MAC *mac = NULL;
    EVP_MAC_CTX *context = NULL;
    OSSL_PARAM params[2];
    size_t mac_len = 0U;
    int status = ALRIOS_FORWARDER_ERR_AUTH;

    mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
    if (!mac) {
        goto cleanup;
    }
    context = EVP_MAC_CTX_new(mac);
    if (!context) {
        goto cleanup;
    }
    params[0] = OSSL_PARAM_construct_utf8_string("digest", "SHA256", 0U);
    params[1] = OSSL_PARAM_construct_end();
    if (EVP_MAC_init(context, key, ALRIOS_FORWARDER_KEY_SIZE, params) != 1 ||
        EVP_MAC_update(context, domain, domain_len) != 1 ||
        EVP_MAC_update(context, body, body_len) != 1 ||
        EVP_MAC_final(context, out_mac, &mac_len,
                      ALRIOS_FORWARDER_MAC_SIZE) != 1 ||
        mac_len != ALRIOS_FORWARDER_MAC_SIZE) {
        goto cleanup;
    }
    status = ALRIOS_FORWARDER_OK;

cleanup:
    EVP_MAC_CTX_free(context);
    EVP_MAC_free(mac);
    if (status != ALRIOS_FORWARDER_OK) {
        alrios_explicit_zeroize(out_mac, ALRIOS_FORWARDER_MAC_SIZE);
    }
    return status;
}

static int read_exact_at(int fd, uint8_t *buffer, size_t length,
                         uint64_t offset) {
    size_t consumed = 0U;

    while (consumed < length) {
        ssize_t result;
        uint64_t current_offset;

        if (offset > UINT64_MAX - consumed) {
            return ALRIOS_FORWARDER_ERR_OVERFLOW;
        }
        current_offset = offset + consumed;
        if (current_offset > (uint64_t)INT64_MAX) {
            return ALRIOS_FORWARDER_ERR_OVERFLOW;
        }
        result = pread(fd, buffer + consumed, length - consumed,
                       (off_t)current_offset);
        if (result > 0) {
            consumed += (size_t)result;
        } else if (result == 0) {
            return ALRIOS_FORWARDER_ERR_STATE_CORRUPT;
        } else if (errno != EINTR) {
            return ALRIOS_FORWARDER_ERR_IO;
        }
    }
    return ALRIOS_FORWARDER_OK;
}

static int write_exact_at(int fd, const uint8_t *buffer, size_t length,
                          uint64_t offset) {
    size_t consumed = 0U;

    while (consumed < length) {
        ssize_t result;
        uint64_t current_offset;

        if (offset > UINT64_MAX - consumed) {
            return ALRIOS_FORWARDER_ERR_OVERFLOW;
        }
        current_offset = offset + consumed;
        if (current_offset > (uint64_t)INT64_MAX) {
            return ALRIOS_FORWARDER_ERR_OVERFLOW;
        }
        result = pwrite(fd, buffer + consumed, length - consumed,
                        (off_t)current_offset);
        if (result > 0) {
            consumed += (size_t)result;
        } else if (result < 0 && errno == EINTR) {
            continue;
        } else {
            return ALRIOS_FORWARDER_ERR_IO;
        }
    }
    return ALRIOS_FORWARDER_OK;
}

int alrios_checkpoint_encode(
    uint64_t sequence,
    uint64_t timestamp_ns,
    const uint8_t block_hash[ALRIOS_AUDIT_BLOCK_HASH_SIZE],
    const uint8_t key[ALRIOS_FORWARDER_KEY_SIZE],
    uint8_t out_wire[ALRIOS_CHECKPOINT_WIRE_SIZE]) {
    int status;

    if (sequence == 0U || !block_hash || !key || !out_wire) {
        return ALRIOS_FORWARDER_ERR_INVALID_ARGUMENT;
    }
    memset(out_wire, 0, ALRIOS_CHECKPOINT_WIRE_SIZE);
    memcpy(out_wire, checkpoint_magic, sizeof(checkpoint_magic));
    store_u16_be(out_wire + 8U, CHECKPOINT_VERSION);
    store_u16_be(out_wire + 10U, 0U);
    store_u64_be(out_wire + 12U, sequence);
    store_u64_be(out_wire + 20U, timestamp_ns);
    memcpy(out_wire + 28U, block_hash, ALRIOS_AUDIT_BLOCK_HASH_SIZE);
    status = hmac_sha256(key, checkpoint_domain,
                         sizeof(checkpoint_domain) - 1U,
                         out_wire, CHECKPOINT_BODY_SIZE,
                         out_wire + CHECKPOINT_BODY_SIZE);
    if (status != ALRIOS_FORWARDER_OK) {
        alrios_explicit_zeroize(out_wire, ALRIOS_CHECKPOINT_WIRE_SIZE);
    }
    return status;
}

int alrios_checkpoint_decode_and_verify(
    const uint8_t in_wire[ALRIOS_CHECKPOINT_WIRE_SIZE],
    const uint8_t key[ALRIOS_FORWARDER_KEY_SIZE],
    alrios_audit_checkpoint_t *out_checkpoint) {
    uint8_t expected_mac[ALRIOS_FORWARDER_MAC_SIZE];
    int status;

    if (!in_wire || !key || !out_checkpoint) {
        return ALRIOS_FORWARDER_ERR_INVALID_ARGUMENT;
    }
    memset(out_checkpoint, 0, sizeof(*out_checkpoint));
    if (memcmp(in_wire, checkpoint_magic, sizeof(checkpoint_magic)) != 0 ||
        load_u16_be(in_wire + 8U) != CHECKPOINT_VERSION ||
        load_u16_be(in_wire + 10U) != 0U ||
        load_u64_be(in_wire + 12U) == 0U) {
        return ALRIOS_FORWARDER_ERR_AUTH;
    }
    status = hmac_sha256(key, checkpoint_domain,
                         sizeof(checkpoint_domain) - 1U,
                         in_wire, CHECKPOINT_BODY_SIZE, expected_mac);
    if (status != ALRIOS_FORWARDER_OK ||
        CRYPTO_memcmp(expected_mac, in_wire + CHECKPOINT_BODY_SIZE,
                      ALRIOS_FORWARDER_MAC_SIZE) != 0) {
        alrios_explicit_zeroize(expected_mac, sizeof(expected_mac));
        return ALRIOS_FORWARDER_ERR_AUTH;
    }
    out_checkpoint->sequence = load_u64_be(in_wire + 12U);
    out_checkpoint->timestamp_ns = load_u64_be(in_wire + 20U);
    memcpy(out_checkpoint->block_hash, in_wire + 28U,
           ALRIOS_AUDIT_BLOCK_HASH_SIZE);
    memcpy(out_checkpoint->mac, in_wire + CHECKPOINT_BODY_SIZE,
           ALRIOS_FORWARDER_MAC_SIZE);
    alrios_explicit_zeroize(expected_mac, sizeof(expected_mac));
    return ALRIOS_FORWARDER_OK;
}

int alrios_receipt_encode(
    uint64_t ack_sequence,
    const uint8_t ack_hash[ALRIOS_AUDIT_BLOCK_HASH_SIZE],
    const uint8_t nonce[ALRIOS_FORWARDER_RECEIPT_NONCE_SIZE],
    const uint8_t key[ALRIOS_FORWARDER_KEY_SIZE],
    uint8_t out_wire[ALRIOS_RECEIPT_WIRE_SIZE]) {
    int status;

    if (ack_sequence == 0U || !ack_hash || !nonce || !key || !out_wire) {
        return ALRIOS_FORWARDER_ERR_INVALID_ARGUMENT;
    }
    memset(out_wire, 0, ALRIOS_RECEIPT_WIRE_SIZE);
    memcpy(out_wire, receipt_magic, sizeof(receipt_magic));
    store_u16_be(out_wire + 8U, CHECKPOINT_VERSION);
    store_u16_be(out_wire + 10U, 0U);
    store_u64_be(out_wire + 12U, ack_sequence);
    memcpy(out_wire + 20U, ack_hash, ALRIOS_AUDIT_BLOCK_HASH_SIZE);
    memcpy(out_wire + 52U, nonce, ALRIOS_FORWARDER_RECEIPT_NONCE_SIZE);
    status = hmac_sha256(key, receipt_domain, sizeof(receipt_domain) - 1U,
                         out_wire, RECEIPT_BODY_SIZE,
                         out_wire + RECEIPT_BODY_SIZE);
    if (status != ALRIOS_FORWARDER_OK) {
        alrios_explicit_zeroize(out_wire, ALRIOS_RECEIPT_WIRE_SIZE);
    }
    return status;
}

int alrios_receipt_decode_and_verify(
    const uint8_t in_wire[ALRIOS_RECEIPT_WIRE_SIZE],
    const uint8_t key[ALRIOS_FORWARDER_KEY_SIZE],
    alrios_audit_receipt_t *out_receipt) {
    uint8_t expected_mac[ALRIOS_FORWARDER_MAC_SIZE];
    int status;

    if (!in_wire || !key || !out_receipt) {
        return ALRIOS_FORWARDER_ERR_INVALID_ARGUMENT;
    }
    memset(out_receipt, 0, sizeof(*out_receipt));
    if (memcmp(in_wire, receipt_magic, sizeof(receipt_magic)) != 0 ||
        load_u16_be(in_wire + 8U) != CHECKPOINT_VERSION ||
        load_u16_be(in_wire + 10U) != 0U ||
        load_u64_be(in_wire + 12U) == 0U) {
        return ALRIOS_FORWARDER_ERR_AUTH;
    }
    status = hmac_sha256(key, receipt_domain, sizeof(receipt_domain) - 1U,
                         in_wire, RECEIPT_BODY_SIZE, expected_mac);
    if (status != ALRIOS_FORWARDER_OK ||
        CRYPTO_memcmp(expected_mac, in_wire + RECEIPT_BODY_SIZE,
                      ALRIOS_FORWARDER_MAC_SIZE) != 0) {
        alrios_explicit_zeroize(expected_mac, sizeof(expected_mac));
        return ALRIOS_FORWARDER_ERR_AUTH;
    }
    out_receipt->ack_sequence = load_u64_be(in_wire + 12U);
    memcpy(out_receipt->ack_hash, in_wire + 20U,
           ALRIOS_AUDIT_BLOCK_HASH_SIZE);
    memcpy(out_receipt->nonce, in_wire + 52U,
           ALRIOS_FORWARDER_RECEIPT_NONCE_SIZE);
    memcpy(out_receipt->mac, in_wire + RECEIPT_BODY_SIZE,
           ALRIOS_FORWARDER_MAC_SIZE);
    alrios_explicit_zeroize(expected_mac, sizeof(expected_mac));
    return ALRIOS_FORWARDER_OK;
}

int alrios_forwarder_state_read(
    int fd,
    const uint8_t key[ALRIOS_FORWARDER_KEY_SIZE],
    alrios_forwarder_state_t *out_state) {
    uint8_t wire[ALRIOS_FORWARDER_STATE_WIRE_SIZE];
    uint8_t expected_mac[ALRIOS_FORWARDER_MAC_SIZE];
    struct stat file_status;
    int status;

    if (fd < 0 || !key || !out_state) {
        return ALRIOS_FORWARDER_ERR_INVALID_ARGUMENT;
    }
    memset(out_state, 0, sizeof(*out_state));
    if (fstat(fd, &file_status) != 0) {
        return ALRIOS_FORWARDER_ERR_IO;
    }
    if (file_status.st_size == 0) {
        return ALRIOS_FORWARDER_OK;
    }
    if (file_status.st_size != (off_t)sizeof(wire)) {
        return ALRIOS_FORWARDER_ERR_STATE_CORRUPT;
    }
    status = read_exact_at(fd, wire, sizeof(wire), 0U);
    if (status != ALRIOS_FORWARDER_OK) {
        return status;
    }
    if (memcmp(wire, state_magic, sizeof(state_magic)) != 0 ||
        load_u16_be(wire + 8U) != STATE_VERSION ||
        load_u16_be(wire + 10U) != 0U ||
        load_u64_be(wire + 12U) == 0U) {
        alrios_explicit_zeroize(wire, sizeof(wire));
        return ALRIOS_FORWARDER_ERR_STATE_CORRUPT;
    }
    status = hmac_sha256(key, state_domain, sizeof(state_domain) - 1U,
                         wire, STATE_BODY_SIZE, expected_mac);
    if (status != ALRIOS_FORWARDER_OK ||
        CRYPTO_memcmp(expected_mac, wire + STATE_BODY_SIZE,
                      ALRIOS_FORWARDER_MAC_SIZE) != 0) {
        alrios_explicit_zeroize(expected_mac, sizeof(expected_mac));
        alrios_explicit_zeroize(wire, sizeof(wire));
        return ALRIOS_FORWARDER_ERR_STATE_CORRUPT;
    }
    out_state->ack_sequence = load_u64_be(wire + 12U);
    memcpy(out_state->ack_hash, wire + 20U, ALRIOS_AUDIT_BLOCK_HASH_SIZE);
    alrios_explicit_zeroize(expected_mac, sizeof(expected_mac));
    alrios_explicit_zeroize(wire, sizeof(wire));
    return ALRIOS_FORWARDER_OK;
}

int alrios_forwarder_state_write(
    int fd,
    const uint8_t key[ALRIOS_FORWARDER_KEY_SIZE],
    const alrios_forwarder_state_t *state) {
    uint8_t wire[ALRIOS_FORWARDER_STATE_WIRE_SIZE];
    int status;

    if (fd < 0 || !key || !state || state->ack_sequence == 0U) {
        return ALRIOS_FORWARDER_ERR_INVALID_ARGUMENT;
    }
    memset(wire, 0, sizeof(wire));
    memcpy(wire, state_magic, sizeof(state_magic));
    store_u16_be(wire + 8U, STATE_VERSION);
    store_u16_be(wire + 10U, 0U);
    store_u64_be(wire + 12U, state->ack_sequence);
    memcpy(wire + 20U, state->ack_hash, ALRIOS_AUDIT_BLOCK_HASH_SIZE);
    status = hmac_sha256(key, state_domain, sizeof(state_domain) - 1U,
                         wire, STATE_BODY_SIZE, wire + STATE_BODY_SIZE);
    if (status == ALRIOS_FORWARDER_OK) {
        status = write_exact_at(fd, wire, sizeof(wire), 0U);
    }
    if (status == ALRIOS_FORWARDER_OK && ftruncate(fd, (off_t)sizeof(wire)) != 0) {
        status = ALRIOS_FORWARDER_ERR_IO;
    }
    if (status == ALRIOS_FORWARDER_OK && fsync(fd) != 0) {
        status = ALRIOS_FORWARDER_ERR_IO;
    }
    alrios_explicit_zeroize(wire, sizeof(wire));
    return status;
}

int alrios_forwarder_verify_chain_not_rolled_back(
    int audit_fd,
    const alrios_forwarder_state_t *persisted_state) {
    alrios_audit_chain_result_t result;
    int status;

    if (audit_fd < 0 || !persisted_state) {
        return ALRIOS_FORWARDER_ERR_INVALID_ARGUMENT;
    }
    status = alrios_audit_chain_verify_fd(audit_fd, &result);
    if (status != ALRIOS_AUDIT_OK) {
        return ALRIOS_FORWARDER_ERR_ROLLBACK_DETECTED;
    }
    if (persisted_state->ack_sequence == 0U) {
        return ALRIOS_FORWARDER_OK;
    }
    if (result.last_sequence != persisted_state->ack_sequence ||
        CRYPTO_memcmp(result.last_hash, persisted_state->ack_hash,
                      ALRIOS_AUDIT_BLOCK_HASH_SIZE) != 0) {
        return ALRIOS_FORWARDER_ERR_ROLLBACK_DETECTED;
    }
    return ALRIOS_FORWARDER_OK;
}

int alrios_forwarder_socket_send(
    const uint8_t *checkpoint_wire,
    size_t wire_len,
    uint8_t *receipt_buf,
    size_t receipt_buf_capacity,
    size_t *receipt_received_len,
    void *user_ctx) {
    alrios_forwarder_socket_transport_t *transport = user_ctx;
    ArPoll *poll = NULL;
    ArPollEvent event;
    uint8_t frame[4U + ALRIOS_CHECKPOINT_WIRE_SIZE];
    uint8_t response[4U + ALRIOS_RECEIPT_WIRE_SIZE];
    size_t sent = 0U;
    size_t received = 0U;
    size_t expected = 4U;
    uint64_t deadline;
    int status = ALRIOS_FORWARDER_ERR_TRANSPORT;

    if (!checkpoint_wire || wire_len != ALRIOS_CHECKPOINT_WIRE_SIZE ||
        !receipt_buf || receipt_buf_capacity < ALRIOS_RECEIPT_WIRE_SIZE ||
        !receipt_received_len || !transport || transport->socket_fd < 0 ||
        transport->timeout_ms <= 0) {
        return ALRIOS_FORWARDER_ERR_INVALID_ARGUMENT;
    }
    *receipt_received_len = 0U;
    if (ar_socket_set_nonblock(transport->socket_fd) != 0) {
        return ALRIOS_FORWARDER_ERR_TRANSPORT;
    }
    store_u32_be(frame, (uint32_t)wire_len);
    memcpy(frame + 4U, checkpoint_wire, wire_len);
    poll = ar_poll_create();
    if (!poll || ar_poll_add(poll, transport->socket_fd,
                             AR_EVENT_WRITE, transport) != 0) {
        goto cleanup;
    }
    deadline = ar_time_ms() + (uint64_t)transport->timeout_ms;
    while (sent < sizeof(frame)) {
        int wait_ms;
        int count;
        int result;
        uint64_t now = ar_time_ms();

        if (now >= deadline) {
            goto cleanup;
        }
        wait_ms = (int)(deadline - now);
        count = ar_poll_wait(poll, &event, 1, wait_ms);
        if (count != 1 || (event.events & AR_EVENT_ERROR) != 0) {
            goto cleanup;
        }
        result = ar_socket_send(transport->socket_fd, frame + sent,
                                sizeof(frame) - sent);
        if (result > 0) {
            sent += (size_t)result;
        } else if (result != -EAGAIN && result != -EWOULDBLOCK &&
                   result != -EINTR) {
            goto cleanup;
        }
    }
    if (ar_poll_mod(poll, transport->socket_fd, AR_EVENT_READ) != 0) {
        goto cleanup;
    }
    while (received < expected) {
        int wait_ms;
        int count;
        int result;
        uint64_t now = ar_time_ms();

        if (now >= deadline) {
            goto cleanup;
        }
        wait_ms = (int)(deadline - now);
        count = ar_poll_wait(poll, &event, 1, wait_ms);
        if (count != 1 || (event.events & AR_EVENT_ERROR) != 0) {
            goto cleanup;
        }
        result = ar_socket_recv(transport->socket_fd, response + received,
                                expected - received);
        if (result > 0) {
            received += (size_t)result;
            if (received == 4U) {
                if (load_u32_be(response) != ALRIOS_RECEIPT_WIRE_SIZE) {
                    goto cleanup;
                }
                expected = sizeof(response);
            }
        } else if (result != -EAGAIN && result != -EWOULDBLOCK &&
                   result != -EINTR) {
            goto cleanup;
        }
    }
    memcpy(receipt_buf, response + 4U, ALRIOS_RECEIPT_WIRE_SIZE);
    *receipt_received_len = ALRIOS_RECEIPT_WIRE_SIZE;
    status = ALRIOS_FORWARDER_OK;

cleanup:
    if (poll) {
        ar_poll_destroy(poll);
    }
    alrios_explicit_zeroize(response, sizeof(response));
    return status;
}

int alrios_forwarder_init(
    alrios_audit_forwarder_t *fwd,
    alrios_audit_checkpoint_t *queue_buffer,
    size_t queue_capacity,
    const uint8_t key[ALRIOS_FORWARDER_KEY_SIZE],
    int state_fd,
    alrios_forwarder_send_fn send_fn,
    void *transport_ctx) {
    int status;

    if (!fwd || !queue_buffer || queue_capacity == 0U ||
        queue_capacity > ALRIOS_FORWARDER_MAX_QUEUE_ENTRIES || !key ||
        !send_fn || state_fd < -1) {
        return ALRIOS_FORWARDER_ERR_INVALID_ARGUMENT;
    }
    memset(fwd, 0, sizeof(*fwd));
    memset(queue_buffer, 0, queue_capacity * sizeof(*queue_buffer));
    fwd->queue = queue_buffer;
    fwd->queue_capacity = queue_capacity;
    fwd->state_fd = state_fd;
    fwd->send_fn = send_fn;
    fwd->transport_ctx = transport_ctx;
    memcpy(fwd->auth_key, key, ALRIOS_FORWARDER_KEY_SIZE);
    if (state_fd >= 0) {
        status = alrios_forwarder_state_read(state_fd, key,
                                              &fwd->persisted_state);
        if (status != ALRIOS_FORWARDER_OK) {
            alrios_forwarder_destroy(fwd);
            return status;
        }
    }
    return ALRIOS_FORWARDER_OK;
}

int alrios_forwarder_enqueue(
    alrios_audit_forwarder_t *fwd,
    uint64_t sequence,
    uint64_t timestamp_ns,
    const uint8_t block_hash[ALRIOS_AUDIT_BLOCK_HASH_SIZE]) {
    alrios_audit_checkpoint_t *entry;

    if (!fwd || !block_hash || sequence == 0U || !fwd->queue ||
        fwd->queue_capacity == 0U || !fwd->send_fn) {
        return ALRIOS_FORWARDER_ERR_INVALID_ARGUMENT;
    }
    if (fwd->queue_count == fwd->queue_capacity) {
        return ALRIOS_FORWARDER_ERR_QUEUE_FULL;
    }
    entry = &fwd->queue[fwd->queue_tail];
    memset(entry, 0, sizeof(*entry));
    entry->sequence = sequence;
    entry->timestamp_ns = timestamp_ns;
    memcpy(entry->block_hash, block_hash, ALRIOS_AUDIT_BLOCK_HASH_SIZE);
    fwd->queue_tail = (fwd->queue_tail + 1U) % fwd->queue_capacity;
    ++fwd->queue_count;
    return ALRIOS_FORWARDER_OK;
}

int alrios_forwarder_flush(alrios_audit_forwarder_t *fwd) {
    uint8_t checkpoint_wire[ALRIOS_CHECKPOINT_WIRE_SIZE];
    uint8_t receipt_wire[ALRIOS_RECEIPT_WIRE_SIZE];
    alrios_audit_receipt_t receipt;
    alrios_forwarder_state_t new_state;
    alrios_audit_checkpoint_t *entry;
    size_t receipt_len = 0U;
    int status;

    if (!fwd || !fwd->queue || fwd->queue_capacity == 0U || !fwd->send_fn) {
        return ALRIOS_FORWARDER_ERR_INVALID_ARGUMENT;
    }
    if (fwd->queue_count == 0U) {
        return ALRIOS_FORWARDER_OK;
    }
    entry = &fwd->queue[fwd->queue_head];
    status = alrios_checkpoint_encode(entry->sequence, entry->timestamp_ns,
                                       entry->block_hash, fwd->auth_key,
                                       checkpoint_wire);
    if (status != ALRIOS_FORWARDER_OK) {
        goto cleanup;
    }
    status = fwd->send_fn(checkpoint_wire, sizeof(checkpoint_wire),
                          receipt_wire, sizeof(receipt_wire), &receipt_len,
                          fwd->transport_ctx);
    if (status != ALRIOS_FORWARDER_OK || receipt_len != sizeof(receipt_wire)) {
        status = ALRIOS_FORWARDER_ERR_TRANSPORT;
        goto cleanup;
    }
    status = alrios_receipt_decode_and_verify(receipt_wire, fwd->auth_key,
                                               &receipt);
    if (status != ALRIOS_FORWARDER_OK ||
        receipt.ack_sequence != entry->sequence ||
        CRYPTO_memcmp(receipt.ack_hash, entry->block_hash,
                      ALRIOS_AUDIT_BLOCK_HASH_SIZE) != 0) {
        status = ALRIOS_FORWARDER_ERR_AUTH;
        goto cleanup;
    }
    if (fwd->persisted_state.ack_sequence > receipt.ack_sequence) {
        status = ALRIOS_FORWARDER_ERR_ROLLBACK_DETECTED;
        goto cleanup;
    }
    new_state.ack_sequence = receipt.ack_sequence;
    memcpy(new_state.ack_hash, receipt.ack_hash,
           ALRIOS_AUDIT_BLOCK_HASH_SIZE);
    if (fwd->state_fd >= 0) {
        status = alrios_forwarder_state_write(fwd->state_fd, fwd->auth_key,
                                               &new_state);
        if (status != ALRIOS_FORWARDER_OK) {
            goto cleanup;
        }
    }
    fwd->persisted_state = new_state;
    alrios_explicit_zeroize(entry, sizeof(*entry));
    fwd->queue_head = (fwd->queue_head + 1U) % fwd->queue_capacity;
    --fwd->queue_count;
    status = ALRIOS_FORWARDER_OK;

cleanup:
    alrios_explicit_zeroize(checkpoint_wire, sizeof(checkpoint_wire));
    alrios_explicit_zeroize(receipt_wire, sizeof(receipt_wire));
    alrios_explicit_zeroize(&receipt, sizeof(receipt));
    alrios_explicit_zeroize(&new_state, sizeof(new_state));
    return status;
}

void alrios_forwarder_destroy(alrios_audit_forwarder_t *fwd) {
    if (!fwd) {
        return;
    }
    if (fwd->queue && fwd->queue_capacity <= ALRIOS_FORWARDER_MAX_QUEUE_ENTRIES) {
        alrios_explicit_zeroize(fwd->queue,
                                fwd->queue_capacity * sizeof(*fwd->queue));
    }
    alrios_explicit_zeroize(fwd, sizeof(*fwd));
    fwd->state_fd = -1;
}
