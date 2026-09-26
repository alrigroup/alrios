/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */

#include "alrios/arapp_format.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

int main(void) {
    /* Total size assertions */
    assert(sizeof(arapp_header_prefix_v1_t) == 136);
    assert(sizeof(arapp_signatures_v1_t) == 3392);
    assert(sizeof(arapp_header_v1_t) == 3528);

    /* Alignment & offset assertions */
    assert(offsetof(arapp_header_v1_t, prefix) == 0);
    assert(offsetof(arapp_header_v1_t, signatures) == 136);

    assert(offsetof(arapp_header_prefix_v1_t, magic) == 0);
    assert(offsetof(arapp_header_prefix_v1_t, version) == 6);
    assert(offsetof(arapp_header_prefix_v1_t, target_arch) == 8);
    assert(offsetof(arapp_header_prefix_v1_t, reserved_alignment) == 10);
    assert(offsetof(arapp_header_prefix_v1_t, flags) == 12);
    assert(offsetof(arapp_header_prefix_v1_t, timestamp_issued) == 16);
    assert(offsetof(arapp_header_prefix_v1_t, timestamp_expiry) == 24);
    assert(offsetof(arapp_header_prefix_v1_t, header_size_bytes) == 32);
    assert(offsetof(arapp_header_prefix_v1_t, entitlements_json_length) == 36);
    assert(offsetof(arapp_header_prefix_v1_t, ciphertext_size_bytes) == 40);
    assert(offsetof(arapp_header_prefix_v1_t, aes_gcm_iv) == 44);
    assert(offsetof(arapp_header_prefix_v1_t, aes_gcm_tag) == 56);
    assert(offsetof(arapp_header_prefix_v1_t, cleartext_sha512) == 72);

    assert(offsetof(arapp_signatures_v1_t, sig_ed25519) == 0);
    assert(offsetof(arapp_signatures_v1_t, sig_ml_dsa_65) == 64);
    assert(offsetof(arapp_signatures_v1_t, padding) == 3373);

    arapp_header_v1_t hdr;
    (void)hdr;
    hdr.prefix.magic[0] = ARAPP_MAGIC_0;
    hdr.prefix.magic[1] = ARAPP_MAGIC_1;
    hdr.prefix.magic[2] = ARAPP_MAGIC_2;
    hdr.prefix.magic[3] = ARAPP_MAGIC_3;
    hdr.prefix.magic[4] = ARAPP_MAGIC_4;
    hdr.prefix.magic[5] = ARAPP_MAGIC_5;

    assert(hdr.prefix.magic[0] == 'A');
    assert(hdr.prefix.magic[1] == 'L');
    assert(hdr.prefix.magic[2] == 'R');
    assert(hdr.prefix.magic[3] == 'I');
    assert(hdr.prefix.magic[4] == 'O');
    assert(hdr.prefix.magic[5] == 'S');

    printf("TASK-002 (ARApp Format Layout): PASS\n");
    return 0;
}
