/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/events/broker.h"

#include <stdlib.h>
#include <string.h>

typedef struct ar_event_message {
    ar_event_metadata_t metadata;
    uint8_t *payload;
} ar_event_message_t;

struct ar_event_subscription {
    ar_event_broker_t *broker;
    ar_event_identity_t owner;
    uint16_t topic;
    ar_event_overflow_policy_t overflow_policy;
    ar_event_message_t *queue;
    size_t queue_capacity;
    size_t queue_head;
    size_t queue_count;
};

struct ar_event_broker {
    ar_event_broker_config_t config;
    ar_event_authorize_fn authorize;
    void *authorize_context;
    ar_event_subscription_t **subscriptions;
    size_t subscription_count;
    size_t topic_count;
    uint64_t next_sequence;
};

static int valid_identity(const ar_event_identity_t *identity) {
    size_t length = 0U;

    if (identity == NULL) {
        return 0;
    }
    while (length < AR_EVENT_MAX_PRINCIPAL_LEN && identity->principal[length] != '\0') {
        length++;
    }
    return length > 0U && length < AR_EVENT_MAX_PRINCIPAL_LEN;
}

static int identity_equal(const ar_event_identity_t *left, const ar_event_identity_t *right) {
    return left->pid == right->pid &&
           left->uid == right->uid &&
           left->gid == right->gid &&
           strncmp(left->principal, right->principal, AR_EVENT_MAX_PRINCIPAL_LEN) == 0;
}

static int authorize_action(
    const ar_event_broker_t *broker,
    ar_event_authorization_action_t action,
    const ar_event_identity_t *actor,
    const ar_event_identity_t *owner,
    uint16_t topic) {
    if (broker->authorize == NULL) {
        return AR_EVENT_ERR_DENIED;
    }
    return broker->authorize(broker->authorize_context, action, actor, owner, topic) == 1
        ? AR_EVENT_OK
        : AR_EVENT_ERR_DENIED;
}

static void release_message(ar_event_message_t *message) {
    if (message != NULL) {
        free(message->payload);
        memset(message, 0, sizeof(*message));
    }
}

static int topic_present_except(
    const ar_event_broker_t *broker,
    uint16_t topic,
    const ar_event_subscription_t *excluded) {
    size_t index;

    for (index = 0U; index < broker->subscription_count; ++index) {
        if (broker->subscriptions[index] != excluded &&
            broker->subscriptions[index]->topic == topic) {
            return 1;
        }
    }
    return 0;
}

static void release_subscription(ar_event_subscription_t *subscription) {
    size_t index;

    if (subscription == NULL) {
        return;
    }
    for (index = 0U; index < subscription->queue_capacity; ++index) {
        release_message(&subscription->queue[index]);
    }
    free(subscription->queue);
    memset(subscription, 0, sizeof(*subscription));
    free(subscription);
}

int ar_event_broker_create(
    const ar_event_broker_config_t *config,
    ar_event_authorize_fn authorize,
    void *authorize_context,
    ar_event_broker_t **out_broker) {
    ar_event_broker_t *broker;

    if (out_broker == NULL) {
        return AR_EVENT_ERR_INVALID;
    }
    *out_broker = NULL;
    if (config == NULL || authorize == NULL ||
        config->max_topics == 0U || config->max_topics > AR_EVENT_HARD_MAX_TOPICS ||
        config->max_payload_size == 0U || config->max_payload_size > AR_EVENT_HARD_MAX_PAYLOAD ||
        config->max_subscribers == 0U || config->max_subscribers > AR_EVENT_HARD_MAX_SUBSCRIBERS ||
        config->subscriber_queue_depth == 0U ||
        config->subscriber_queue_depth > AR_EVENT_HARD_MAX_QUEUE_DEPTH) {
        return AR_EVENT_ERR_INVALID;
    }

    broker = calloc(1U, sizeof(*broker));
    if (broker == NULL) {
        return AR_EVENT_ERR_NOMEM;
    }
    broker->subscriptions = calloc(config->max_subscribers, sizeof(*broker->subscriptions));
    if (broker->subscriptions == NULL) {
        free(broker);
        return AR_EVENT_ERR_NOMEM;
    }
    broker->config = *config;
    broker->authorize = authorize;
    broker->authorize_context = authorize_context;
    broker->next_sequence = 1U;
    *out_broker = broker;
    return AR_EVENT_OK;
}

void ar_event_broker_destroy(ar_event_broker_t *broker) {
    size_t index;

    if (broker == NULL) {
        return;
    }
    for (index = 0U; index < broker->subscription_count; ++index) {
        release_subscription(broker->subscriptions[index]);
    }
    free(broker->subscriptions);
    memset(broker, 0, sizeof(*broker));
    free(broker);
}

int ar_event_broker_subscribe(
    ar_event_broker_t *broker,
    const ar_event_identity_t *consumer,
    uint16_t topic,
    ar_event_overflow_policy_t overflow_policy,
    ar_event_subscription_t **out_subscription) {
    ar_event_subscription_t *subscription;
    size_t index;

    if (out_subscription == NULL) {
        return AR_EVENT_ERR_INVALID;
    }
    *out_subscription = NULL;
    if (broker == NULL || !valid_identity(consumer) || topic == 0U ||
        (overflow_policy != AR_EVENT_OVERFLOW_DROP_NEWEST &&
         overflow_policy != AR_EVENT_OVERFLOW_DROP_OLDEST)) {
        return AR_EVENT_ERR_INVALID;
    }
    if (authorize_action(broker, AR_EVENT_AUTHORIZE_SUBSCRIBE, consumer, consumer, topic) != AR_EVENT_OK) {
        return AR_EVENT_ERR_DENIED;
    }
    if (broker->subscription_count >= broker->config.max_subscribers) {
        return AR_EVENT_ERR_LIMIT;
    }
    for (index = 0U; index < broker->subscription_count; ++index) {
        if (broker->subscriptions[index]->topic == topic &&
            identity_equal(&broker->subscriptions[index]->owner, consumer)) {
            return AR_EVENT_ERR_EXISTS;
        }
    }
    if (!topic_present_except(broker, topic, NULL) && broker->topic_count >= broker->config.max_topics) {
        return AR_EVENT_ERR_LIMIT;
    }

    subscription = calloc(1U, sizeof(*subscription));
    if (subscription == NULL) {
        return AR_EVENT_ERR_NOMEM;
    }
    subscription->queue = calloc(broker->config.subscriber_queue_depth, sizeof(*subscription->queue));
    if (subscription->queue == NULL) {
        free(subscription);
        return AR_EVENT_ERR_NOMEM;
    }
    subscription->broker = broker;
    subscription->owner = *consumer;
    subscription->topic = topic;
    subscription->overflow_policy = overflow_policy;
    subscription->queue_capacity = broker->config.subscriber_queue_depth;
    broker->subscriptions[broker->subscription_count++] = subscription;
    if (!topic_present_except(broker, topic, subscription)) {
        broker->topic_count++;
    }
    *out_subscription = subscription;
    return AR_EVENT_OK;
}

int ar_event_broker_unsubscribe(
    ar_event_broker_t *broker,
    const ar_event_identity_t *actor,
    ar_event_subscription_t *subscription) {
    size_t index;
    int topic_will_be_removed;

    if (broker == NULL || subscription == NULL || subscription->broker != broker ||
        !valid_identity(actor)) {
        return AR_EVENT_ERR_INVALID;
    }
    if (authorize_action(
            broker,
            AR_EVENT_AUTHORIZE_UNSUBSCRIBE,
            actor,
            &subscription->owner,
            subscription->topic) != AR_EVENT_OK) {
        return AR_EVENT_ERR_DENIED;
    }
    for (index = 0U; index < broker->subscription_count; ++index) {
        if (broker->subscriptions[index] == subscription) {
            break;
        }
    }
    if (index == broker->subscription_count) {
        return AR_EVENT_ERR_NOT_FOUND;
    }

    topic_will_be_removed = !topic_present_except(broker, subscription->topic, subscription);
    broker->subscription_count--;
    broker->subscriptions[index] = broker->subscriptions[broker->subscription_count];
    broker->subscriptions[broker->subscription_count] = NULL;
    if (topic_will_be_removed) {
        broker->topic_count--;
    }
    subscription->broker = NULL;
    release_subscription(subscription);
    return AR_EVENT_OK;
}

static int enqueue_message(
    ar_event_subscription_t *subscription,
    const ar_event_metadata_t *metadata,
    const void *payload,
    size_t payload_len,
    ar_event_emit_result_t *result) {
    size_t tail;
    uint8_t *payload_copy = NULL;

    if (subscription->queue_count == subscription->queue_capacity &&
        subscription->overflow_policy == AR_EVENT_OVERFLOW_DROP_NEWEST) {
        result->rejected_newest++;
        return AR_EVENT_PARTIAL;
    }
    if (payload_len > 0U) {
        payload_copy = malloc(payload_len);
        if (payload_copy == NULL) {
            result->rejected_newest++;
            return AR_EVENT_ERR_NOMEM;
        }
        memcpy(payload_copy, payload, payload_len);
    }
    if (subscription->queue_count == subscription->queue_capacity) {
        release_message(&subscription->queue[subscription->queue_head]);
        subscription->queue_head = (subscription->queue_head + 1U) % subscription->queue_capacity;
        subscription->queue_count--;
        result->dropped_oldest++;
    }
    tail = (subscription->queue_head + subscription->queue_count) % subscription->queue_capacity;
    subscription->queue[tail].metadata = *metadata;
    subscription->queue[tail].payload = payload_copy;
    subscription->queue_count++;
    result->delivered++;
    return AR_EVENT_OK;
}

int ar_event_broker_emit(
    ar_event_broker_t *broker,
    const ar_event_identity_t *producer,
    uint16_t topic,
    uint64_t transaction_id,
    uint64_t timestamp_nanos,
    const void *payload,
    size_t payload_len,
    ar_event_emit_result_t *out_result) {
    ar_event_metadata_t metadata;
    ar_event_emit_result_t result = {0U, 0U, 0U};
    size_t index;
    int status = AR_EVENT_OK;

    if (out_result != NULL) {
        *out_result = result;
    }
    if (broker == NULL || !valid_identity(producer) || topic == 0U ||
        payload_len > broker->config.max_payload_size ||
        (payload_len > 0U && payload == NULL)) {
        return AR_EVENT_ERR_INVALID;
    }
    if (authorize_action(broker, AR_EVENT_AUTHORIZE_EMIT, producer, NULL, topic) != AR_EVENT_OK) {
        return AR_EVENT_ERR_DENIED;
    }
    if (broker->next_sequence == UINT64_MAX) {
        return AR_EVENT_ERR_LIMIT;
    }

    memset(&metadata, 0, sizeof(metadata));
    metadata.topic = topic;
    metadata.sequence = broker->next_sequence++;
    metadata.transaction_id = transaction_id;
    metadata.timestamp_nanos = timestamp_nanos;
    metadata.producer = *producer;
    metadata.payload_len = payload_len;

    for (index = 0U; index < broker->subscription_count; ++index) {
        int enqueue_status;

        if (broker->subscriptions[index]->topic != topic) {
            continue;
        }
        enqueue_status = enqueue_message(
            broker->subscriptions[index], &metadata, payload, payload_len, &result);
        if (enqueue_status != AR_EVENT_OK) {
            status = AR_EVENT_PARTIAL;
        }
    }
    if (out_result != NULL) {
        *out_result = result;
    }
    return status;
}

int ar_event_subscription_receive(
    ar_event_subscription_t *subscription,
    ar_event_metadata_t *out_metadata,
    void *out_payload,
    size_t payload_capacity,
    size_t *out_payload_len) {
    ar_event_message_t *message;

    if (out_payload_len != NULL) {
        *out_payload_len = 0U;
    }
    if (subscription == NULL || subscription->broker == NULL ||
        out_metadata == NULL || out_payload_len == NULL ||
        (payload_capacity > 0U && out_payload == NULL)) {
        return AR_EVENT_ERR_INVALID;
    }
    if (subscription->queue_count == 0U) {
        *out_payload_len = 0U;
        return AR_EVENT_EMPTY;
    }
    message = &subscription->queue[subscription->queue_head];
    *out_payload_len = message->metadata.payload_len;
    if (payload_capacity < message->metadata.payload_len) {
        return AR_EVENT_ERR_BUFFER_TOO_SMALL;
    }
    *out_metadata = message->metadata;
    if (message->metadata.payload_len > 0U) {
        memcpy(out_payload, message->payload, message->metadata.payload_len);
    }
    release_message(message);
    subscription->queue_head = (subscription->queue_head + 1U) % subscription->queue_capacity;
    subscription->queue_count--;
    return AR_EVENT_OK;
}

size_t ar_event_broker_subscription_count(const ar_event_broker_t *broker) {
    return broker == NULL ? 0U : broker->subscription_count;
}

size_t ar_event_broker_topic_count(const ar_event_broker_t *broker) {
    return broker == NULL ? 0U : broker->topic_count;
}

size_t ar_event_subscription_pending(const ar_event_subscription_t *subscription) {
    return subscription == NULL || subscription->broker == NULL ? 0U : subscription->queue_count;
}

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
    size_t i;
    for (i = 8U; i > 0U; --i) {
        output[i - 1U] = (uint8_t)value;
        value >>= 8U;
    }
}

static uint16_t load_u16_be(const uint8_t *input) {
    return (uint16_t)(((uint16_t)input[0] << 8U) | (uint16_t)input[1]);
}

static uint32_t load_u32_be(const uint8_t *input) {
    return ((uint32_t)input[0] << 24U) |
           ((uint32_t)input[1] << 16U) |
           ((uint32_t)input[2] << 8U) |
           (uint32_t)input[3];
}

static uint64_t load_u64_be(const uint8_t *input) {
    uint64_t value = 0U;
    size_t i;
    for (i = 0U; i < 8U; ++i) {
        value = (value << 8U) | (uint64_t)input[i];
    }
    return value;
}

int ar_event_wire_header_encode(
    const ar_event_wire_header_t *header,
    uint8_t out_wire[sizeof(ar_event_wire_header_t)]) {
    size_t plen = 0U;

    if (header == NULL || out_wire == NULL) {
        return AR_EVENT_ERR_INVALID;
    }

    store_u32_be(out_wire + 0U, header->magic);
    store_u16_be(out_wire + 4U, header->version);
    store_u16_be(out_wire + 6U, header->opcode);
    store_u16_be(out_wire + 8U, header->topic);
    store_u16_be(out_wire + 10U, header->flags);
    store_u64_be(out_wire + 12U, header->sequence);
    store_u64_be(out_wire + 20U, header->transaction_id);
    store_u64_be(out_wire + 28U, header->timestamp_nanos);
    store_u32_be(out_wire + 36U, header->producer_pid);
    store_u32_be(out_wire + 40U, header->producer_uid);
    store_u32_be(out_wire + 44U, header->producer_gid);
    store_u32_be(out_wire + 48U, header->payload_len);

    memset(out_wire + 52U, 0, AR_EVENT_MAX_PRINCIPAL_LEN);
    while (plen < AR_EVENT_MAX_PRINCIPAL_LEN - 1U && header->producer_principal[plen] != '\0') {
        out_wire[52U + plen] = (uint8_t)header->producer_principal[plen];
        plen++;
    }
    return AR_EVENT_OK;
}

int ar_event_wire_header_decode(
    const uint8_t in_wire[sizeof(ar_event_wire_header_t)],
    ar_event_wire_header_t *out_header) {
    if (in_wire == NULL || out_header == NULL) {
        return AR_EVENT_ERR_INVALID;
    }

    memset(out_header, 0, sizeof(*out_header));
    out_header->magic = load_u32_be(in_wire + 0U);
    if (out_header->magic != AR_EVENT_WIRE_MAGIC) {
        return AR_EVENT_ERR_INVALID;
    }
    out_header->version = load_u16_be(in_wire + 4U);
    if (out_header->version != AR_EVENT_WIRE_VERSION) {
        return AR_EVENT_ERR_INVALID;
    }
    out_header->opcode = load_u16_be(in_wire + 6U);
    out_header->topic = load_u16_be(in_wire + 8U);
    out_header->flags = load_u16_be(in_wire + 10U);
    out_header->sequence = load_u64_be(in_wire + 12U);
    out_header->transaction_id = load_u64_be(in_wire + 20U);
    out_header->timestamp_nanos = load_u64_be(in_wire + 28U);
    out_header->producer_pid = load_u32_be(in_wire + 36U);
    out_header->producer_uid = load_u32_be(in_wire + 40U);
    out_header->producer_gid = load_u32_be(in_wire + 44U);
    out_header->payload_len = load_u32_be(in_wire + 48U);

    memcpy(out_header->producer_principal, in_wire + 52U, AR_EVENT_MAX_PRINCIPAL_LEN);
    out_header->producer_principal[AR_EVENT_MAX_PRINCIPAL_LEN - 1U] = '\0';
    return AR_EVENT_OK;
}
