/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/events/broker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                      __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static int authorize(const char *consumer_id,
                     const char *topic,
                     void *context) {
    (void)context;
    if (strcmp(consumer_id, "denied.consumer") == 0) {
        return 0;
    }
    return strcmp(topic, "app.deployed") == 0;
}

int main(void) {
    static const char payload_one[] = "release-1";
    static const char payload_two[] = "release-2";
    alrios_event_broker_t *broker = NULL;
    alrios_event_t event;
    uint64_t fast_subscription = 0U;
    uint64_t slow_subscription = 0U;
    uint64_t dropped = 0U;
    size_t queued = 0U;
    size_t delivered = 0U;
    size_t emit_dropped = 0U;

    CHECK(alrios_event_broker_create(4U, authorize, NULL, &broker) ==
          ALRIOS_EVENT_OK);
    CHECK(alrios_event_subscribe(broker, "denied.consumer", "app.deployed",
                                 1U, &fast_subscription) ==
          ALRIOS_EVENT_ERR_DENIED);
    CHECK(alrios_event_subscribe(broker, "fast.consumer", "app.deployed",
                                 4U, &fast_subscription) == ALRIOS_EVENT_OK);
    CHECK(alrios_event_subscribe(broker, "slow.consumer", "app.deployed",
                                 1U, &slow_subscription) == ALRIOS_EVENT_OK);

    CHECK(alrios_event_emit(broker, "deploy.supervisor", "app.deployed",
                            payload_one, sizeof(payload_one),
                            &delivered, &emit_dropped) == ALRIOS_EVENT_OK);
    CHECK(delivered == 2U && emit_dropped == 0U);
    CHECK(alrios_event_emit(broker, "deploy.supervisor", "app.deployed",
                            payload_two, sizeof(payload_two),
                            &delivered, &emit_dropped) == ALRIOS_EVENT_OK);
    CHECK(delivered == 1U && emit_dropped == 1U);

    CHECK(alrios_event_receive(broker, fast_subscription, "fast.consumer",
                               &event) == ALRIOS_EVENT_OK);
    CHECK(strcmp(event.producer_id, "deploy.supervisor") == 0);
    CHECK(strcmp(event.topic, "app.deployed") == 0);
    CHECK(event.payload_size == sizeof(payload_one));
    CHECK(memcmp(event.payload, payload_one, sizeof(payload_one)) == 0);
    CHECK(alrios_event_receive(broker, fast_subscription, "fast.consumer",
                               &event) == ALRIOS_EVENT_OK);
    CHECK(memcmp(event.payload, payload_two, sizeof(payload_two)) == 0);

    CHECK(alrios_event_subscription_stats(broker, slow_subscription,
                                          &dropped, &queued) ==
          ALRIOS_EVENT_OK);
    CHECK(dropped == 1U && queued == 1U);
    CHECK(alrios_event_receive(broker, slow_subscription, "fast.consumer",
                               &event) == ALRIOS_EVENT_ERR_DENIED);
    CHECK(alrios_event_unsubscribe(broker, slow_subscription,
                                   "slow.consumer") == ALRIOS_EVENT_OK);
    CHECK(alrios_event_receive(broker, slow_subscription, "slow.consumer",
                               &event) == ALRIOS_EVENT_ERR_NOT_FOUND);
    CHECK(alrios_event_unsubscribe(broker, fast_subscription,
                                   "fast.consumer") == ALRIOS_EVENT_OK);

    alrios_event_broker_destroy(broker);
    (void)printf("TEST_PUBSUB: PASS\n");
    return EXIT_SUCCESS;
}
