/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "zip.h"
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif
#include <string.h>
#include <errno.h>

/* --- CRC32 --- */
static unsigned int crc32_tab[] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82e07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

static unsigned int crc32(unsigned int crc, const void *buf, int len) {
    const unsigned char *p = (const unsigned char *)buf;
    crc ^= 0xFFFFFFFF;
    while (len--) crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFF;
}

/* --- Helpers --- */
void ar_write_le16(unsigned char *p, unsigned short v) {
    p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF;
}
static void write_le32(unsigned char *p, unsigned int v) {
    p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF; p[3] = (v >> 24) & 0xFF;
}
static unsigned short read_le16(const unsigned char *p) {
    return (unsigned short)(p[0] | ((unsigned short)p[1] << 8));
}
static unsigned int read_le32(const unsigned char *p) {
    return (unsigned int)(p[0] | ((unsigned int)p[1] << 8)
           | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24));
}

/* ======================== WRITER ======================== */
struct zip_writer {
    FILE *fp;
    int   entry_count;
    int   file_count;
    long  *offsets;        /* local header offset per file */
    char  **names;
    int    *crcs;
    int    *sizes_comp;
    int    *sizes_uncomp;
};

zip_writer_t *zip_open(const char *path) {
    zip_writer_t *z = calloc(1, sizeof(zip_writer_t));
    if (!z) return NULL;
    z->fp = fopen(path, "wb");
    if (!z->fp) { free(z); return NULL; }
    z->entry_count = 0;
    z->file_count = 0;
    z->offsets = NULL;
    z->names = NULL;
    z->crcs = NULL;
    z->sizes_comp = NULL;
    z->sizes_uncomp = NULL;
    return z;
}

int zip_add_entry(zip_writer_t *z, const char *filename, int method) {
    int name_len = (int)strlen(filename);
    unsigned char hdr[30];

    memset(hdr, 0, 30);
    hdr[0] = 'P'; hdr[1] = 'K';
    hdr[2] = 3; hdr[3] = 4;               /* local header sig */
    hdr[4] = 20; hdr[5] = 0;               /* version needed 2.0 */
    ar_write_le16(hdr + 8, method);            /* compression method */
    ar_write_le16(hdr + 26, (unsigned short)name_len); /* filename len */

    int padding = 0; /* no data descriptor; we know sizes up front (STORED) */
    long offset = ftell(z->fp);

    fwrite(hdr, 1, 30, z->fp);
    fwrite(filename, 1, name_len, z->fp);

    /* grow metadata arrays */
    z->file_count++;
    z->offsets = realloc(z->offsets, z->file_count * sizeof(long));
    z->names = realloc(z->names, z->file_count * sizeof(char *));
    z->crcs = realloc(z->crcs, z->file_count * sizeof(int));
    z->sizes_comp = realloc(z->sizes_comp, z->file_count * sizeof(int));
    z->sizes_uncomp = realloc(z->sizes_uncomp, z->file_count * sizeof(int));
    if (!z->offsets || !z->names || !z->crcs || !z->sizes_comp || !z->sizes_uncomp)
        return -1;

    z->offsets[z->file_count - 1] = offset;
    z->names[z->file_count - 1] = strdup(filename);
    z->crcs[z->file_count - 1] = 0;
    z->sizes_comp[z->file_count - 1] = 0;
    z->sizes_uncomp[z->file_count - 1] = 0;
    z->entry_count++;

    return 0;
}

int zip_write(zip_writer_t *z, const void *data, int len) {
    if (z->file_count == 0) return -1;
    fwrite(data, 1, len, z->fp);
    int idx = z->file_count - 1;
    z->crcs[idx] = crc32(z->crcs[idx], data, len);
    z->sizes_comp[idx] += len;
    z->sizes_uncomp[idx] += len;
    return 0;
}

int zip_close(zip_writer_t *z) {
    if (!z) return -1;

    /* patch local headers with correct CRC and sizes */
    for (int i = 0; i < z->file_count; i++) {
        long pos = ftell(z->fp);
        fseek(z->fp, z->offsets[i] + 14, SEEK_SET);
        unsigned char buf[12];
        write_le32(buf, (unsigned int)z->crcs[i]);
        write_le32(buf + 4, (unsigned int)z->sizes_comp[i]);
        write_le32(buf + 8, (unsigned int)z->sizes_uncomp[i]);
        fwrite(buf, 1, 12, z->fp);
    }

    /* seek back to end of file after patching */
    fseek(z->fp, 0, SEEK_END);
    long cd_offset = ftell(z->fp);

    /* write central directory */
    for (int i = 0; i < z->file_count; i++) {
        int name_len = (int)strlen(z->names[i]);
        unsigned char cd[46];
        memset(cd, 0, 46);
        cd[0] = 'P'; cd[1] = 'K';
        cd[2] = 1; cd[3] = 2;              /* central dir sig */
        ar_write_le16(cd + 4, 20);             /* version made by */
        ar_write_le16(cd + 6, 20);             /* version needed */
        ar_write_le16(cd + 10, ZIP_METHOD_STORED);
        write_le32(cd + 16, (unsigned int)z->crcs[i]);
        write_le32(cd + 20, (unsigned int)z->sizes_comp[i]);
        write_le32(cd + 24, (unsigned int)z->sizes_uncomp[i]);
        ar_write_le16(cd + 28, (unsigned short)name_len);
        ar_write_le16(cd + 30, 0);             /* extra field len */
        ar_write_le16(cd + 32, 0);             /* comment len */
        ar_write_le16(cd + 34, 0);             /* disk start */
        ar_write_le16(cd + 36, 0);             /* internal attrs */
        write_le32(cd + 38, 0);             /* external attrs */
        write_le32(cd + 42, (unsigned int)z->offsets[i]); /* local hdr offset */
        fwrite(cd, 1, 46, z->fp);
        fwrite(z->names[i], 1, name_len, z->fp);
    }

    long cd_end = ftell(z->fp);
    int cd_size = (int)(cd_end - cd_offset);

    /* EOCD */
    unsigned char eocd[22];
    memset(eocd, 0, 22);
    eocd[0] = 'P'; eocd[1] = 'K';
    eocd[2] = 5; eocd[3] = 6;              /* EOCD sig */
    ar_write_le16(eocd + 4, 0);                /* disk num */
    ar_write_le16(eocd + 6, 0);                /* disk of CD */
    ar_write_le16(eocd + 8, (unsigned short)z->file_count); /* total entries this disk */
    ar_write_le16(eocd + 10, (unsigned short)z->file_count); /* total entries */
    write_le32(eocd + 12, (unsigned int)cd_size);
    write_le32(eocd + 16, (unsigned int)cd_offset);
    ar_write_le16(eocd + 20, 0);               /* comment length */
    fwrite(eocd, 1, 22, z->fp);

    fclose(z->fp);

    for (int i = 0; i < z->file_count; i++)
        free(z->names[i]);
    free(z->offsets);
    free(z->names);
    free(z->crcs);
    free(z->sizes_comp);
    free(z->sizes_uncomp);
    free(z);
    return 0;
}

/* ======================== .ARAPP HEADER ======================== */

zip_writer_t *zip_open_arapp(const char *path) {
    zip_writer_t *z = zip_open(path);
    if (!z) return NULL;
    /* header: magic(13) + reserved(1) + version(2) = 16 bytes */
    unsigned char hdr[16];
    memset(hdr, 0, 16);
    memcpy(hdr, "ALRIGROUP@APP", 13);
    ar_write_le16(hdr + 14, 0x0001);        /* version 1.0 */
    fwrite(hdr, 1, 16, z->fp);
    return z;
}

int zip_is_arapp(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    unsigned char hdr[16];
    int ok = (fread(hdr, 1, 16, f) == 16
              && memcmp(hdr, "ALRIGROUP@APP", 13) == 0);
    fclose(f);
    return ok;
}

zip_reader_t *zip_reader_open_arapp(const char *path) {
    if (!zip_is_arapp(path)) return NULL;
    return zip_reader_open(path);
}

int ar_write_header_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *content = (unsigned char *)malloc((size_t)len + 1);
    if (!content) { fclose(f); return -1; }
    fread(content, 1, (size_t)len, f);
    fclose(f);

    unsigned char hdr[16];
    memset(hdr, 0, 16);
    memcpy(hdr, "ALRIGROUP@APP", 13);
    ar_write_le16(hdr + 14, 0x0001);

    f = fopen(path, "wb");
    if (!f) { free(content); return -1; }
    fwrite(hdr, 1, 16, f);
    fwrite(content, 1, (size_t)len, f);
    fclose(f);
    free(content);
    return 0;
}

int zip_is_armake(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    unsigned char hdr[20];
    int ok = (fread(hdr, 1, 20, f) == 20
              && memcmp(hdr, "ALRIGROUP@APPMAKE", 17) == 0);
    fclose(f);
    return ok;
}

int ar_write_armake_header_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *content = (unsigned char *)malloc((size_t)len + 1);
    if (!content) { fclose(f); return -1; }
    fread(content, 1, (size_t)len, f);
    fclose(f);

    unsigned char hdr[20];
    memset(hdr, 0, 20);
    memcpy(hdr, "ALRIGROUP@APPMAKE", 17);
    ar_write_le16(hdr + 18, 0x0001);

    f = fopen(path, "wb");
    if (!f) { free(content); return -1; }
    fwrite(hdr, 1, 20, f);
    fwrite(content, 1, (size_t)len, f);
    fclose(f);
    free(content);
    return 0;
}

int ar_detect_header(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    unsigned char hdr[20];
    int n = (int)fread(hdr, 1, 20, f);
    fclose(f);
    if (n >= 17 && memcmp(hdr, "ALRIGROUP@APPMAKE", 17) == 0) return 20;
    if (n >= 13 && memcmp(hdr, "ALRIGROUP@APP", 13) == 0) return 16;
    return 0;
}

/* ======================== READER ======================== */
struct zip_reader {
    FILE *fp;
    int   entry_count;
    long  *offsets;
    char  **names;
    int   *methods;
    int   *crcs;
    int   *sizes_comp;
    int   *sizes_uncomp;
};

zip_reader_t *zip_reader_open(const char *path) {
    zip_reader_t *z = calloc(1, sizeof(zip_reader_t));
    if (!z) return NULL;
    z->fp = fopen(path, "rb");
    if (!z->fp) { free(z); return NULL; }

    /* seek to EOCD — read up to 64KB from end */
    fseek(z->fp, 0, SEEK_END);
    long filesize = ftell(z->fp);
    if (filesize < 22) { fclose(z->fp); free(z); return NULL; }

    unsigned char *buf = malloc(66000);
    if (!buf) { fclose(z->fp); free(z); return NULL; }
    int search = (filesize > 66000) ? 66000 : (int)filesize;
    fseek(z->fp, filesize - search, SEEK_SET);
    fread(buf, 1, search, z->fp);

    int eocd_pos = -1;
    for (int i = search - 22; i >= 0; i--) {
        if (buf[i] == 'P' && buf[i+1] == 'K' && buf[i+2] == 5 && buf[i+3] == 6) {
            eocd_pos = i;
            break;
        }
    }
    if (eocd_pos < 0) { free(buf); fclose(z->fp); free(z); return NULL; }

    unsigned int num_entries = read_le16(buf + eocd_pos + 8);
    unsigned int cd_size   = read_le32(buf + eocd_pos + 12);
    unsigned int cd_offset = read_le32(buf + eocd_pos + 16);
    free(buf);

    z->entry_count = (int)num_entries;
    if (z->entry_count <= 0 || z->entry_count > 65535) {
        fclose(z->fp); free(z); return NULL;
    }

    z->offsets = calloc(z->entry_count, sizeof(long));
    z->names = calloc(z->entry_count, sizeof(char *));
    z->methods = calloc(z->entry_count, sizeof(int));
    z->crcs = calloc(z->entry_count, sizeof(int));
    z->sizes_comp = calloc(z->entry_count, sizeof(int));
    z->sizes_uncomp = calloc(z->entry_count, sizeof(int));

    fseek(z->fp, (long)cd_offset, SEEK_SET);
    for (int i = 0; i < z->entry_count; i++) {
        unsigned char cd[46];
        if (fread(cd, 1, 46, z->fp) != 46) break;
        if (cd[0] != 'P' || cd[1] != 'K') break;

        int name_len = read_le16(cd + 28);
        int extra_len = read_le16(cd + 30);
        int comment_len = read_le16(cd + 32);

        z->methods[i] = read_le16(cd + 10);
        z->crcs[i] = (int)read_le32(cd + 16);
        z->sizes_comp[i] = (int)read_le32(cd + 20);
        z->sizes_uncomp[i] = (int)read_le32(cd + 24);
        z->offsets[i] = (long)read_le32(cd + 42);

        if (name_len > 255) name_len = 255;
        char name[256] = {0};
        fread(name, 1, name_len, z->fp);
        z->names[i] = strdup(name);

        fseek(z->fp, extra_len + comment_len, SEEK_CUR);
    }

    return z;
}

int zip_reader_count(zip_reader_t *z) {
    return z ? z->entry_count : 0;
}

int zip_reader_entry(zip_reader_t *z, int idx, zip_entry_t *out) {
    if (!z || idx < 0 || idx >= z->entry_count) return -1;
    strncpy(out->name, z->names[idx], sizeof(out->name) - 1);
    out->method = z->methods[idx];
    out->compressed_size = z->sizes_comp[idx];
    out->uncompressed_size = z->sizes_uncomp[idx];
    out->crc32 = z->crcs[idx];
    out->offset = z->offsets[idx];
    return 0;
}

static void mkdirp(const char *path) {
    char tmp[1024];
    strncpy(tmp, path, sizeof(tmp) - 1);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
#ifdef _WIN32
            _mkdir(tmp);
#else
            mkdir(tmp, 0755);
#endif
            *p = '/';
        }
    }
#ifdef _WIN32
    _mkdir(tmp);
#else
    mkdir(tmp, 0755);
#endif
}

int zip_reader_extract(zip_reader_t *z, int idx, const char *outdir) {
    if (!z || idx < 0 || idx >= z->entry_count) return -1;

    /* build output path */
    char outpath[1024];

        snprintf(outpath, sizeof(outpath), "%s/%s", outdir, z->names[idx]);
    /* ensure parent dir exists */
    char *slash = strrchr(outpath, '/');
    char *bslash = strrchr(outpath, '\\');
    char *last_sep = (slash > bslash) ? slash : bslash;
    if (last_sep) {
        char dir[1024];
        int len = (int)(last_sep - outpath);
        memcpy(dir, outpath, len);
        dir[len] = '\0';
        mkdirp(dir);
    }

    if (z->methods[idx] != ZIP_METHOD_STORED)
        return -2; /* only STORED supported */

    /* seek to file data: skip local file header */
    fseek(z->fp, z->offsets[idx], SEEK_SET);
    unsigned char lfh[30];
    fread(lfh, 1, 30, z->fp);
    int lfname_len = read_le16(lfh + 26);
    int lfextra_len = read_le16(lfh + 28);
    fseek(z->fp, lfname_len + lfextra_len, SEEK_CUR);

    /* write file data */
    FILE *out = fopen(outpath, "wb");
    if (!out) return -1;

    unsigned char data[4096];
    int remaining = z->sizes_comp[idx];
    while (remaining > 0) {
        int chunk = (remaining > 4096) ? 4096 : remaining;
        fread(data, 1, chunk, z->fp);
        fwrite(data, 1, chunk, out);
        remaining -= chunk;
    }
    fclose(out);

    /* set executable bit for .exe files */
    size_t nlen = strlen(outpath);
    if (nlen >= 4 && (strcmp(outpath + nlen - 4, ".exe") == 0 ||
                      strcmp(outpath + nlen - 4, ".bin") == 0)) {
        chmod(outpath, 0755);
    }
    /* runtime binaries live under bin/ — make them executable */
    if (strstr(outpath, "/bin/") != NULL) {
        chmod(outpath, 0755);
    }

    return 0;
}

void zip_reader_close(zip_reader_t *z) {
    if (!z) return;
    fclose(z->fp);
    for (int i = 0; i < z->entry_count; i++)
        free(z->names[i]);
    free(z->offsets);
    free(z->names);
    free(z->methods);
    free(z->crcs);
    free(z->sizes_comp);
    free(z->sizes_uncomp);
    free(z);
}
