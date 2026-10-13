/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/armake/package_v2.h"
#include "alrios/package/arapp_v2.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <io.h>
#define unlink_file _unlink
#else
#include <unistd.h>
#define unlink_file unlink
#endif

int alrios_manifest_canonicalize(const char *in_json, char *out_canon,
                                 size_t max_out);

#define ALRIOS_ARMAKE_V2_IO_BUFFER_SIZE 65536U

static const uint8_t arapp_v2_magic[ALRIOS_ARAPP_V2_MAGIC_SIZE] = {
    'A', 'L', 'R', 'I', 'G', 'R', 'O', 'U',
    'P', '@', 'A', 'R', 'A', 'P', 'P', 0U
};

static int valid_architecture(uint16_t architecture) {
    return architecture == ALRIOS_ARAPP_V2_ARCH_X86_64 ||
           architecture == ALRIOS_ARAPP_V2_ARCH_AARCH64 ||
           architecture == ALRIOS_ARAPP_V2_ARCH_RISCV64;
}

static int valid_unsigned_flags(uint32_t flags) {
    uint32_t profiles = flags & (ALRIOS_ARAPP_V2_FLAG_PROFILE_SOVEREIGN |
                                  ALRIOS_ARAPP_V2_FLAG_PROFILE_ENTERPRISE |
                                  ALRIOS_ARAPP_V2_FLAG_PROFILE_PERFORMANCE);
    uint32_t rings = flags & (ALRIOS_ARAPP_V2_FLAG_RING_SOVEREIGN |
                              ALRIOS_ARAPP_V2_FLAG_RING_DEVMODE);

    if ((flags & ~ALRIOS_ARAPP_V2_FLAGS_MASK) != 0U ||
        (flags & (ALRIOS_ARAPP_V2_FLAG_ENCRYPTED_GCM |
                  ALRIOS_ARAPP_V2_FLAG_PQC_HYBRID_SIG)) != 0U) {
        return 0;
    }
    if (profiles == 0U || (profiles & (profiles - 1U)) != 0U) {
        return 0;
    }
    if (rings != ALRIOS_ARAPP_V2_FLAG_RING_DEVMODE) {
        return 0;
    }
    if ((flags & ALRIOS_ARAPP_V2_FLAG_PROFILE_SOVEREIGN) != 0U) {
        return 0;
    }
    return 1;
}

static int has_forbidden_mutable_suffix(const char *path) {
    static const char *const suffixes[] = {
        ".db", ".sqlite", ".session", ".lock",
        ".wal", ".shm", ".log", ".tmp"
    };
    const char *name;
    size_t name_len;
    size_t index;

    if (!path || path[0] == '\0') {
        return 0;
    }
    name = strrchr(path, '/');
    if (!name) {
        name = strrchr(path, '\\');
    }
    name = name ? name + 1 : path;
    name_len = strlen(name);

    for (index = 0U; index < sizeof(suffixes) / sizeof(suffixes[0]); ++index) {
        size_t suffix_len = strlen(suffixes[index]);
        size_t offset;
        size_t byte_index;

        if (name_len < suffix_len) {
            continue;
        }
        offset = name_len - suffix_len;
        for (byte_index = 0U; byte_index < suffix_len; ++byte_index) {
            unsigned char actual = (unsigned char)name[offset + byte_index];
            unsigned char expected = (unsigned char)suffixes[index][byte_index];
            if (actual >= (unsigned char)'A' && actual <= (unsigned char)'Z') {
                actual = (unsigned char)(actual - (unsigned char)'A' +
                                         (unsigned char)'a');
            }
            if (actual != expected) {
                break;
            }
        }
        if (byte_index == suffix_len) {
            return 1;
        }
    }
    return 0;
}

static int get_file_size(FILE *file, uint64_t *out_size) {
    long size;

    if (!file || !out_size || fseek(file, 0L, SEEK_END) != 0) {
        return ALRIOS_ARMAKE_V2_ERR_IO;
    }
    size = ftell(file);
    if (size < 0L || fseek(file, 0L, SEEK_SET) != 0) {
        return ALRIOS_ARMAKE_V2_ERR_IO;
    }
    *out_size = (uint64_t)(unsigned long)size;
    return ALRIOS_ARMAKE_V2_OK;
}

static int read_canonical_manifest(const char *path, uint8_t **out_manifest,
                                   uint32_t *out_length) {
    FILE *file;
    uint64_t input_size_u64;
    size_t input_size;
    char *input;
    char *canonical;
    size_t canonical_length;
    int status;

    if (!path || !out_manifest || !out_length) {
        return ALRIOS_ARMAKE_V2_ERR_ARGUMENT;
    }
    *out_manifest = NULL;
    *out_length = 0U;
    file = fopen(path, "rb");
    if (!file) {
        return ALRIOS_ARMAKE_V2_ERR_IO;
    }
    status = get_file_size(file, &input_size_u64);
    if (status != ALRIOS_ARMAKE_V2_OK || input_size_u64 == 0U ||
        input_size_u64 > ALRIOS_ARAPP_V2_MAX_MANIFEST_SIZE) {
        (void)fclose(file);
        return ALRIOS_ARMAKE_V2_ERR_MANIFEST;
    }
    input_size = (size_t)input_size_u64;
    input = malloc(input_size + 1U);
    canonical = malloc(ALRIOS_ARAPP_V2_MAX_MANIFEST_SIZE + 1U);
    if (!input || !canonical) {
        free(canonical);
        free(input);
        (void)fclose(file);
        return ALRIOS_ARMAKE_V2_ERR_CAPACITY;
    }
    if (fread(input, 1U, input_size, file) != input_size ||
        fclose(file) != 0) {
        free(canonical);
        free(input);
        return ALRIOS_ARMAKE_V2_ERR_IO;
    }
    input[input_size] = '\0';
    if (memchr(input, '\0', input_size) != NULL ||
        alrios_manifest_canonicalize(
            input, canonical, ALRIOS_ARAPP_V2_MAX_MANIFEST_SIZE + 1U) != 0) {
        free(canonical);
        free(input);
        return ALRIOS_ARMAKE_V2_ERR_MANIFEST;
    }
    free(input);
    canonical_length = strlen(canonical);
    if (canonical_length == 0U ||
        canonical_length > ALRIOS_ARAPP_V2_MAX_MANIFEST_SIZE ||
        canonical_length > UINT32_MAX) {
        free(canonical);
        return ALRIOS_ARMAKE_V2_ERR_MANIFEST;
    }
    *out_manifest = (uint8_t *)canonical;
    *out_length = (uint32_t)canonical_length;
    return ALRIOS_ARMAKE_V2_OK;
}

static void write_u16_be(uint8_t *output, uint16_t value) {
    output[0] = (uint8_t)(value >> 8U);
    output[1] = (uint8_t)value;
}

static void write_u32_be(uint8_t *output, uint32_t value) {
    output[0] = (uint8_t)(value >> 24U);
    output[1] = (uint8_t)(value >> 16U);
    output[2] = (uint8_t)(value >> 8U);
    output[3] = (uint8_t)value;
}

static void write_u64_be(uint8_t *output, uint64_t value) {
    size_t index;

    for (index = 8U; index > 0U; --index) {
        output[index - 1U] = (uint8_t)value;
        value >>= 8U;
    }
}

static int write_all(FILE *output, const uint8_t *data, size_t length) {
    if (length == 0U) {
        return ALRIOS_ARMAKE_V2_OK;
    }
    if (!output || !data || fwrite(data, 1U, length, output) != length) {
        return ALRIOS_ARMAKE_V2_ERR_IO;
    }
    return ALRIOS_ARMAKE_V2_OK;
}

static int copy_payload(FILE *input, FILE *output, uint64_t length) {
    uint8_t buffer[ALRIOS_ARMAKE_V2_IO_BUFFER_SIZE];
    uint64_t remaining = length;

    while (remaining > 0U) {
        size_t chunk = remaining > sizeof(buffer) ?
                       sizeof(buffer) : (size_t)remaining;
        if (fread(buffer, 1U, chunk, input) != chunk ||
            write_all(output, buffer, chunk) != ALRIOS_ARMAKE_V2_OK) {
            return ALRIOS_ARMAKE_V2_ERR_IO;
        }
        remaining -= (uint64_t)chunk;
    }
    if (fgetc(input) != EOF || ferror(input)) {
        return ALRIOS_ARMAKE_V2_ERR_IO;
    }
    return ALRIOS_ARMAKE_V2_OK;
}

int alrios_armake_package_v2(
    const char *manifest_path,
    const char *payload_path,
    const char *output_path,
    const alrios_armake_v2_options_t *options
) {
    uint8_t *manifest = NULL;
    uint32_t manifest_length = 0U;
    uint8_t header[ALRIOS_ARAPP_V2_HEADER_SIZE];
    FILE *payload = NULL;
    FILE *output = NULL;
    uint64_t payload_length = 0U;
    int status;

    if (!manifest_path || !payload_path || !output_path || !options ||
        manifest_path[0] == '\0' || payload_path[0] == '\0' ||
        output_path[0] == '\0') {
        return ALRIOS_ARMAKE_V2_ERR_ARGUMENT;
    }
    if (!valid_architecture(options->target_arch) ||
        !valid_unsigned_flags(options->container_flags) ||
        options->issued_at == 0U || options->expires_at <= options->issued_at) {
        return ALRIOS_ARMAKE_V2_ERR_POLICY;
    }
    if (has_forbidden_mutable_suffix(payload_path)) {
        return ALRIOS_ARMAKE_V2_ERR_MUTABLE_FILE;
    }
    if (strcmp(manifest_path, output_path) == 0 ||
        strcmp(payload_path, output_path) == 0) {
        return ALRIOS_ARMAKE_V2_ERR_ARGUMENT;
    }

    status = read_canonical_manifest(manifest_path, &manifest, &manifest_length);
    if (status != ALRIOS_ARMAKE_V2_OK) {
        return status;
    }
    payload = fopen(payload_path, "rb");
    if (!payload) {
        free(manifest);
        return ALRIOS_ARMAKE_V2_ERR_IO;
    }
    status = get_file_size(payload, &payload_length);
    if (status != ALRIOS_ARMAKE_V2_OK) {
        (void)fclose(payload);
        free(manifest);
        return status;
    }

    errno = 0;
    output = fopen(output_path, "wbx");
    if (!output) {
        int open_error = errno;

        (void)fclose(payload);
        free(manifest);
        return open_error == EEXIST ? ALRIOS_ARMAKE_V2_ERR_OUTPUT_EXISTS :
                                     ALRIOS_ARMAKE_V2_ERR_IO;
    }

    memset(header, 0, sizeof(header));
    memcpy(header, arapp_v2_magic, sizeof(arapp_v2_magic));
    if (payload_length > UINT32_MAX) {
        (void)fclose(output);
        (void)fclose(payload);
        free(manifest);
        (void)unlink_file(output_path);
        return ALRIOS_ARMAKE_V2_ERR_CAPACITY;
    }
    write_u16_be(header + 16U, ALRIOS_ARAPP_V2_VERSION);
    write_u16_be(header + 18U, options->target_arch);
    write_u32_be(header + 20U, options->container_flags);
    write_u64_be(header + 24U, options->issued_at);
    write_u64_be(header + 32U, options->expires_at);
    write_u32_be(header + 40U, ALRIOS_ARAPP_V2_HEADER_SIZE);
    write_u32_be(header + 44U, manifest_length);
    write_u32_be(header + 48U, (uint32_t)payload_length);

    status = write_all(output, header, sizeof(header));
    if (status == ALRIOS_ARMAKE_V2_OK) {
        status = write_all(output, manifest, manifest_length);
    }
    if (status == ALRIOS_ARMAKE_V2_OK) {
        status = copy_payload(payload, output, payload_length);
    }
    if (fclose(payload) != 0 && status == ALRIOS_ARMAKE_V2_OK) {
        status = ALRIOS_ARMAKE_V2_ERR_IO;
    }
    if (fflush(output) != 0 && status == ALRIOS_ARMAKE_V2_OK) {
        status = ALRIOS_ARMAKE_V2_ERR_IO;
    }
    if (fclose(output) != 0 && status == ALRIOS_ARMAKE_V2_OK) {
        status = ALRIOS_ARMAKE_V2_ERR_IO;
    }
    free(manifest);
    if (status != ALRIOS_ARMAKE_V2_OK) {
        (void)unlink_file(output_path);
    }
    return status;
}

const char *alrios_armake_v2_error_string(int status) {
    switch (status) {
    case ALRIOS_ARMAKE_V2_OK:
        return "success";
    case ALRIOS_ARMAKE_V2_ERR_ARGUMENT:
        return "invalid argument";
    case ALRIOS_ARMAKE_V2_ERR_IO:
        return "I/O failure";
    case ALRIOS_ARMAKE_V2_ERR_MANIFEST:
        return "invalid manifest";
    case ALRIOS_ARMAKE_V2_ERR_MUTABLE_FILE:
        return "mutable payload type is forbidden";
    case ALRIOS_ARMAKE_V2_ERR_POLICY:
        return "unsigned package policy violation";
    case ALRIOS_ARMAKE_V2_ERR_CAPACITY:
        return "capacity exceeded";
    case ALRIOS_ARMAKE_V2_ERR_OUTPUT_EXISTS:
        return "output already exists";
    default:
        return "unknown package error";
    }
}
