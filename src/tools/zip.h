/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef AR_ZIP_H
#define AR_ZIP_H

#include <stdio.h>

#define ZIP_METHOD_STORED 0

/* .arapp header (16 bytes) prepended before ZIP data */
#define AR_MAGIC        "ALRIGROUP@APP"
#define AR_HEADER_SIZE  16

/* .armake header (20 bytes) for source manifest files */
#define ARMAKE_MAGIC        "ALRIGROUP@APPMAKE"
#define ARMAKE_HEADER_SIZE  20

typedef struct {
    char  name[256];
    int   method;
    int   compressed_size;
    int   uncompressed_size;
    int   crc32;
    int   offset;
} zip_entry_t;

/* ---- WRITE (create archive) ---- */
typedef struct zip_writer zip_writer_t;

zip_writer_t *zip_open(const char *path);
int zip_add_entry(zip_writer_t *z, const char *filename, int method);
int zip_write(zip_writer_t *z, const void *data, int len);
int zip_close(zip_writer_t *z);

/* Open for writing with .arapp header (ALRIGROUP@APP) */
zip_writer_t *zip_open_arapp(const char *path);

/* ---- READ (extract archive) ---- */
typedef struct zip_reader zip_reader_t;

zip_reader_t *zip_reader_open(const char *path);
int           zip_reader_count(zip_reader_t *z);
int           zip_reader_entry(zip_reader_t *z, int idx, zip_entry_t *out);
int           zip_reader_extract(zip_reader_t *z, int idx, const char *outdir);
void          zip_reader_close(zip_reader_t *z);

/* Open for reading, validates .arapp header, skips it */
zip_reader_t *zip_reader_open_arapp(const char *path);

/* Check for .arapp / .armake headers */
int zip_is_arapp(const char *path);
int zip_is_armake(const char *path);

/* Prepend header to an existing file (overwrites in-place) */
int ar_write_header_file(const char *path);
int ar_write_armake_header_file(const char *path);

/* Detect header size: returns AR_HEADER_SIZE, ARMAKE_HEADER_SIZE, or 0 */
int ar_detect_header(const char *path);

/* Write little-endian 16-bit value (useful for building headers) */
void ar_write_le16(unsigned char *p, unsigned short v);

#endif
