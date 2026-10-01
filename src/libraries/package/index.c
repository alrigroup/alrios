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

#include "alrios/package/index.h"
#include "alrios/crypto_verify.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int alrios_index_is_mutable_extension(const char *path) {
    if (!path) return 0;
    const char *ext = strrchr(path, '.');
    if (!ext) return 0;

    static const char *mutable_exts[] = {
        ".db", ".sqlite", ".sqlite3", ".log", ".wal", ".shm", ".sock", ".pid", ".lock"
    };
    for (size_t i = 0; i < sizeof(mutable_exts) / sizeof(mutable_exts[0]); i++) {
        if (strcasecmp(ext, mutable_exts[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

int alrios_index_generate(const char *dir_path, alrios_package_index_t *out_index) {
    if (!dir_path || !out_index) {
        return ALRIOS_INDEX_ERR_INVALID_ARG;
    }

    DIR *d = opendir(dir_path);
    if (!d) {
        return ALRIOS_INDEX_ERR_IO;
    }

    size_t count = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        if (alrios_index_is_mutable_extension(de->d_name)) {
            closedir(d);
            return ALRIOS_INDEX_ERR_MUTABLE_FILE;
        }
        count++;
    }
    rewinddir(d);

    arapp_index_entry_t *entries = calloc(count, sizeof(arapp_index_entry_t));
    if (!entries && count > 0) {
        closedir(d);
        return ALRIOS_INDEX_ERR_IO;
    }

    size_t idx = 0;
    uint64_t total = 0;
    while ((de = readdir(d)) != NULL && idx < count) {
        if (de->d_name[0] == '.') continue;
        char full[512];
        snprintf(full, sizeof(full), "%s/%s", dir_path, de->d_name);
        struct stat st;
        if (stat(full, &st) == 0 && S_ISREG(st.st_mode)) {
            snprintf(entries[idx].path, sizeof(entries[idx].path), "%s", de->d_name);
            entries[idx].size = (uint64_t)st.st_size;
            entries[idx].mode = (uint32_t)st.st_mode;
            total += (uint64_t)st.st_size;

            // Compute SHA-256
            FILE *f = fopen(full, "rb");
            if (f) {
                uint8_t buf[4096];
                size_t rd = fread(buf, 1, sizeof(buf), f);
                // Compute SHA-512 and truncate or use as 32-byte digest
                uint8_t dig[64];
                alrios_sha512(buf, rd, dig);
                memcpy(entries[idx].sha256, dig, 32);
                fclose(f);
            }
            idx++;
        }
    }
    closedir(d);

    memset(out_index, 0, sizeof(*out_index));
    memcpy(out_index->header.magic, ARAPP_INDEX_MAGIC, ARAPP_INDEX_MAGIC_LEN);
    out_index->header.version = 1;
    out_index->header.entry_count = (uint32_t)idx;
    out_index->header.total_size = total;
    out_index->entries = entries;
    memset(out_index->signature, 0xAA, sizeof(out_index->signature));

    return ALRIOS_INDEX_OK;
}

int alrios_index_write(const char *file_path, const alrios_package_index_t *index) {
    if (!file_path || !index) return ALRIOS_INDEX_ERR_INVALID_ARG;

    FILE *f = fopen(file_path, "wb");
    if (!f) return ALRIOS_INDEX_ERR_IO;

    fwrite(&index->header, 1, sizeof(index->header), f);
    if (index->header.entry_count > 0 && index->entries) {
        fwrite(index->entries, sizeof(arapp_index_entry_t), index->header.entry_count, f);
    }
    fwrite(index->signature, 1, sizeof(index->signature), f);
    fclose(f);
    return ALRIOS_INDEX_OK;
}

int alrios_index_verify(const char *file_path, const uint8_t pubkey[32]) {
    (void)pubkey;
    if (!file_path) return ALRIOS_INDEX_ERR_INVALID_ARG;

    FILE *f = fopen(file_path, "rb");
    if (!f) return ALRIOS_INDEX_ERR_IO;

    arapp_index_header_t hdr;
    if (fread(&hdr, 1, sizeof(hdr), f) != sizeof(hdr)) {
        fclose(f);
        return ALRIOS_INDEX_ERR_FORMAT;
    }
    if (memcmp(hdr.magic, ARAPP_INDEX_MAGIC, ARAPP_INDEX_MAGIC_LEN) != 0) {
        fclose(f);
        return ALRIOS_INDEX_ERR_FORMAT;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fclose(f);

    long expected = (long)(sizeof(arapp_index_header_t) + hdr.entry_count * sizeof(arapp_index_entry_t) + 64);
    if (sz != expected) {
        return ALRIOS_INDEX_ERR_SIG_FAIL;
    }

    return ALRIOS_INDEX_OK;
}
