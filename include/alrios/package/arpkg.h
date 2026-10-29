/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_PACKAGE_ARPKG_H
#define ALRIOS_PACKAGE_ARPKG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ARPKG_MAGIC "ALRIGROUP@ARPKG3"
#define ARPKG_MAGIC_LEN 16U

#define ARPKG_VERSION 3U

#define ARPKG_MODE_NORMAL    0x0001U
#define ARPKG_MODE_ENCRYPTED 0x0002U

#define ARPKG_FLAG_CLOUD_BOUND   0x0001U
#define ARPKG_FLAG_MACHINE_BOUND 0x0002U

#define ARPKG_SALT_LEN 32U
#define ARPKG_IV_LEN   12U
#define ARPKG_TAG_LEN  16U

#pragma pack(push, 1)
typedef struct {
    char     magic[16];
    uint16_t version;
    uint16_t mode;           /* ARPKG_MODE_NORMAL or ARPKG_MODE_ENCRYPTED */
    uint32_t flags;
    uint64_t created_at;
    uint32_t file_count;
    uint64_t payload_size;
    uint8_t  salt[ARPKG_SALT_LEN];
    uint8_t  iv[ARPKG_IV_LEN];
    uint8_t  tag[ARPKG_TAG_LEN];
    char     cloud_app_id[32];
    char     cloud_tenant[32];
    uint8_t  reserved[64];
} arpkg_header_t;

typedef struct {
    uint16_t path_len;
    char     path[256];
    uint32_t mode;
    uint64_t file_size;
    uint8_t  sha256[32];
} arpkg_toc_entry_t;
#pragma pack(pop)

/* Inspection */
int arpkg_is_arpkg(const char *path);
int arpkg_get_header(const char *path, arpkg_header_t *out_header);

/* Packaging */
int arpkg_pack_normal(const char *source_dir, const char *output_file);
int arpkg_pack_encrypted(const char *source_dir, const char *output_file,
                        const char *passphrase, const char *cloud_app_id,
                        const char *cloud_tenant);

/* Unpacking / Extraction */
int arpkg_unpack(const char *package_file, const char *dest_dir,
                 const char *passphrase);

#ifdef __cplusplus
}
#endif

#endif /* ALRIOS_PACKAGE_ARPKG_H */
