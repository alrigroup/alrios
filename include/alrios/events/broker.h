/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_EVENTS_BROKER_H
#define ALRIOS_EVENTS_BROKER_H

#include <stddef.h>
#include <stdint.h>

#define AR_EVENT_MAX_PRINCIPAL_LEN       64U
#define AR_EVENT_HARD_MAX_TOPICS         4096U
#define AR_EVENT_HARD_MAX_PAYLOAD        65524U
#define AR_EVENT_HARD_MAX_SUBSCRIBERS    4096U
#define AR_EVENT_HARD_MAX_QUEUE_DEPTH    1024U
#define AR_EVENT_WIRE_MAGIC              0x41524556U
#define AR_EVENT_WIRE_VERSION            1U

#define IPC_EVENT_SUBSCRIBE              0x0301U
#define IPC_EVENT_UNSUBSCRIBE            0x0302U
#define IPC_EVENT_EMIT                   0x0303U
#define IPC_EVENT_ACK                    0x0304U
#define IPC_EVENT_NACK                   0x0305U

#define EVENT_APP_DEPLOY_STARTED         0x1001U
#define EVENT_APP_DEPLOYED               0x1002U
#define EVENT_APP_DEPLOY_FAILED          0x1003U
#define EVENT_APP_CRASHED                0x1004U
#define EVENT_APP_DRAIN_STARTED          0x1005U
#define EVENT_APP_DRAIN_COMPLETED        0x1006U
#define EVENT_HOOK_STARTED               0x1101U
#define EVENT_HOOK_COMPLETED             0x1102U
#define EVENT_HOOK_FAILED                0x1103U
#define EVENT_STORAGE_MIGRATION_STARTED  0x1201U
#define EVENT_STORAGE_MIGRATION_COMPLETED 0x1202U
#define EVENT_CDN_PURGE_REQUESTED        0x1301U
#define EVENT_CDN_PURGE_COMPLETED        0x1302U

typedef struct ar_event_broker ar_event_broker_t;
typedef struct ar_event_subscription ar_event_subscription_t;

typedef enum ar_event_status {
    AR_EVENT_OK = 0,
    AR_EVENT_PARTIAL = 1,
    AR_EVENT_EMPTY = 2,
    AR_EVENT_ERR_INVALID = -4001,
    AR_EVENT_ERR_DENIED = -4002,
    AR_EVENT_ERR_LIMIT = -4003,
    AR_EVENT_ERR_EXISTS = -4004,
    AR_EVENT_ERR_NOT_FOUND = -4005,
    AR_EVENT_ERR_BUFFER_TOO_SMALL = -4006,
    AR_EVENT_ERR_NOMEM = -4007
} ar_event_status_t;

typedef enum ar_event_overflow_policy {
    AR_EVENT_OVERFLOW_DROP_NEWEST = 1,
    AR_EVENT_OVERFLOW_DROP_OLDEST = 2
} ar_event_overflow_policy_t;

typedef enum ar_event_authorization_action {
    AR_EVENT_AUTHORIZE_SUBSCRIBE = 1,
    AR_EVENT_AUTHORIZE_UNSUBSCRIBE = 2,
    AR_EVENT_AUTHORIZE_EMIT = 3
} ar_event_authorization_action_t;

typedef struct ar_event_identity {
    uint32_t pid;
    uint32_t uid;
    uint32_t gid;
    char principal[AR_EVENT_MAX_PRINCIPAL_LEN];
} ar_event_identity_t;

typedef struct ar_event_broker_config {
    size_t max_topics;
    size_t max_payload_size;
    size_t max_subscribers;
    size_t subscriber_queue_depth;
} ar_event_broker_config_t;

#pragma pack(push, 1)
typedef struct ar_event_wire_header {
    uint32_t magic;
    uint16_t version;
    uint16_t opcode;
    uint16_t topic;
    uint16_t flags;
    uint64_t sequence;
    uint64_t transaction_id;
    uint64_t timestamp_nanos;
    uint32_t producer_pid;
    uint32_t producer_uid;
    uint32_t producer_gid;
    uint32_t payload_len;
    char producer_principal[AR_EVENT_MAX_PRINCIPAL_LEN];
} ar_event_wire_header_t;
#pragma pack(pop)

typedef struct ar_event_metadata {
    uint16_t topic;
    uint64_t sequence;
    uint64_t transaction_id;
    uint64_t timestamp_nanos;
    ar_event_identity_t producer;
    size_t payload_len;
} ar_event_metadata_t;

typedef struct ar_event_emit_result {
    size_t delivered;
    size_t dropped_oldest;
    size_t rejected_newest;
} ar_event_emit_result_t;

_Static_assert(sizeof(ar_event_wire_header_t) == 116U,
               "ar_event_wire_header_t wire size changed");
_Static_assert(offsetof(ar_event_wire_header_t, producer_principal) == 52U,
               "ar_event_wire_header_t layout changed");

/* The wire format contains scalar values and fixed byte arrays only. All
 * multi-byte fields must be encoded in network byte order at an IPC boundary. */

/* The daemon must derive actor identities from an authenticated server-side
 * channel. Identity fields received in an event wire payload are not trusted. */
typedef int (*ar_event_authorize_fn)(
    void *context,
    ar_event_authorization_action_t action,
    const ar_event_identity_t *actor,
    const ar_event_identity_t *subscription_owner,
    uint16_t topic);

int ar_event_broker_create(
    const ar_event_broker_config_t *config,
    ar_event_authorize_fn authorize,
    void *authorize_context,
    ar_event_broker_t **out_broker);

void ar_event_broker_destroy(ar_event_broker_t *broker);

int ar_event_broker_subscribe(
    ar_event_broker_t *broker,
    const ar_event_identity_t *consumer,
    uint16_t topic,
    ar_event_overflow_policy_t overflow_policy,
    ar_event_subscription_t **out_subscription);

int ar_event_broker_unsubscribe(
    ar_event_broker_t *broker,
    const ar_event_identity_t *actor,
    ar_event_subscription_t *subscription);

int ar_event_broker_emit(
    ar_event_broker_t *broker,
    const ar_event_identity_t *producer,
    uint16_t topic,
    uint64_t transaction_id,
    uint64_t timestamp_nanos,
    const void *payload,
    size_t payload_len,
    ar_event_emit_result_t *out_result);

/* Payload bytes are copied into caller-owned storage. A too-small buffer does
 * not consume the queued event; out_payload_len receives the required size. */
int ar_event_subscription_receive(
    ar_event_subscription_t *subscription,
    ar_event_metadata_t *out_metadata,
    void *out_payload,
    size_t payload_capacity,
    size_t *out_payload_len);

size_t ar_event_broker_subscription_count(const ar_event_broker_t *broker);
size_t ar_event_broker_topic_count(const ar_event_broker_t *broker);
size_t ar_event_subscription_pending(const ar_event_subscription_t *subscription);

#endif /* ALRIOS_EVENTS_BROKER_H */
