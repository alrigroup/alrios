/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/events/broker.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALRIOS_EVENT_SUBSCRIPTION_UNUSED 0U

typedef struct alrios_event_subscription {
    uint64_t id;
    char consumer_id[ALRIOS_EVENT_IDENTITY_MAX];
    char topic[ALRIOS_EVENT_TOPIC_MAX];
    alrios_event_t *queue;
    size_t capacity;
    size_t head;
    size_t count;
    uint64_t dropped;
} alrios_event_subscription_t;

struct alrios_event_broker {
    pthread_mutex_t lock;
    alrios_event_subscription_t *subscriptions;
    size_t subscription_capacity;
    uint64_t next_subscription_id;
    uint64_t next_event_sequence;
    alrios_event_authorize_fn authorize;
    void *authorize_context;
};

static int bounded_string_valid(const char *value, size_t capacity) {
    size_t length;

    if (!value || value[0] == '\0') {
        return 0;
    }
    length = strnlen(value, capacity);
    return length > 0U && length < capacity;
}

static alrios_event_subscription_t *find_subscription(alrios_event_broker_t *broker,
                                                       uint64_t subscription_id) {
    size_t index;

    for (index = 0U; index < broker->subscription_capacity; ++index) {
        if (broker->subscriptions[index].id == subscription_id) {
            return &broker->subscriptions[index];
        }
    }
    return NULL;
}

int alrios_event_broker_create(size_t max_subscriptions,
                               alrios_event_authorize_fn authorize,
                               void *authorize_context,
                               alrios_event_broker_t **out_broker) {
    alrios_event_broker_t *broker;

    if (!out_broker || max_subscriptions == 0U ||
        max_subscriptions > ALRIOS_EVENT_QUEUE_MAX) {
        return ALRIOS_EVENT_ERR_INVALID;
    }
    *out_broker = NULL;
    broker = calloc(1U, sizeof(*broker));
    if (!broker) {
        return ALRIOS_EVENT_ERR_INTERNAL;
    }
    broker->subscriptions = calloc(max_subscriptions,
                                   sizeof(*broker->subscriptions));
    if (!broker->subscriptions) {
        free(broker);
        return ALRIOS_EVENT_ERR_INTERNAL;
    }
    if (pthread_mutex_init(&broker->lock, NULL) != 0) {
        free(broker->subscriptions);
        free(broker);
        return ALRIOS_EVENT_ERR_INTERNAL;
    }
    broker->subscription_capacity = max_subscriptions;
    broker->next_subscription_id = 1U;
    broker->next_event_sequence = 1U;
    broker->authorize = authorize;
    broker->authorize_context = authorize_context;
    *out_broker = broker;
    return ALRIOS_EVENT_OK;
}

void alrios_event_broker_destroy(alrios_event_broker_t *broker) {
    size_t index;

    if (!broker) {
        return;
    }
    for (index = 0U; index < broker->subscription_capacity; ++index) {
        free(broker->subscriptions[index].queue);
    }
    (void)pthread_mutex_destroy(&broker->lock);
    free(broker->subscriptions);
    free(broker);
}

int alrios_event_subscribe(alrios_event_broker_t *broker,
                           const char *consumer_id,
                           const char *topic,
                           size_t queue_capacity,
                           uint64_t *out_subscription_id) {
    alrios_event_subscription_t *free_slot = NULL;
    size_t index;

    if (!broker || !out_subscription_id ||
        !bounded_string_valid(consumer_id, ALRIOS_EVENT_IDENTITY_MAX) ||
        !bounded_string_valid(topic, ALRIOS_EVENT_TOPIC_MAX) ||
        queue_capacity == 0U || queue_capacity > ALRIOS_EVENT_QUEUE_MAX) {
        return ALRIOS_EVENT_ERR_INVALID;
    }
    if (broker->authorize &&
        broker->authorize(consumer_id, topic, broker->authorize_context) != 1) {
        return ALRIOS_EVENT_ERR_DENIED;
    }
    if (pthread_mutex_lock(&broker->lock) != 0) {
        return ALRIOS_EVENT_ERR_INTERNAL;
    }
    for (index = 0U; index < broker->subscription_capacity; ++index) {
        alrios_event_subscription_t *candidate = &broker->subscriptions[index];
        if (candidate->id == ALRIOS_EVENT_SUBSCRIPTION_UNUSED) {
            if (!free_slot) {
                free_slot = candidate;
            }
        } else if (strcmp(candidate->consumer_id, consumer_id) == 0 &&
                   strcmp(candidate->topic, topic) == 0) {
            (void)pthread_mutex_unlock(&broker->lock);
            return ALRIOS_EVENT_ERR_EXISTS;
        }
    }
    if (!free_slot) {
        (void)pthread_mutex_unlock(&broker->lock);
        return ALRIOS_EVENT_ERR_CAPACITY;
    }
    free_slot->queue = calloc(queue_capacity, sizeof(*free_slot->queue));
    if (!free_slot->queue) {
        (void)pthread_mutex_unlock(&broker->lock);
        return ALRIOS_EVENT_ERR_INTERNAL;
    }
    free_slot->id = broker->next_subscription_id++;
    free_slot->capacity = queue_capacity;
    (void)snprintf(free_slot->consumer_id, sizeof(free_slot->consumer_id),
                   "%s", consumer_id);
    (void)snprintf(free_slot->topic, sizeof(free_slot->topic), "%s", topic);
    *out_subscription_id = free_slot->id;
    (void)pthread_mutex_unlock(&broker->lock);
    return ALRIOS_EVENT_OK;
}

int alrios_event_unsubscribe(alrios_event_broker_t *broker,
                             uint64_t subscription_id,
                             const char *consumer_id) {
    alrios_event_subscription_t *subscription;

    if (!broker || subscription_id == 0U ||
        !bounded_string_valid(consumer_id, ALRIOS_EVENT_IDENTITY_MAX)) {
        return ALRIOS_EVENT_ERR_INVALID;
    }
    if (pthread_mutex_lock(&broker->lock) != 0) {
        return ALRIOS_EVENT_ERR_INTERNAL;
    }
    subscription = find_subscription(broker, subscription_id);
    if (!subscription) {
        (void)pthread_mutex_unlock(&broker->lock);
        return ALRIOS_EVENT_ERR_NOT_FOUND;
    }
    if (strcmp(subscription->consumer_id, consumer_id) != 0) {
        (void)pthread_mutex_unlock(&broker->lock);
        return ALRIOS_EVENT_ERR_DENIED;
    }
    free(subscription->queue);
    memset(subscription, 0, sizeof(*subscription));
    (void)pthread_mutex_unlock(&broker->lock);
    return ALRIOS_EVENT_OK;
}

int alrios_event_emit(alrios_event_broker_t *broker,
                      const char *producer_id,
                      const char *topic,
                      const void *payload,
                      size_t payload_size,
                      size_t *out_delivered,
                      size_t *out_dropped) {
    alrios_event_t event;
    size_t delivered = 0U;
    size_t dropped = 0U;
    size_t index;

    if (!broker ||
        !bounded_string_valid(producer_id, ALRIOS_EVENT_IDENTITY_MAX) ||
        !bounded_string_valid(topic, ALRIOS_EVENT_TOPIC_MAX) ||
        (payload_size > 0U && !payload) ||
        payload_size > ALRIOS_EVENT_PAYLOAD_MAX) {
        return ALRIOS_EVENT_ERR_INVALID;
    }
    memset(&event, 0, sizeof(event));
    event.payload_size = (uint32_t)payload_size;
    (void)snprintf(event.topic, sizeof(event.topic), "%s", topic);
    (void)snprintf(event.producer_id, sizeof(event.producer_id), "%s",
                   producer_id);
    if (payload_size > 0U) {
        memcpy(event.payload, payload, payload_size);
    }
    if (pthread_mutex_lock(&broker->lock) != 0) {
        return ALRIOS_EVENT_ERR_INTERNAL;
    }
    event.sequence = broker->next_event_sequence++;
    for (index = 0U; index < broker->subscription_capacity; ++index) {
        alrios_event_subscription_t *subscription = &broker->subscriptions[index];
        size_t tail;
        if (subscription->id == ALRIOS_EVENT_SUBSCRIPTION_UNUSED ||
            strcmp(subscription->topic, topic) != 0) {
            continue;
        }
        if (subscription->count == subscription->capacity) {
            subscription->dropped++;
            dropped++;
            continue;
        }
        tail = (subscription->head + subscription->count) % subscription->capacity;
        subscription->queue[tail] = event;
        subscription->count++;
        delivered++;
    }
    (void)pthread_mutex_unlock(&broker->lock);
    if (out_delivered) {
        *out_delivered = delivered;
    }
    if (out_dropped) {
        *out_dropped = dropped;
    }
    return ALRIOS_EVENT_OK;
}

int alrios_event_receive(alrios_event_broker_t *broker,
                         uint64_t subscription_id,
                         const char *consumer_id,
                         alrios_event_t *out_event) {
    alrios_event_subscription_t *subscription;

    if (!broker || !out_event || subscription_id == 0U ||
        !bounded_string_valid(consumer_id, ALRIOS_EVENT_IDENTITY_MAX)) {
        return ALRIOS_EVENT_ERR_INVALID;
    }
    if (pthread_mutex_lock(&broker->lock) != 0) {
        return ALRIOS_EVENT_ERR_INTERNAL;
    }
    subscription = find_subscription(broker, subscription_id);
    if (!subscription) {
        (void)pthread_mutex_unlock(&broker->lock);
        return ALRIOS_EVENT_ERR_NOT_FOUND;
    }
    if (strcmp(subscription->consumer_id, consumer_id) != 0) {
        (void)pthread_mutex_unlock(&broker->lock);
        return ALRIOS_EVENT_ERR_DENIED;
    }
    if (subscription->count == 0U) {
        (void)pthread_mutex_unlock(&broker->lock);
        return ALRIOS_EVENT_ERR_EMPTY;
    }
    *out_event = subscription->queue[subscription->head];
    subscription->head = (subscription->head + 1U) % subscription->capacity;
    subscription->count--;
    (void)pthread_mutex_unlock(&broker->lock);
    return ALRIOS_EVENT_OK;
}

int alrios_event_subscription_stats(alrios_event_broker_t *broker,
                                    uint64_t subscription_id,
                                    uint64_t *out_dropped,
                                    size_t *out_queued) {
    alrios_event_subscription_t *subscription;

    if (!broker || subscription_id == 0U || !out_dropped || !out_queued) {
        return ALRIOS_EVENT_ERR_INVALID;
    }
    if (pthread_mutex_lock(&broker->lock) != 0) {
        return ALRIOS_EVENT_ERR_INTERNAL;
    }
    subscription = find_subscription(broker, subscription_id);
    if (!subscription) {
        (void)pthread_mutex_unlock(&broker->lock);
        return ALRIOS_EVENT_ERR_NOT_FOUND;
    }
    *out_dropped = subscription->dropped;
    *out_queued = subscription->count;
    (void)pthread_mutex_unlock(&broker->lock);
    return ALRIOS_EVENT_OK;
}
