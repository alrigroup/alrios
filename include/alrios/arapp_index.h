/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#ifndef ALRIOS_ARAPP_INDEX_H
#define ALRIOS_ARAPP_INDEX_H

#include <stdint.h>
#include <stddef.h>

#define ARAPP_INDEX_MAGIC "ARAPPINDEXv1"
#define ARAPP_INDEX_MAGIC_LEN 12

#pragma pack(push, 1)

typedef struct arapp_index_entry {
    char path[256];
    uint64_t size;
    uint8_t sha256[32];
    uint32_t mode;
    uint32_t flags;
} arapp_index_entry_t;

typedef struct arapp_index_header {
    uint8_t magic[16];
    uint32_t version;
    uint32_t entry_count;
    uint64_t total_size;
} arapp_index_header_t;

#pragma pack(pop)

_Static_assert(sizeof(arapp_index_header_t) == 32, "arapp_index_header_t must be 32 bytes");

int alrios_index_compare_and_hardlink(const char *old_slot, const char *new_slot, const arapp_index_entry_t *entries, size_t count);

#endif /* ALRIOS_ARAPP_INDEX_H */
