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

#include "alrios/memory/sensitive.h"
#include "alrios/crash_report.h"
#include "alrios/crypto_verify.h"

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "CHECK failed [%s:%d]: %s\n", __FILE__, __LINE__, #cond); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void test_sensitive_memory_registration_and_zeroize(void) {
    printf("[*] Running test_sensitive_memory_registration_and_zeroize...\n");

    size_t size = 4096;
    char *secret_buffer = mmap(NULL, size, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK(secret_buffer != MAP_FAILED);

    /* Write secret content */
    const char *secret_str = "SUPER_SECRET_ALRIOS_DATABASE_MASTER_KEY_001";
    snprintf(secret_buffer, alloc_size, "%s", secret_str);

    /* Verify registration */
    CHECK(!alrios_sensitive_is_registered(secret_buffer, size));
    int reg_rc = alrios_sensitive_register(secret_buffer, size);
    CHECK(reg_rc == ALRIOS_SENSITIVE_OK);
    CHECK(alrios_sensitive_is_registered(secret_buffer, size));

    /* Check query ranges */
    alrios_sensitive_range_t ranges[4];
    size_t total = 0;
    CHECK(alrios_sensitive_get_ranges(ranges, 4, &total) == ALRIOS_SENSITIVE_OK);
    CHECK(total >= 1);
    CHECK(ranges[0].ptr == secret_buffer);
    CHECK(ranges[0].size == size);

    /* Verify unregister explicitly zeroizes */
    int unreg_rc = alrios_sensitive_unregister(secret_buffer, size);
    CHECK(unreg_rc == ALRIOS_SENSITIVE_OK);
    CHECK(!alrios_sensitive_is_registered(secret_buffer, size));

    /* Verify that secret memory was wiped */
    for (size_t i = 0; i < size; ++i) {
        CHECK(secret_buffer[i] == 0);
    }

    munmap(secret_buffer, size);
    printf("    [PASS] Sensitive registration & zeroize verified.\n");
}

static void test_crash_report_scrubbing(void) {
    printf("[*] Running test_crash_report_scrubbing...\n");

    /* Allocate secret area and register it */
    char secret_page[512];
    memset(secret_page, 'S', sizeof(secret_page));
    CHECK(alrios_sensitive_register(secret_page, sizeof(secret_page)) == ALRIOS_SENSITIVE_OK);

    /* Prepare crash state pointing into secret memory */
    alrios_crash_state_t state;
    memset(&state, 0, sizeof(state));
    state.signal_number = SIGSEGV;
    state.faulting_pid = 12345;
    state.instruction_pointer = 0x401050ULL;
    state.stack_pointer = (uint64_t)(uintptr_t)(secret_page + 100);
    state.registers[0] = (uint64_t)(uintptr_t)(secret_page + 32);
    state.registers[1] = 0x11223344ULL;
    state.register_count = 2;
    state.stack_frames[0] = (uint64_t)(uintptr_t)(secret_page + 64);
    state.stack_frames[1] = 0x401000ULL;
    state.frame_count = 2;

    /* Put secret data in memory_sample */
    state.memory_sample_address = (uint64_t)(uintptr_t)secret_page;
    state.memory_sample_len = 64;
    memcpy(state.memory_sample, secret_page, 64);

    /* Scrub crash state */
    CHECK(alrios_crash_scrub_state(&state) == ALRIOS_CRASH_OK);

    /* Registered pointer locations must be zeroed/scrubbed */
    CHECK(state.stack_pointer == 0);
    CHECK(state.registers[0] == 0);
    CHECK(state.registers[1] == 0x11223344ULL); /* Non-sensitive preserved */
    CHECK(state.stack_frames[0] == 0);
    CHECK(state.stack_frames[1] == 0x401000ULL); /* Non-sensitive preserved */
    CHECK(state.memory_sample_len == 0);
    for (size_t i = 0; i < sizeof(state.memory_sample); ++i) {
        CHECK(state.memory_sample[i] == 0);
    }

    /* Clean up registry */
    alrios_sensitive_clear_all();
    printf("    [PASS] Crash report scrubbing verified.\n");
}

static void test_crash_report_encryption_and_tamper_rejection(void) {
    printf("[*] Running test_crash_report_encryption_and_tamper_rejection...\n");

    uint8_t cluster_key[ALRIOS_CRASH_KEY_LEN];
    memset(cluster_key, 0x5A, sizeof(cluster_key));

    alrios_crash_state_t original_state;
    memset(&original_state, 0, sizeof(original_state));
    original_state.signal_number = SIGABRT;
    original_state.error_code = 139;
    original_state.faulting_pid = 9876;
    original_state.fault_address = 0x7fffbeefULL;
    original_state.instruction_pointer = 0x555555554000ULL;
    original_state.stack_pointer = 0x7fffffffe000ULL;
    strncpy(original_state.app_id, "com.alrigroup.core", sizeof(original_state.app_id) - 1);
    original_state.registers[0] = 0x1ULL;
    original_state.registers[1] = 0x2ULL;
    original_state.register_count = 2;
    original_state.stack_frames[0] = 0x555555554000ULL;
    original_state.frame_count = 1;

    /* Encrypt container */
    uint8_t *container = NULL;
    size_t container_len = 0;
    int enc_rc = alrios_crash_report_encrypt(&original_state, cluster_key, &container, &container_len);
    CHECK(enc_rc == ALRIOS_CRASH_OK);
    CHECK(container != NULL);
    CHECK(container_len > sizeof(alrios_crash_header_t));

    /* Verify magic header */
    alrios_crash_header_t *hdr = (alrios_crash_header_t *)container;
    CHECK(memcmp(hdr->magic, ALRIOS_CRASH_MAGIC, ALRIOS_CRASH_MAGIC_LEN) == 0);
    CHECK(hdr->version == ALRIOS_CRASH_CONTAINER_VERSION);

    /* Verify authorized decrypt succeeds */
    alrios_crash_state_t decrypted_state;
    int dec_rc = alrios_crash_report_decrypt(container, container_len, cluster_key, &decrypted_state);
    CHECK(dec_rc == ALRIOS_CRASH_OK);
    CHECK(decrypted_state.signal_number == original_state.signal_number);
    CHECK(decrypted_state.faulting_pid == original_state.faulting_pid);
    CHECK(decrypted_state.fault_address == original_state.fault_address);
    CHECK(strcmp(decrypted_state.app_id, "com.alrigroup.core") == 0);
    CHECK(decrypted_state.register_count == 2);
    CHECK(decrypted_state.registers[0] == 1);
    CHECK(decrypted_state.registers[1] == 2);

    /* Case 1: Unauthorized decrypt with wrong key fails closed */
    uint8_t wrong_key[ALRIOS_CRASH_KEY_LEN];
    memset(wrong_key, 0x5B, sizeof(wrong_key));
    alrios_crash_state_t bad_state;
    int wrong_rc = alrios_crash_report_decrypt(container, container_len, wrong_key, &bad_state);
    CHECK(wrong_rc == ALRIOS_CRASH_ERR_AUTH_FAIL);

    /* Case 2: Tampered ciphertext fails closed */
    uint8_t *tampered_ct = malloc(container_len);
    CHECK(tampered_ct != NULL);
    memcpy(tampered_ct, container, container_len);
    tampered_ct[container_len - 1] ^= 0x01; /* Bit flip in ciphertext */
    int tamper_ct_rc = alrios_crash_report_decrypt(tampered_ct, container_len, cluster_key, &bad_state);
    CHECK(tamper_ct_rc == ALRIOS_CRASH_ERR_AUTH_FAIL);
    free(tampered_ct);

    /* Case 3: Tampered tag fails closed */
    uint8_t *tampered_tag = malloc(container_len);
    CHECK(tampered_tag != NULL);
    memcpy(tampered_tag, container, container_len);
    alrios_crash_header_t *tampered_hdr = (alrios_crash_header_t *)tampered_tag;
    tampered_hdr->tag[0] ^= 0xFF;
    int tamper_tag_rc = alrios_crash_report_decrypt(tampered_tag, container_len, cluster_key, &bad_state);
    CHECK(tamper_tag_rc == ALRIOS_CRASH_ERR_AUTH_FAIL);
    free(tampered_tag);

    /* Case 4: Corrupted magic fails closed */
    uint8_t *bad_magic_buf = malloc(container_len);
    CHECK(bad_magic_buf != NULL);
    memcpy(bad_magic_buf, container, container_len);
    bad_magic_buf[0] = 'X';
    int bad_magic_rc = alrios_crash_report_decrypt(bad_magic_buf, container_len, cluster_key, &bad_state);
    CHECK(bad_magic_rc == ALRIOS_CRASH_ERR_BAD_MAGIC);
    free(bad_magic_buf);

    free(container);
    printf("    [PASS] Encryption and fail-closed authentication verified.\n");
}

int main(void) {
    printf("=====================================================\n");
    printf("Starting MP-027 Core Dump & Crash Report Security Tests\n");
    printf("=====================================================\n");

    test_sensitive_memory_registration_and_zeroize();
    test_crash_report_scrubbing();
    test_crash_report_encryption_and_tamper_rejection();

    printf("=====================================================\n");
    printf("All MP-027 Security Verification Tests PASSED!\n");
    printf("=====================================================\n");
    return 0;
}
