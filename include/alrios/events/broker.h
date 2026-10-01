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

#ifdef __cplusplus
extern "C" {
#endif

#define ALRIOS_EVENT_TOPIC_MAX 96U
#define ALRIOS_EVENT_IDENTITY_MAX 96U
#define ALRIOS_EVENT_PAYLOAD_MAX 4096U
#define ALRIOS_EVENT_QUEUE_MAX 1024U

#define ALRIOS_EVENT_OK 0
#define ALRIOS_EVENT_ERR_INVALID -9001
#define ALRIOS_EVENT_ERR_DENIED -9002
#define ALRIOS_EVENT_ERR_EXISTS -9003
#define ALRIOS_EVENT_ERR_NOT_FOUND -9004
#define ALRIOS_EVENT_ERR_CAPACITY -9005
#define ALRIOS_EVENT_ERR_EMPTY -9006
#define ALRIOS_EVENT_ERR_INTERNAL -9007

typedef struct alrios_event {
    uint64_t sequence;
    uint32_t payload_size;
    char topic[ALRIOS_EVENT_TOPIC_MAX];
    char producer_id[ALRIOS_EVENT_IDENTITY_MAX];
    uint8_t payload[ALRIOS_EVENT_PAYLOAD_MAX];
} alrios_event_t;

typedef int (*alrios_event_authorize_fn)(const char *consumer_id,
                                         const char *topic,
                                         void *context);

typedef struct alrios_event_broker alrios_event_broker_t;

int alrios_event_broker_create(size_t max_subscriptions,
                               alrios_event_authorize_fn authorize,
                               void *authorize_context,
                               alrios_event_broker_t **out_broker);

void alrios_event_broker_destroy(alrios_event_broker_t *broker);

int alrios_event_subscribe(alrios_event_broker_t *broker,
                           const char *consumer_id,
                           const char *topic,
                           size_t queue_capacity,
                           uint64_t *out_subscription_id);

int alrios_event_unsubscribe(alrios_event_broker_t *broker,
                             uint64_t subscription_id,
                             const char *consumer_id);

int alrios_event_emit(alrios_event_broker_t *broker,
                      const char *producer_id,
                      const char *topic,
                      const void *payload,
                      size_t payload_size,
                      size_t *out_delivered,
                      size_t *out_dropped);

int alrios_event_receive(alrios_event_broker_t *broker,
                         uint64_t subscription_id,
                         const char *consumer_id,
                         alrios_event_t *out_event);

int alrios_event_subscription_stats(alrios_event_broker_t *broker,
                                    uint64_t subscription_id,
                                    uint64_t *out_dropped,
                                    size_t *out_queued);

#ifdef __cplusplus
}
#endif

#endif
