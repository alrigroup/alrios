/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/package/arapp_v2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static const char valid_manifest[] =
    "{\"app_id\":\"com.alrigroup.testapp\","
    "\"execution_profile\":\"enterprise\","
    "\"execution_ring\":\"devmode_sandbox\","
    "\"ipc\":{\"allowed_peers\":[\"arws\"]},"
    "\"network\":{\"allow_inbound\":true,\"allow_outbound\":true,\"ports\":[]},"
    "\"resources\":{\"cpu_weight\":100,\"max_threads\":4,\"memory_burst_mb\":128,\"memory_guaranteed_mb\":64},"
    "\"storage\":{\"persistent_mount\":\"var/data/test\",\"quota_mb\":512},"
    "\"vault\":{\"secrets\":[\"SECRET_KEY\"]},"
    "\"version\":\"1.0.0\"}";

int armake_validate_and_canonicalize_manifest(const char *raw_json, char *out_canon, size_t max_out);

static void test_manifest_validation_and_canonicalization(void) {
    char canon[8192];
    CHECK(armake_validate_and_canonicalize_manifest(valid_manifest, canon, sizeof(canon)) == 0);
    CHECK(strstr(canon, "\"app_id\":\"com.alrigroup.testapp\"") != NULL);

    const char invalid_manifest[] =
        "{\"app_id\":\"../../../etc/passwd\","
        "\"execution_profile\":\"enterprise\","
        "\"execution_ring\":\"devmode_sandbox\","
        "\"ipc\":{\"allowed_peers\":[\"arws\"]},"
        "\"network\":{\"allow_inbound\":true,\"allow_outbound\":true,\"ports\":[]},"
        "\"resources\":{\"cpu_weight\":100,\"max_threads\":4,\"memory_burst_mb\":128,\"memory_guaranteed_mb\":64},"
        "\"storage\":{\"persistent_mount\":\"var/data/test\",\"quota_mb\":512},"
        "\"vault\":{\"secrets\":[\"SECRET_KEY\"]},"
        "\"version\":\"1.0.0\"}";

    CHECK(armake_validate_and_canonicalize_manifest(invalid_manifest, canon, sizeof(canon)) != 0);
}

static void test_envelope_roundtrip(void) {
    arapp_v2_header_prefix_t prefix;
    arapp_v2_package_t pkg;
    uint8_t buffer[16384];
    size_t written = 0U;
    const uint8_t dummy_payload[64] = { 0x12, 0x34 };

    memset(&prefix, 0, sizeof(prefix));
    memcpy(prefix.magic, "ALRIGROUP@ARAPP\0", ARAPP_V2_MAGIC_LEN);
    prefix.format_version = ARAPP_V2_FORMAT_VERSION;
    prefix.target_arch = ARAPP_V2_ARCH_X86_64;
    prefix.container_flags = ARAPP_V2_FLAG_PROFILE_ENTERPRISE;
    prefix.timestamp_issued = 1700000000ULL;
    prefix.timestamp_expiry = 1800000000ULL;
    prefix.header_size_bytes = ARAPP_V2_HEADER_PREFIX_SIZE;
    prefix.entitlements_json_length = (uint32_t)strlen(valid_manifest);
    prefix.ciphertext_size_bytes = sizeof(dummy_payload);

    CHECK(arapp_v2_build(&prefix, NULL, valid_manifest, strlen(valid_manifest),
                         dummy_payload, sizeof(dummy_payload), buffer, sizeof(buffer), &written) == ARAPP_V2_OK);

    CHECK(arapp_v2_read_and_validate(buffer, written, 1700050000ULL, &pkg) == ARAPP_V2_OK);
    CHECK(pkg.format_version == ARAPP_V2_FORMAT_VERSION);
    CHECK(pkg.has_signatures == 0U);
}

int main(void) {
    test_manifest_validation_and_canonicalization();
    test_envelope_roundtrip();
    printf("MP-007 (Upgrade armake to emit unsigned immutable .arapp v2 envelopes): PASS\n");
    return 0;
}
