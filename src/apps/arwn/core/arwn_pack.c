/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arwn_pack.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define ARWEB_HEADER_SIZE   48
#define ARWEB_ENTRY_SIZE    48
#define ARWEB_MAX_PAYLOAD   ARWN_PACK_MAX_PAYLOAD

/* ------------------------------------------------------------------ */
/* CRC32 (ISO-HDLC, polinômio 0xEDB88320)                              */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* CRC32 (ISO-HDLC, polinômio 0xEDB88320)                              */
/* ------------------------------------------------------------------ */

uint32_t arwn_crc32(const void *data, size_t len) {
    static uint32_t table[256];
    static int init = 0;
    if (!init) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) {
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            table[i] = c;
        }
        init = 1;
    }

    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

/* ------------------------------------------------------------------ */
/* Layout binário:                                                     */
/*   header 48B: magic[16] + ver u16 + flags u16 + count u16 +         */
/*               header_crc u32 (crc dos primeiros 22 bytes) +         */
/*               table_off u32 + payload_off u32 + payload_size u32 +  */
/*               reserved u32                                          */
/*   entries 48B cada: name[32] + offset u32 + size u32 + crc u32 +    */
/*               compressed u8 + reserved[3]                           */
/*   payload: bytes das seções em ordem                                */
/* ------------------------------------------------------------------ */

static void wr_u16(uint8_t *d, uint16_t v) {
    d[0] = (uint8_t)(v & 0xFF);
    d[1] = (uint8_t)((v >> 8) & 0xFF);
}

static void wr_u32(uint8_t *d, uint32_t v) {
    d[0] = (uint8_t)(v & 0xFF);
    d[1] = (uint8_t)((v >> 8) & 0xFF);
    d[2] = (uint8_t)((v >> 16) & 0xFF);
    d[3] = (uint8_t)((v >> 24) & 0xFF);
}

static uint16_t rd_u16(const uint8_t *d) {
    return (uint16_t)(d[0] | (d[1] << 8));
}

static uint32_t rd_u32(const uint8_t *d) {
    return (uint32_t)d[0] | ((uint32_t)d[1] << 8) |
           ((uint32_t)d[2] << 16) | ((uint32_t)d[3] << 24);
}

int arwn_pack_build(const arwn_pack_section_t *sections, int count,
                    uint8_t *out, size_t out_cap, size_t *out_len) {
    if (!sections || count < 0 || !out || !out_len) return -1;
    if (count > ARWN_ARWEB_MAX_SECTIONS) return -1;

    uint64_t payload_total = 0;
    for (int i = 0; i < count; i++) {
        payload_total += sections[i].size;
    }
    uint64_t total = (uint64_t)ARWEB_HEADER_SIZE +
                     (uint64_t)count * ARWEB_ENTRY_SIZE + payload_total;
    if (total > ARWEB_MAX_PAYLOAD) return -1;
    if (total > out_cap) return -1;

    uint32_t table_off = ARWEB_HEADER_SIZE;
    uint32_t payload_off = (uint32_t)(ARWEB_HEADER_SIZE + (uint64_t)count * ARWEB_ENTRY_SIZE);

    /* header */
    memcpy(out, ARWN_ARWEB_MAGIC, 16);
    wr_u16(out + 16, ARWN_ARWEB_VERSION);
    wr_u16(out + 18, 0);
    wr_u16(out + 20, (uint16_t)count);
    wr_u32(out + 26, table_off);
    wr_u32(out + 30, payload_off);
    wr_u32(out + 34, (uint32_t)payload_total);
    wr_u32(out + 38, 0);
    wr_u32(out + 22, arwn_crc32(out, 22));

    /* entries + payload */
    uint32_t cur = payload_off;
    for (int i = 0; i < count; i++) {
        uint8_t *e = out + table_off + (uint32_t)i * ARWEB_ENTRY_SIZE;
        memset(e, 0, ARWEB_ENTRY_SIZE);
        size_t nl = strlen(sections[i].name);
        if (nl > ARWN_ARWEB_NAME_MAX) return -1;
        memcpy(e, sections[i].name, nl);
        wr_u32(e + 32, cur);
        wr_u32(e + 36, sections[i].size);
        wr_u32(e + 40, arwn_crc32(sections[i].data, sections[i].size));
        e[44] = 0; /* compressed: não comprimido no MVP */

        if (sections[i].size > 0) {
            memcpy(out + cur, sections[i].data, sections[i].size);
            cur += sections[i].size;
        }
    }

    *out_len = (size_t)total;
    return 0;
}

int arwn_pack_validate(const uint8_t *data, size_t len) {
    if (!data || len < ARWEB_HEADER_SIZE) return -1;
    if (memcmp(data, ARWN_ARWEB_MAGIC, 16) != 0) return -1;
    if (rd_u16(data + 16) != ARWN_ARWEB_VERSION) return -1;

    if (arwn_crc32(data, 22) != rd_u32(data + 22)) return -1;

    uint16_t count = rd_u16(data + 20);
    if (count > ARWN_ARWEB_MAX_SECTIONS) return -1;

    uint32_t table_off = rd_u32(data + 26);
    uint32_t payload_off = rd_u32(data + 30);
    uint32_t payload_size = rd_u32(data + 34);

    /* clamp: tabela e payload dentro do buffer */
    if (table_off != ARWEB_HEADER_SIZE) return -1;
    if ((uint64_t)payload_off + payload_size > len) return -1;
    if ((uint64_t)table_off + (uint64_t)count * ARWEB_ENTRY_SIZE > payload_off) return -1;

    uint32_t end = payload_off;
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *e = data + table_off + i * ARWEB_ENTRY_SIZE;
        uint32_t off = rd_u32(e + 32);
        uint32_t size = rd_u32(e + 36);
        uint32_t crc = rd_u32(e + 40);

        /* nome terminado em NUL dentro do campo */
        int has_nul = 0;
        for (int k = 0; k < ARWN_ARWEB_NAME_MAX; k++) {
            if (e[k] == 0) { has_nul = 1; break; }
        }
        if (!has_nul) return -1;

        /* clamp de offset+size (sem overflow u64) */
        if ((uint64_t)off + size > len) return -1;
        if (off < end) return -1; /* seções em ordem, sem overlap */
        end = off + size;

        if (arwn_crc32(data + off, size) != crc) return -1;
    }

    (void)payload_size;
    return 0;
}

int arwn_pack_index(const uint8_t *data, size_t len,
                    arwn_pack_section_t *views, int views_cap) {
    if (!data || len < ARWEB_HEADER_SIZE || !views || views_cap <= 0)
        return -1;
    if (arwn_pack_validate(data, len) != 0) return -1;

    uint16_t count = rd_u16(data + 20);
    if (count > ARWN_ARWEB_MAX_SECTIONS) return -1;
    if (count > views_cap) return -1;

    uint32_t table_off = rd_u32(data + 26);
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *e = data + table_off + i * ARWEB_ENTRY_SIZE;
        size_t nl = 0;
        while (nl < ARWN_ARWEB_NAME_MAX && e[nl] != 0) nl++;
        if (nl == 0 || nl > ARWN_ARWEB_NAME_MAX) return -1;
        memcpy(views[i].name, e, nl);
        views[i].name[nl] = '\0';
        views[i].data = data + rd_u32(e + 32);
        views[i].size = rd_u32(e + 36);
    }
    return (int)count;
}