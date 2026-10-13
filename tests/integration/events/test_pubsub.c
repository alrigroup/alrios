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

#define CHECK(cond) do { \
    if (!(cond)) { \
        (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                      __FILE__, __LINE__, #cond); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

typedef struct authorization_policy {
    uint16_t denied_topic;
} authorization_policy_t;

static ar_event_identity_t identity(uint32_t pid, uint32_t uid, const char *principal) {
    ar_event_identity_t value;
    int written;

    memset(&value, 0, sizeof(value));
    value.pid = pid;
    value.uid = uid;
    value.gid = uid;
    written = snprintf(value.principal, sizeof(value.principal), "%s", principal);
    CHECK(written > 0 && (size_t)written < sizeof(value.principal));
    return value;
}

static int authorize(
    void *context,
    ar_event_authorization_action_t action,
    const ar_event_identity_t *actor,
    const ar_event_identity_t *subscription_owner,
    uint16_t topic) {
    const authorization_policy_t *policy = context;

    if (actor == NULL || actor->principal[0] == '\0') {
        return 0;
    }
    if (action == AR_EVENT_AUTHORIZE_SUBSCRIBE) {
        return topic != policy->denied_topic && strcmp(actor->principal, "blocked") != 0;
    }
    if (action == AR_EVENT_AUTHORIZE_UNSUBSCRIBE) {
        return subscription_owner != NULL &&
               (strcmp(actor->principal, subscription_owner->principal) == 0 ||
                strcmp(actor->principal, "admin") == 0);
    }
    if (action == AR_EVENT_AUTHORIZE_EMIT) {
        return strncmp(actor->principal, "producer", 8U) == 0;
    }
    return 0;
}

static void assert_receive(
    ar_event_subscription_t *subscription,
    const char *payload,
    const char *producer,
    uint64_t transaction_id) {
    ar_event_metadata_t metadata;
    char buffer[32];
    size_t payload_len = 0U;
    int status;

    memset(&metadata, 0, sizeof(metadata));
    memset(buffer, 0, sizeof(buffer));
    status = ar_event_subscription_receive(
        subscription, &metadata, buffer, sizeof(buffer), &payload_len);
    CHECK(status == AR_EVENT_OK);
    CHECK(payload_len == strlen(payload));
    CHECK(memcmp(buffer, payload, payload_len) == 0);
    CHECK(strcmp(metadata.producer.principal, producer) == 0);
    CHECK(metadata.producer.pid == 200U);
    CHECK(metadata.transaction_id == transaction_id);
}

int main(void) {
    const ar_event_broker_config_t config = {2U, 16U, 4U, 2U};
    authorization_policy_t policy = {0x7777U};
    ar_event_broker_t *broker = NULL;
    ar_event_subscription_t *fast = NULL;
    ar_event_subscription_t *slow_drop_oldest = NULL;
    ar_event_subscription_t *slow_drop_newest = NULL;
    ar_event_subscription_t *other_topic = NULL;
    ar_event_identity_t consumer_fast = identity(100U, 1000U, "consumer-fast");
    ar_event_identity_t consumer_slow = identity(101U, 1001U, "consumer-slow");
    ar_event_identity_t consumer_newest = identity(102U, 1002U, "consumer-newest");
    ar_event_identity_t consumer_other = identity(103U, 1003U, "consumer-other");
    ar_event_identity_t blocked = identity(104U, 1004U, "blocked");
    ar_event_identity_t intruder = identity(105U, 1005U, "intruder");
    ar_event_identity_t producer = identity(200U, 2000U, "producer-deployer");
    ar_event_identity_t denied_producer = identity(201U, 2001U, "consumer-emitter");
    ar_event_emit_result_t result;
    ar_event_metadata_t metadata;
    char source_payload[] = "one";
    char small_buffer[2];
    char receive_buffer[32];
    size_t payload_len = 0U;
    int status;

    CHECK(ar_event_broker_create(&config, authorize, &policy, &broker) == AR_EVENT_OK);
    CHECK(ar_event_broker_subscribe(
        broker, &blocked, policy.denied_topic, AR_EVENT_OVERFLOW_DROP_NEWEST, &fast) ==
        AR_EVENT_ERR_DENIED);
    CHECK(fast == NULL);

    CHECK(ar_event_broker_subscribe(
        broker, &consumer_fast, EVENT_APP_DEPLOYED, AR_EVENT_OVERFLOW_DROP_NEWEST, &fast) ==
        AR_EVENT_OK);
    CHECK(ar_event_broker_subscribe(
        broker, &consumer_slow, EVENT_APP_DEPLOYED, AR_EVENT_OVERFLOW_DROP_OLDEST,
        &slow_drop_oldest) == AR_EVENT_OK);
    CHECK(ar_event_broker_subscribe(
        broker, &consumer_newest, EVENT_APP_DEPLOYED, AR_EVENT_OVERFLOW_DROP_NEWEST,
        &slow_drop_newest) == AR_EVENT_OK);
    CHECK(ar_event_broker_subscribe(
        broker, &consumer_other, EVENT_APP_CRASHED, AR_EVENT_OVERFLOW_DROP_NEWEST,
        &other_topic) == AR_EVENT_OK);
    CHECK(ar_event_broker_topic_count(broker) == 2U);
    CHECK(ar_event_broker_subscription_count(broker) == 4U);

    status = ar_event_broker_emit(
        broker, &denied_producer, EVENT_APP_DEPLOYED, 0U, 0U, "x", 1U, &result);
    CHECK(status == AR_EVENT_ERR_DENIED);
    status = ar_event_broker_emit(
        broker, &producer, EVENT_APP_DEPLOYED, 1U, 10U, source_payload, 3U, &result);
    CHECK(status == AR_EVENT_OK && result.delivered == 3U);
    source_payload[0] = 'X';
    assert_receive(fast, "one", "producer-deployer", 1U);

    CHECK(ar_event_broker_emit(
        broker, &producer, EVENT_APP_DEPLOYED, 2U, 20U, "two", 3U, &result) == AR_EVENT_OK);
    assert_receive(fast, "two", "producer-deployer", 2U);
    CHECK(ar_event_broker_emit(
        broker, &producer, EVENT_APP_DEPLOYED, 3U, 30U, "three", 5U, &result) ==
        AR_EVENT_PARTIAL);
    CHECK(result.delivered == 2U);
    CHECK(result.dropped_oldest == 1U);
    CHECK(result.rejected_newest == 1U);
    assert_receive(fast, "three", "producer-deployer", 3U);

    memset(&metadata, 0, sizeof(metadata));
    status = ar_event_subscription_receive(
        slow_drop_oldest, &metadata, small_buffer, sizeof(small_buffer), &payload_len);
    CHECK(status == AR_EVENT_ERR_BUFFER_TOO_SMALL && payload_len == 3U);
    CHECK(ar_event_subscription_pending(slow_drop_oldest) == 2U);
    assert_receive(slow_drop_oldest, "two", "producer-deployer", 2U);
    assert_receive(slow_drop_oldest, "three", "producer-deployer", 3U);
    assert_receive(slow_drop_newest, "one", "producer-deployer", 1U);
    assert_receive(slow_drop_newest, "two", "producer-deployer", 2U);

    CHECK(ar_event_broker_emit(
        broker, &producer, EVENT_APP_DEPLOYED, 4U, 40U, "payload-too-large", 17U,
        &result) == AR_EVENT_ERR_INVALID);
    CHECK(ar_event_broker_unsubscribe(broker, &intruder, fast) == AR_EVENT_ERR_DENIED);
    CHECK(ar_event_broker_unsubscribe(broker, &consumer_fast, fast) == AR_EVENT_OK);
    fast = NULL;
    CHECK(ar_event_broker_subscription_count(broker) == 3U);

    CHECK(ar_event_broker_emit(
        broker, &producer, EVENT_APP_CRASHED, 5U, 50U, "crash", 5U, &result) ==
        AR_EVENT_OK);
    memset(receive_buffer, 0, sizeof(receive_buffer));
    CHECK(ar_event_subscription_receive(
        other_topic, &metadata, receive_buffer, sizeof(receive_buffer), &payload_len) ==
        AR_EVENT_OK);
    CHECK(payload_len == 5U && memcmp(receive_buffer, "crash", 5U) == 0);

    /* Verify wire header serialization and deserialization */
    {
        ar_event_wire_header_t orig_hdr;
        ar_event_wire_header_t dec_hdr;
        uint8_t wire[sizeof(ar_event_wire_header_t)];

        memset(&orig_hdr, 0, sizeof(orig_hdr));
        orig_hdr.magic = AR_EVENT_WIRE_MAGIC;
        orig_hdr.version = AR_EVENT_WIRE_VERSION;
        orig_hdr.opcode = IPC_EVENT_EMIT;
        orig_hdr.topic = EVENT_APP_DEPLOYED;
        orig_hdr.flags = 0x0102U;
        orig_hdr.sequence = 0x0102030405060708ULL;
        orig_hdr.transaction_id = 0x1122334455667788ULL;
        orig_hdr.timestamp_nanos = 0xAABBCCDDEEFF0011ULL;
        orig_hdr.producer_pid = 12345U;
        orig_hdr.producer_uid = 1000U;
        orig_hdr.producer_gid = 1000U;
        orig_hdr.payload_len = 42U;
        strncpy(orig_hdr.producer_principal, "test-principal", sizeof(orig_hdr.producer_principal) - 1U);

        CHECK(ar_event_wire_header_encode(&orig_hdr, wire) == AR_EVENT_OK);
        CHECK(ar_event_wire_header_decode(wire, &dec_hdr) == AR_EVENT_OK);
        CHECK(dec_hdr.magic == AR_EVENT_WIRE_MAGIC);
        CHECK(dec_hdr.version == AR_EVENT_WIRE_VERSION);
        CHECK(dec_hdr.opcode == IPC_EVENT_EMIT);
        CHECK(dec_hdr.topic == EVENT_APP_DEPLOYED);
        CHECK(dec_hdr.flags == 0x0102U);
        CHECK(dec_hdr.sequence == 0x0102030405060708ULL);
        CHECK(dec_hdr.transaction_id == 0x1122334455667788ULL);
        CHECK(dec_hdr.timestamp_nanos == 0xAABBCCDDEEFF0011ULL);
        CHECK(dec_hdr.producer_pid == 12345U);
        CHECK(dec_hdr.producer_uid == 1000U);
        CHECK(dec_hdr.producer_gid == 1000U);
        CHECK(dec_hdr.payload_len == 42U);
        CHECK(strcmp(dec_hdr.producer_principal, "test-principal") == 0);
    }

    ar_event_broker_destroy(broker);
    puts("MP-022 event broker pub/sub: PASS");
    return 0;
}
