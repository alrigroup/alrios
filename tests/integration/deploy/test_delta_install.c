/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/deploy_layering.h"
#include "alrios/crypto_verify.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

extern int alrios_arpm_install_delta(const char *old_slot,
                                    const char *new_slot,
                                    const arapp_index_entry_t *entries,
                                    size_t count);

int main(void) {
    const char *old_slot = "/tmp/alrios_test_slot_old";
    const char *new_slot = "/tmp/alrios_test_slot_new";
    (void)system("rm -rf /tmp/alrios_test_slot_old /tmp/alrios_test_slot_new");
    mkdir(old_slot, 0755);

    // 1. Setup old slot file
    char old_file[512];
    snprintf(old_file, sizeof(old_file), "%s/payload.bin", old_slot);
    FILE *f = fopen(old_file, "wb");
    assert(f != NULL);
    const char data[] = "DELTA_PAYLOAD_IMMUTABLE_CONTENT";
    fwrite(data, 1, sizeof(data), f);
    fclose(f);

    arapp_index_entry_t entries[1];
    memset(&entries[0], 0, sizeof(entries[0]));
    snprintf(entries[0].path, sizeof(entries[0].path), "payload.bin");
    entries[0].size = sizeof(data);
    entries[0].mode = 0644;
    uint8_t dig[64];
    alrios_sha512((const uint8_t *)data, sizeof(data), dig);
    memcpy(entries[0].sha256, dig, 32);

    // 2. Acceptance Criterion 1: Unchanged files zero-copy (hardlink/reflink)
    int rc = alrios_arpm_install_delta(old_slot, new_slot, entries, 1);
    (void)rc;
    assert(rc == 0);

    // Verify same inode (hardlinked zero-copy)
    char new_file[512];
    snprintf(new_file, sizeof(new_file), "%s/payload.bin", new_slot);
    struct stat st_old, st_new;
    int s1 = stat(old_file, &st_old);
    int s2 = stat(new_file, &st_new);
    (void)s1; (void)s2;
    assert(s1 == 0 && s2 == 0);
    assert(st_old.st_ino == st_new.st_ino); // Hardlinked zero-copy confirmed!

    // 3. Acceptance Criterion 3: Final hash verification
    int vr = alrios_deploy_verify_slot(new_slot, entries, 1);
    (void)vr;
    assert(vr == 0);

    // 4. Acceptance Criterion 4: Rollback leaves active slot untouched
    arapp_index_entry_t bad_entries[1];
    memcpy(&bad_entries[0], &entries[0], sizeof(entries[0]));
    bad_entries[0].sha256[0] ^= 0xFF; // Corrupt expected hash

    const char *fail_slot = "/tmp/alrios_test_slot_fail";
    rc = alrios_arpm_install_delta(old_slot, fail_slot, bad_entries, 1);
    assert(rc != 0); // Fails hash check and triggers rollback

    // Active old slot must be 100% intact!
    assert(access(old_file, F_OK) == 0);
    int s3 = stat(old_file, &st_old);
    (void)s3;
    assert(s3 == 0);

    (void)system("rm -rf /tmp/alrios_test_slot_old /tmp/alrios_test_slot_new /tmp/alrios_test_slot_fail");
    printf("TEST_DELTA_INSTALL: PASS\n");
    return 0;
}
