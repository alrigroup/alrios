/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/package/arapp_v2.h"
#include "alrios/crypto_verify.h"
#include "zip.h"
#include "arapp_parser.h"
#include "aros_hal.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #include <io.h>
    #define SEPARATOR '\\'
    #define OTHER_SEP '/'
    #define mkdir_p_(p) _mkdir(p)
    #define remove_file_(p) _unlink(p)
    #define remove_dir_(p) _rmdir(p)
#else
    #include <dirent.h>
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/wait.h>
    #include <limits.h>
    #include <errno.h>
    #include <ctype.h>
    #define SEPARATOR '/'
    #define OTHER_SEP '\\'
    #define mkdir_p_(p) mkdir(p, 0755)
    #define remove_file_(p) unlink(p)
    #define remove_dir_(p) rmdir(p)
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

int armake_validate_and_canonicalize_manifest(const char *raw_json, char *out_canon, size_t max_out);

static int is_private_key_option(const char *arg) {
    if (!arg) return 0;
    if (strcmp(arg, "--key") == 0 ||
        strcmp(arg, "-k") == 0 ||
        strcmp(arg, "--private-key") == 0 ||
        strcmp(arg, "--sign") == 0 ||
        strcmp(arg, "--secret") == 0) {
        return 1;
    }
    return 0;
}

static void print_usage(void) {
    printf("ALRIOS App Packager (V2 Immutability Engine)\n");
    printf("Usage:\n");
    printf("  armake build [dir] [output] [--legacy] [--target <os>]\n");
    printf("  armake pack <dir> <output.arapp> [--legacy]\n");
    printf("  armake extract <file.arapp> <dir>\n");
    printf("  armake list <file.arapp>\n");
}

static void normalize_path(char *path) {
    for (; *path; path++)
        if (*path == OTHER_SEP) *path = SEPARATOR;
}

static int stream_file_to_zip(FILE *file, zip_writer_t *zip, size_t size) {
    unsigned char buffer[4096];
    size_t remaining = size;
    while (remaining > 0U) {
        size_t chunk = remaining > sizeof(buffer) ? sizeof(buffer) : remaining;
        size_t count = fread(buffer, 1U, chunk, file);
        if (count != chunk) return -1;
        if (zip_write(zip, buffer, (int)count) != 0) return -1;
        remaining -= count;
    }
    return 0;
}

static int pack_dir_to_memory_zip(const char *dir, uint8_t **out_buf, size_t *out_len) {
    char tmp_zip[PATH_MAX];
    snprintf(tmp_zip, sizeof(tmp_zip), "%s%c.armake_temp_%u.zip", dir, SEPARATOR, (unsigned int)rand());
    normalize_path(tmp_zip);

    zip_writer_t *z = zip_open_arapp(tmp_zip);
    if (!z) return -1;

    DIR *d = opendir(dir);
    if (!d) {
        zip_close(z);
        remove_file_(tmp_zip);
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 ||
            strcmp(entry->d_name, ".git") == 0 || strcmp(entry->d_name, ".arappmake") == 0) {
            continue;
        }
        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s%c%s", dir, SEPARATOR, entry->d_name);
        FILE *f = fopen(full, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (sz >= 0 && zip_add_entry(z, entry->d_name, ZIP_METHOD_STORED) == 0) {
                stream_file_to_zip(f, z, (size_t)sz);
            }
            fclose(f);
        }
    }
    closedir(d);
    zip_close(z);

    FILE *zf = fopen(tmp_zip, "rb");
    if (!zf) {
        remove_file_(tmp_zip);
        return -1;
    }
    fseek(zf, 0, SEEK_END);
    long zsz = ftell(zf);
    fseek(zf, 0, SEEK_SET);
    if (zsz < 0) {
        fclose(zf);
        remove_file_(tmp_zip);
        return -1;
    }

    uint8_t *buf = (uint8_t *)malloc((size_t)zsz);
    if (!buf) {
        fclose(zf);
        remove_file_(tmp_zip);
        return -1;
    }
    if (fread(buf, 1, (size_t)zsz, zf) != (size_t)zsz) {
        free(buf);
        fclose(zf);
        remove_file_(tmp_zip);
        return -1;
    }
    fclose(zf);
    remove_file_(tmp_zip);

    *out_buf = buf;
    *out_len = (size_t)zsz;
    return 0;
}

int armake_validate_and_canonicalize_manifest(const char *raw_json, char *out_canon, size_t max_out);
int armake_build_v2_envelope_buffer(const char *canon_manifest, const uint8_t *payload, size_t payload_len, uint8_t *out_buf, size_t out_cap, size_t *out_written);
int armake_check_no_private_keys(const char *manifest_json);

static int emit_arapp_v2_envelope(const char *manifest_json, const uint8_t *payload, size_t payload_len, const char *output_path) {
    char canon_manifest[8192];
    if (armake_validate_and_canonicalize_manifest(manifest_json, canon_manifest, sizeof(canon_manifest)) != 0) {
        fprintf(stderr, "[ERRO] Manifesto invalido ou nao-canônico rejeitado.\n");
        return -1;
    }

    size_t max_out = ARAPP_V2_HEADER_PREFIX_SIZE + strlen(canon_manifest) + payload_len;
    uint8_t *out_buf = (uint8_t *)malloc(max_out);
    if (!out_buf) return -1;

    size_t written = 0U;
    int rc = armake_build_v2_envelope_buffer(canon_manifest, payload, payload_len, out_buf, max_out, &written);
    if (rc != 0) {
        free(out_buf);
        return -1;
    }

    FILE *f = fopen(output_path, "wb");
    if (!f) {
        free(out_buf);
        return -1;
    }
    if (fwrite(out_buf, 1, written, f) != written) {
        fclose(f);
        free(out_buf);
        return -1;
    }
    fclose(f);
    free(out_buf);
    return 0;
}

static int cmd_build_or_pack(const char *dir, const char *output, int legacy) {
    char manifest_path[PATH_MAX];
    snprintf(manifest_path, sizeof(manifest_path), "%s%cmanifest.json", dir, SEPARATOR);
    FILE *mf = fopen(manifest_path, "rb");
    if (!mf) {
        snprintf(manifest_path, sizeof(manifest_path), "%s%capp.arappmake", dir, SEPARATOR);
        mf = fopen(manifest_path, "rb");
    }
    if (!mf) {
        snprintf(manifest_path, sizeof(manifest_path), "%s", dir);
        mf = fopen(manifest_path, "rb");
    }
    if (!mf) {
        fprintf(stderr, "[ERRO] Manifesto nao encontrado em: %s\n", dir);
        return 1;
    }
    fseek(mf, 0, SEEK_END);
    long msz = ftell(mf);
    fseek(mf, 0, SEEK_SET);
    if (msz < 0) {
        fclose(mf);
        return 1;
    }
    char *mjson = (char *)malloc((size_t)msz + 1U);
    if (!mjson) {
        fclose(mf);
        return 1;
    }
    size_t read_bytes = fread(mjson, 1, (size_t)msz, mf);
    fclose(mf);
    if (read_bytes != (size_t)msz) {
        free(mjson);
        return 1;
    }
    mjson[msz] = '\0';

    uint8_t *payload = NULL;
    size_t payload_len = 0U;
    if (armake_check_no_private_keys(mjson) != 0) {
        free(mjson);
        fprintf(stderr, "[ERRO] Rejeitado: material de chave privada detectado.\n");
        return 1;
    }
    if (pack_dir_to_memory_zip(dir, &payload, &payload_len) != 0) {
        free(mjson);
        fprintf(stderr, "[ERRO] Falha ao empacotar diretorio de aplicativos.\n");
        return 1;
    }

    if (!legacy) {
        if (emit_arapp_v2_envelope(mjson, payload, payload_len, output) == 0) {
            free(mjson);
            free(payload);
            printf("[OK] Immutavel v2 .arapp envelope criado: %s\n", output);
            return 0;
        }
    }

    zip_writer_t *z = zip_open_arapp(output);
    if (!z) {
        free(mjson);
        free(payload);
        return 1;
    }
    zip_add_entry(z, "manifest.json", ZIP_METHOD_STORED);
    zip_write(z, mjson, (int)strlen(mjson));
    zip_add_entry(z, "payload.zip", ZIP_METHOD_STORED);
    zip_write(z, payload, (int)payload_len);
    zip_close(z);
    free(mjson);
    free(payload);
    printf("[OK] Legacy .arapp criado: %s\n", output);
    return 0;
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (is_private_key_option(argv[i])) {
            fprintf(stderr, "[SECURITY ERROR] armake is an immutable packager and does not accept private keys (%s rejected)\n", argv[i]);
            return 1;
        }
    }

    if (argc < 2) {
        print_usage();
        return 1;
    }

    const char *cmd = argv[1];
    if (strcmp(cmd, "build") == 0 || strcmp(cmd, "pack") == 0) {
        const char *dir = (argc > 2) ? argv[2] : ".";
        const char *output = (argc > 3) ? argv[3] : "output.arapp";
        int legacy = 0;
        for (int i = 4; i < argc; ++i) {
            if (strcmp(argv[i], "--legacy") == 0) {
                legacy = 1;
            }
        }
        return cmd_build_or_pack(dir, output, legacy);
    } else if (strcmp(cmd, "extract") == 0) {
        if (argc < 4) { print_usage(); return 1; }
        zip_reader_t *z = zip_reader_open(argv[2]);
        if (!z) return 1;
        int count = zip_reader_count(z);
        for (int i = 0; i < count; i++) {
            zip_reader_extract(z, i, argv[3]);
        }
        zip_reader_close(z);
        printf("[OK] Extraido para: %s\n", argv[3]);
        return 0;
    } else if (strcmp(cmd, "list") == 0) {
        if (argc < 3) { print_usage(); return 1; }
        zip_reader_t *z = zip_reader_open(argv[2]);
        if (!z) return 1;
        int count = zip_reader_count(z);
        for (int i = 0; i < count; i++) {
            zip_entry_t e;
            zip_reader_entry(z, i, &e);
            printf("  %s\n", e.name);
        }
        zip_reader_close(z);
        return 0;
    }

    print_usage();
    return 1;
}
