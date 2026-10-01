/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/memory/sensitive.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    char dump_buffer[128] = "PUBLIC_CRASH_INFO: system running. API_SECRET=super_secret_12345 remaining state.";
    char secret_pattern[] = "super_secret_12345";
    void *secret_ptr = strstr(dump_buffer, secret_pattern);
    (void)secret_ptr;
    assert(secret_ptr != NULL);

    // 1. Acceptance Criterion 1: Registered secrets absent from report
    assert(alrios_sensitive_register(secret_ptr, strlen(secret_pattern)) == ALRIOS_SENSITIVE_OK);

    uint8_t key[32] = {0x01};
    uint8_t iv[12] = {0x02};
    uint8_t encrypted[256];
    size_t encrypted_size = 0;
    uint8_t tag[16];

    int rc = alrios_sensitive_sanitize_dump(dump_buffer, sizeof(dump_buffer), key, iv, encrypted, &encrypted_size, tag);
    (void)rc;
    assert(rc == ALRIOS_SENSITIVE_OK);

    // Decrypt and verify secret is completely absent (zeroed out)
    uint8_t decrypted[256];
    size_t decrypted_size = 0;
    rc = alrios_sensitive_decrypt_dump(encrypted, encrypted_size, key, iv, tag, decrypted, &decrypted_size);
    assert(rc == ALRIOS_SENSITIVE_OK);

    assert(strstr((char *)decrypted, secret_pattern) == NULL); // Secret successfully scrubbed!

    // 2. Acceptance Criterion 3: Unauthorized decrypt rejected (corrupted tag)
    uint8_t bad_tag[16] = {0xFF};
    rc = alrios_sensitive_decrypt_dump(encrypted, encrypted_size, key, iv, bad_tag, decrypted, &decrypted_size);
    assert(rc == ALRIOS_SENSITIVE_ERR_AUTH); // Unauthorized decryption rejected!

    assert(alrios_sensitive_unregister(secret_ptr) == ALRIOS_SENSITIVE_OK);
    printf("TEST_CORE_DUMP_SANITIZATION: PASS\n");
    return 0;
}
