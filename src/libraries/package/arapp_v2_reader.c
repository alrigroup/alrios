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

#include <stdlib.h>
#include <string.h>

int alrios_manifest_canonicalize(const char *in_json, char *out_canon, size_t max_out);

static uint16_t load_u16_be(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8U) | (uint16_t)p[1]);
}

static uint32_t load_u32_be(const uint8_t *p) {
    return ((uint32_t)p[0] << 24U) | ((uint32_t)p[1] << 16U) |
           ((uint32_t)p[2] << 8U) | (uint32_t)p[3];
}

static uint64_t load_u64_be(const uint8_t *p) {
    uint64_t value = 0U;
    size_t i;
    for (i = 0U; i < 8U; ++i) {
        value = (value << 8U) | (uint64_t)p[i];
    }
    return value;
}

static void store_u16_be(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8U);
    p[1] = (uint8_t)v;
}

static void store_u32_be(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24U);
    p[1] = (uint8_t)(v >> 16U);
    p[2] = (uint8_t)(v >> 8U);
    p[3] = (uint8_t)v;
}

static void store_u64_be(uint8_t *p, uint64_t v) {
    size_t i;
    for (i = 8U; i > 0U; --i) {
        p[i - 1U] = (uint8_t)v;
        v >>= 8U;
    }
}

static int buffers_overlap(const void *a, size_t a_len, const void *b, size_t b_len) {
    uintptr_t a_start;
    uintptr_t a_end;
    uintptr_t b_start;
    uintptr_t b_end;

    if (!a || !b || a_len == 0U || b_len == 0U) {
        return 0;
    }
    a_start = (uintptr_t)a;
    b_start = (uintptr_t)b;
    if (a_len > UINTPTR_MAX - a_start || b_len > UINTPTR_MAX - b_start) {
        return 1;
    }
    a_end = a_start + a_len;
    b_end = b_start + b_len;
    return (a_start < b_end) && (b_start < a_end);
}

int arapp_v2_read_header(const uint8_t *buffer, size_t size, arapp_v2_package_t *out_pkg) {
    static const uint8_t expected_magic[ARAPP_V2_MAGIC_LEN] = {
        'A', 'L', 'R', 'I', 'G', 'R', 'O', 'U', 'P', '@', 'A', 'R', 'A', 'P', 'P', '\0'
    };

    if (!buffer || !out_pkg) {
        return ARAPP_V2_ERR_INVALID_ARGUMENT;
    }

    if (size < ARAPP_V2_HEADER_PREFIX_SIZE) {
        return ARAPP_V2_ERR_TRUNCATED;
    }

    if (alrios_constant_time_memcmp(buffer, expected_magic, ARAPP_V2_MAGIC_LEN) != 0) {
        return ARAPP_V2_ERR_BAD_MAGIC;
    }

    uint16_t version = load_u16_be(buffer + 16U);
    if (version != ARAPP_V2_FORMAT_VERSION) {
        return ARAPP_V2_ERR_VERSION;
    }

    uint16_t arch = load_u16_be(buffer + 18U);
    if (arch != ARAPP_V2_ARCH_X86_64 &&
        arch != ARAPP_V2_ARCH_AARCH64 &&
        arch != ARAPP_V2_ARCH_RISCV64) {
        return ARAPP_V2_ERR_ARCH;
    }

    uint32_t flags = load_u32_be(buffer + 20U);
    uint64_t issued = load_u64_be(buffer + 24U);
    uint64_t expiry = load_u64_be(buffer + 32U);
    uint32_t header_size = load_u32_be(buffer + 40U);
    uint32_t entitlements_len = load_u32_be(buffer + 44U);
    uint32_t ciphertext_len = load_u32_be(buffer + 48U);

    if (header_size != ARAPP_V2_HEADER_PREFIX_SIZE &&
        header_size != ARAPP_V2_FILE_HEADER_SIZE) {
        return ARAPP_V2_ERR_HEADER_SIZE;
    }

    if ((size_t)header_size > size) {
        return ARAPP_V2_ERR_HEADER_SIZE;
    }

    int expects_signatures = (flags & (ARAPP_V2_FLAG_PROFILE_SOVEREIGN | ARAPP_V2_FLAG_PQC_HYBRID_SIG)) != 0;

    if (expects_signatures && header_size != ARAPP_V2_FILE_HEADER_SIZE) {
        return ARAPP_V2_ERR_HEADER_SIZE;
    }

    memset(out_pkg, 0, sizeof(*out_pkg));
    memcpy(out_pkg->prefix.magic, buffer, ARAPP_V2_MAGIC_LEN);
    out_pkg->prefix.format_version = version;
    out_pkg->prefix.target_arch = arch;
    out_pkg->prefix.container_flags = flags;
    out_pkg->prefix.timestamp_issued = issued;
    out_pkg->prefix.timestamp_expiry = expiry;
    out_pkg->prefix.header_size_bytes = header_size;
    out_pkg->prefix.entitlements_json_length = entitlements_len;
    out_pkg->prefix.ciphertext_size_bytes = ciphertext_len;
    memcpy(out_pkg->prefix.aes_gcm_iv, buffer + 52U, ARAPP_V2_GCM_IV_LEN);
    memcpy(out_pkg->prefix.aes_gcm_tag, buffer + 64U, ARAPP_V2_GCM_TAG_LEN);
    memcpy(out_pkg->prefix.cleartext_sha512, buffer + 80U, ARAPP_V2_SHA512_LEN);

    out_pkg->format_version = version;
    out_pkg->target_arch = arch;
    out_pkg->container_flags = flags;
    out_pkg->timestamp_issued = issued;
    out_pkg->timestamp_expiry = expiry;
    out_pkg->header_size_bytes = header_size;
    out_pkg->entitlements_json_length = entitlements_len;
    out_pkg->ciphertext_size_bytes = ciphertext_len;
    out_pkg->header_offset = 0U;

    if (header_size >= ARAPP_V2_FILE_HEADER_SIZE && size >= ARAPP_V2_FILE_HEADER_SIZE) {
        out_pkg->signatures = (const arapp_v2_signatures_t *)(buffer + ARAPP_V2_HEADER_PREFIX_SIZE);
        out_pkg->has_signatures = 1U;
    }

    return ARAPP_V2_OK;
}

int arapp_v2_validate_offsets(const uint8_t *buffer, size_t size, arapp_v2_package_t *out_pkg) {
    if (!buffer || !out_pkg) {
        return ARAPP_V2_ERR_INVALID_ARGUMENT;
    }

    size_t header_size = (size_t)out_pkg->header_size_bytes;
    size_t manifest_len = (size_t)out_pkg->entitlements_json_length;
    size_t ciphertext_len = (size_t)out_pkg->ciphertext_size_bytes;

    if (header_size != ARAPP_V2_HEADER_PREFIX_SIZE &&
        header_size != ARAPP_V2_FILE_HEADER_SIZE) {
        return ARAPP_V2_ERR_HEADER_SIZE;
    }

    if (header_size > size) {
        return ARAPP_V2_ERR_TRUNCATED;
    }

    size_t manifest_offset = header_size;
    if (manifest_len > SIZE_MAX - manifest_offset) {
        return ARAPP_V2_ERR_OFFSET_OVERFLOW;
    }
    size_t manifest_end = manifest_offset + manifest_len;
    if (manifest_end > size) {
        return ARAPP_V2_ERR_TRUNCATED;
    }

    size_t ciphertext_offset = manifest_end;
    if (ciphertext_len > SIZE_MAX - ciphertext_offset) {
        return ARAPP_V2_ERR_OFFSET_OVERFLOW;
    }
    size_t expected_total = ciphertext_offset + ciphertext_len;

    if (size < expected_total) {
        return ARAPP_V2_ERR_TRUNCATED;
    }
    if (size > expected_total) {
        return ARAPP_V2_ERR_TRAILING_BYTES;
    }

    out_pkg->manifest_offset = manifest_offset;
    out_pkg->manifest_len = manifest_len;
    out_pkg->manifest_json = (manifest_len > 0U) ? (const char *)(buffer + manifest_offset) : NULL;

    out_pkg->ciphertext_offset = ciphertext_offset;
    out_pkg->ciphertext_len = ciphertext_len;
    out_pkg->ciphertext = (ciphertext_len > 0U) ? (buffer + ciphertext_offset) : NULL;

    out_pkg->total_size = size;

    return ARAPP_V2_OK;
}

int arapp_v2_validate_anti_replay(const arapp_v2_package_t *pkg, uint64_t current_time, uint64_t max_skew_sec) {
    if (!pkg) {
        return ARAPP_V2_ERR_INVALID_ARGUMENT;
    }

    if (pkg->timestamp_issued == 0U || pkg->timestamp_expiry == 0U) {
        return ARAPP_V2_ERR_TIMESTAMP_ORDER;
    }

    if (pkg->timestamp_issued > pkg->timestamp_expiry) {
        return ARAPP_V2_ERR_TIMESTAMP_ORDER;
    }

    if (current_time > 0U) {
        if (pkg->timestamp_issued > current_time &&
            (pkg->timestamp_issued - current_time) > max_skew_sec) {
            return ARAPP_V2_ERR_TIMESTAMP_FUTURE;
        }

        if (current_time > pkg->timestamp_expiry &&
            (current_time - pkg->timestamp_expiry) > max_skew_sec) {
            return ARAPP_V2_ERR_TIMESTAMP_EXPIRED;
        }
    }

    return ARAPP_V2_OK;
}

static int extract_top_level_string_field_n(const char *json, size_t json_len, const char *key, char *out_val, size_t out_max) {
    if (!json || json_len == 0U || !key || !out_val || out_max == 0U) {
        return -1;
    }

    const char *p = json;
    const char *end = json + json_len;

    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
        p++;
    }
    if (p >= end || *p != '{') {
        return -1;
    }
    p++;

    size_t key_len = strlen(key);

    for (;;) {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
            p++;
        }
        if (p >= end || *p == '}') {
            break;
        }
        if (*p != '\"') {
            return -1;
        }
        p++;

        const char *k_start = p;
        while (p < end && *p != '\"') {
            if (*p == '\\') {
                p++;
                if (p >= end) return -1;
            }
            p++;
        }
        if (p >= end || *p != '\"') {
            return -1;
        }
        size_t k_len = (size_t)(p - k_start);
        p++; /* skip closing quote */

        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
            p++;
        }
        if (p >= end || *p != ':') {
            return -1;
        }
        p++; /* skip ':' */

        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
            p++;
        }
        if (p >= end) {
            return -1;
        }

        int is_match = (k_len == key_len && memcmp(k_start, key, key_len) == 0);

        if (*p == '\"') {
            p++;
            size_t val_len = 0U;
            while (p < end && *p != '\"') {
                if (*p == '\\') {
                    p++;
                    if (p >= end) return -1;
                }
                if (is_match) {
                    if (val_len + 1U >= out_max) {
                        return -1;
                    }
                    out_val[val_len++] = *p;
                }
                p++;
            }
            if (p >= end || *p != '\"') {
                return -1;
            }
            p++; /* skip closing quote */

            if (is_match) {
                out_val[val_len] = '\0';
                return 0;
            }
        } else if (*p == '{' || *p == '[') {
            /* Skip nested object/array */
            char open_ch = *p;
            char close_ch = (open_ch == '{') ? '}' : ']';
            int nest = 1;
            p++;
            while (p < end && nest > 0) {
                if (*p == '\"') {
                    p++;
                    while (p < end && *p != '\"') {
                        if (*p == '\\') {
                            p++;
                            if (p >= end) return -1;
                        }
                        p++;
                    }
                    if (p < end && *p == '\"') p++;
                } else {
                    if (*p == open_ch) {
                        nest++;
                    } else if (*p == close_ch) {
                        nest--;
                    }
                    p++;
                }
            }
            if (nest != 0) {
                return -1;
            }
        } else {
            /* primitive value (number, bool, null) */
            while (p < end && *p != ',' && *p != '}' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
                p++;
            }
        }

        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
            p++;
        }
        if (p < end && *p == ',') {
            p++;
        } else if (p < end && *p == '}') {
            /* loop will terminate */
        } else {
            return -1;
        }
    }

    return -1;
}

static int extract_top_level_string_field(const char *json, const char *key, char *out_val, size_t out_max) {
    if (!json) {
        return -1;
    }
    return extract_top_level_string_field_n(json, strlen(json), key, out_val, out_max);
}

static int extract_storage_mount_field(const char *json, char *out_val, size_t out_max) {
    if (!json || !out_val || out_max == 0U) {
        return -1;
    }

    const char *p = strstr(json, "\"storage\":{");
    if (!p) {
        return -1;
    }
    p += strlen("\"storage\":{");

    for (;;) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
            p++;
        }
        if (*p == '}') {
            break;
        }
        if (*p != '\"') {
            return -1;
        }
        p++;

        const char *k_start = p;
        while (*p && *p != '\"') {
            if (*p == '\\') {
                p++;
                if (!*p) return -1;
            }
            p++;
        }
        if (*p != '\"') {
            return -1;
        }
        size_t k_len = (size_t)(p - k_start);
        p++; /* skip closing quote */

        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
            p++;
        }
        if (*p != ':') {
            return -1;
        }
        p++; /* skip ':' */

        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
            p++;
        }

        int is_match = (k_len == strlen("persistent_mount") &&
                        memcmp(k_start, "persistent_mount", k_len) == 0);

        if (*p == '\"') {
            p++;
            size_t val_len = 0U;
            while (*p && *p != '\"') {
                if (*p == '\\') {
                    p++;
                    if (!*p) return -1;
                }
                if (is_match) {
                    if (val_len + 1U >= out_max) {
                        return -1;
                    }
                    out_val[val_len++] = *p;
                }
                p++;
            }
            if (*p != '\"') {
                return -1;
            }
            p++;

            if (is_match) {
                out_val[val_len] = '\0';
                return 0;
            }
        } else if (*p == '{' || *p == '[') {
            char open_ch = *p;
            char close_ch = (open_ch == '{') ? '}' : ']';
            int nest = 1;
            p++;
            while (*p && nest > 0) {
                if (*p == '\"') {
                    p++;
                    while (*p && *p != '\"') {
                        if (*p == '\\') {
                            p++;
                            if (!*p) return -1;
                        }
                        p++;
                    }
                    if (*p == '\"') p++;
                } else {
                    if (*p == open_ch) nest++;
                    else if (*p == close_ch) nest--;
                    p++;
                }
            }
            if (nest != 0) return -1;
        } else {
            while (*p && *p != ',' && *p != '}' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
                p++;
            }
        }

        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
            p++;
        }
        if (*p == ',') {
            p++;
        } else if (*p == '}') {
            break;
        } else {
            return -1;
        }
    }

    return -1;
}

static int check_path_traversal(const char *val) {
    if (!val || val[0] == '\0') {
        return 0;
    }
    /* Reject absolute paths: Unix root '/' or Windows drive 'C:\' / 'C:/' or UNC '\\' */
    if (val[0] == '/' || val[0] == '\\') {
        return 1;
    }
    if (((val[0] >= 'a' && val[0] <= 'z') || (val[0] >= 'A' && val[0] <= 'Z')) && val[1] == ':') {
        return 1;
    }
    /* Reject any "../" or "..\" sequence */
    if (strstr(val, "../") || strstr(val, "..\\")) {
        return 1;
    }
    /* Reject ending with ".." or equal to ".." */
    size_t len = strlen(val);
    if (len >= 2U) {
        if (val[len - 2U] == '.' && val[len - 1U] == '.') {
            if (len == 2U || val[len - 3U] == '/' || val[len - 3U] == '\\') {
                return 1;
            }
        }
    }
    return 0;
}

static int check_safe_app_id(const char *app_id) {
    if (!app_id || app_id[0] == '\0') {
        return 0;
    }
    if (check_path_traversal(app_id)) {
        return 0;
    }
    /* app_id must not contain path separators, control chars, spaces */
    for (size_t i = 0; app_id[i] != '\0'; ++i) {
        unsigned char c = (unsigned char)app_id[i];
        if (c == '/' || c == '\\' || c == ':' || c <= 32 || c >= 127) {
            return 0;
        }
    }
    return 1;
}

int arapp_v2_validate_manifest(const arapp_v2_package_t *pkg) {
    static const char *const required_props[] = {
        "\"app_id\":",
        "\"version\":",
        "\"execution_profile\":",
        "\"execution_ring\":",
        "\"network\":",
        "\"resources\":",
        "\"ipc\":",
        "\"storage\":",
        "\"vault\":"
    };
    size_t i;

    if (!pkg) {
        return ARAPP_V2_ERR_INVALID_ARGUMENT;
    }

    if (pkg->manifest_len == 0U || !pkg->manifest_json) {
        return ARAPP_V2_ERR_NONCANONICAL_MANIFEST;
    }

    char *copy = (char *)malloc(pkg->manifest_len + 1U);
    if (!copy) {
        return ARAPP_V2_ERR_MEMORY;
    }
    memcpy(copy, pkg->manifest_json, pkg->manifest_len);
    copy[pkg->manifest_len] = '\0';

    char *canon = (char *)malloc(pkg->manifest_len + 1U);
    if (!canon) {
        free(copy);
        return ARAPP_V2_ERR_MEMORY;
    }

    int rc = alrios_manifest_canonicalize(copy, canon, pkg->manifest_len + 1U);
    if (rc != 0) {
        free(canon);
        free(copy);
        return ARAPP_V2_ERR_NONCANONICAL_MANIFEST;
    }

    size_t canon_len = strlen(canon);
    if (canon_len != pkg->manifest_len ||
        memcmp(canon, pkg->manifest_json, pkg->manifest_len) != 0) {
        free(canon);
        free(copy);
        return ARAPP_V2_ERR_NONCANONICAL_MANIFEST;
    }

    for (i = 0U; i < sizeof(required_props) / sizeof(required_props[0]); ++i) {
        if (!strstr(canon, required_props[i])) {
            free(canon);
            free(copy);
            return ARAPP_V2_ERR_MANIFEST_SCHEMA;
        }
    }

    if (!strstr(canon, "\"execution_profile\":\"sovereign\"") &&
        !strstr(canon, "\"execution_profile\":\"enterprise\"") &&
        !strstr(canon, "\"execution_profile\":\"performance\"")) {
        free(canon);
        free(copy);
        return ARAPP_V2_ERR_MANIFEST_SCHEMA;
    }

    if (!strstr(canon, "\"execution_ring\":\"sovereign_trust\"") &&
        !strstr(canon, "\"execution_ring\":\"devmode_sandbox\"")) {
        free(canon);
        free(copy);
        return ARAPP_V2_ERR_MANIFEST_SCHEMA;
    }

    /* Validate app_id for traversal / injection */
    char app_id_val[256];
    if (extract_top_level_string_field(canon, "app_id", app_id_val, sizeof(app_id_val)) != 0 ||
        !check_safe_app_id(app_id_val)) {
        free(canon);
        free(copy);
        return ARAPP_V2_ERR_MANIFEST_SCHEMA;
    }

    /* Validate persistent_mount if present in storage */
    char mount_val[512];
    if (extract_storage_mount_field(canon, mount_val, sizeof(mount_val)) == 0) {
        if (check_path_traversal(mount_val)) {
            free(canon);
            free(copy);
            return ARAPP_V2_ERR_MANIFEST_SCHEMA;
        }
    }

    free(canon);
    free(copy);
    return ARAPP_V2_OK;
}

int arapp_v2_read(const uint8_t *buffer, size_t size, arapp_v2_package_t *out_pkg) {
    int status;

    if (!buffer || !out_pkg) {
        return ARAPP_V2_ERR_INVALID_ARGUMENT;
    }

    status = arapp_v2_read_header(buffer, size, out_pkg);
    if (status != ARAPP_V2_OK) {
        return status;
    }

    status = arapp_v2_validate_offsets(buffer, size, out_pkg);
    if (status != ARAPP_V2_OK) {
        return status;
    }

    return ARAPP_V2_OK;
}

int arapp_v2_read_and_validate(const uint8_t *buffer, size_t size, uint64_t current_time, arapp_v2_package_t *out_pkg) {
    int status;

    if (!buffer || !out_pkg) {
        return ARAPP_V2_ERR_INVALID_ARGUMENT;
    }

    status = arapp_v2_read(buffer, size, out_pkg);
    if (status != ARAPP_V2_OK) {
        return status;
    }

    status = arapp_v2_validate_anti_replay(out_pkg, current_time, 0U);
    if (status != ARAPP_V2_OK) {
        return status;
    }
    out_pkg->timestamps_valid = 1U;

    status = arapp_v2_validate_manifest(out_pkg);
    if (status != ARAPP_V2_OK) {
        return status;
    }
    out_pkg->manifest_is_canonical = 1U;

    char profile_val[64];
    if (extract_top_level_string_field_n(out_pkg->manifest_json, out_pkg->manifest_len, "execution_profile", profile_val, sizeof(profile_val)) == 0) {
        if (strcmp(profile_val, "sovereign") == 0) {
            if ((out_pkg->container_flags & ARAPP_V2_FLAG_PROFILE_SOVEREIGN) == 0U ||
                out_pkg->header_size_bytes != ARAPP_V2_FILE_HEADER_SIZE ||
                !out_pkg->has_signatures ||
                !out_pkg->signatures) {
                return ARAPP_V2_ERR_SIGNATURE_INVALID;
            }
        }
    }

    return ARAPP_V2_OK;
}

int arapp_v2_compute_digest(const arapp_v2_package_t *pkg, uint8_t out_digest[ARAPP_V2_SHA512_LEN]) {
    alrios_sha512_ctx_t ctx;
    uint8_t wire_prefix[ARAPP_V2_HEADER_PREFIX_SIZE];

    if (!pkg || !out_digest) {
        return ARAPP_V2_ERR_INVALID_ARGUMENT;
    }

    /* Serialize prefix canonically to wire format (big-endian) */
    memcpy(wire_prefix, pkg->prefix.magic, ARAPP_V2_MAGIC_LEN);
    store_u16_be(wire_prefix + 16U, pkg->prefix.format_version);
    store_u16_be(wire_prefix + 18U, pkg->prefix.target_arch);
    store_u32_be(wire_prefix + 20U, pkg->prefix.container_flags);
    store_u64_be(wire_prefix + 24U, pkg->prefix.timestamp_issued);
    store_u64_be(wire_prefix + 32U, pkg->prefix.timestamp_expiry);
    store_u32_be(wire_prefix + 40U, pkg->prefix.header_size_bytes);
    store_u32_be(wire_prefix + 44U, pkg->prefix.entitlements_json_length);
    store_u32_be(wire_prefix + 48U, pkg->prefix.ciphertext_size_bytes);
    memcpy(wire_prefix + 52U, pkg->prefix.aes_gcm_iv, ARAPP_V2_GCM_IV_LEN);
    memcpy(wire_prefix + 64U, pkg->prefix.aes_gcm_tag, ARAPP_V2_GCM_TAG_LEN);
    memcpy(wire_prefix + 80U, pkg->prefix.cleartext_sha512, ARAPP_V2_SHA512_LEN);

    if (alrios_sha512_init(&ctx) != ALRIOS_CRYPTO_OK) {
        return ARAPP_V2_ERR_MEMORY;
    }

    if (alrios_sha512_update(&ctx, wire_prefix, sizeof(wire_prefix)) != ALRIOS_CRYPTO_OK) {
        return ARAPP_V2_ERR_MEMORY;
    }

    if (pkg->manifest_len > 0U && pkg->manifest_json) {
        if (alrios_sha512_update(&ctx, (const uint8_t *)pkg->manifest_json, pkg->manifest_len) != ALRIOS_CRYPTO_OK) {
            return ARAPP_V2_ERR_MEMORY;
        }
    }

    if (pkg->ciphertext_len > 0U && pkg->ciphertext) {
        if (alrios_sha512_update(&ctx, pkg->ciphertext, pkg->ciphertext_len) != ALRIOS_CRYPTO_OK) {
            return ARAPP_V2_ERR_MEMORY;
        }
    }

    if (alrios_sha512_final(&ctx, out_digest) != ALRIOS_CRYPTO_OK) {
        return ARAPP_V2_ERR_MEMORY;
    }

    return ARAPP_V2_OK;
}

int arapp_v2_verify_signatures(const arapp_v2_package_t *pkg, const uint8_t *ed25519_pubkey, const uint8_t *ml_dsa_65_pubkey) {
    uint8_t digest[ARAPP_V2_SHA512_LEN];

    if (!pkg) {
        return ARAPP_V2_ERR_INVALID_ARGUMENT;
    }

    if (!pkg->has_signatures || !pkg->signatures) {
        return ARAPP_V2_ERR_SIGNATURE_INVALID;
    }

    if (!ed25519_pubkey && !ml_dsa_65_pubkey) {
        return ARAPP_V2_ERR_INVALID_ARGUMENT;
    }

    if (arapp_v2_compute_digest(pkg, digest) != ARAPP_V2_OK) {
        return ARAPP_V2_ERR_SIGNATURE_INVALID;
    }

    int hybrid_required = (pkg->container_flags & ARAPP_V2_FLAG_PQC_HYBRID_SIG) != 0;

    if (hybrid_required) {
        if (!ed25519_pubkey || !ml_dsa_65_pubkey) {
            return ARAPP_V2_ERR_SIGNATURE_INVALID;
        }
        if (alrios_ed25519_verify(ed25519_pubkey, digest, pkg->signatures->sig_ed25519) != ALRIOS_CRYPTO_OK) {
            return ARAPP_V2_ERR_SIGNATURE_INVALID;
        }
        if (alrios_ml_dsa_65_verify(ml_dsa_65_pubkey, digest, pkg->signatures->sig_ml_dsa_65) != ALRIOS_CRYPTO_OK) {
            return ARAPP_V2_ERR_SIGNATURE_INVALID;
        }
        return ARAPP_V2_OK;
    }

    if (ed25519_pubkey) {
        if (alrios_ed25519_verify(ed25519_pubkey, digest, pkg->signatures->sig_ed25519) != ALRIOS_CRYPTO_OK) {
            return ARAPP_V2_ERR_SIGNATURE_INVALID;
        }
    }

    int pqc_required = (pkg->container_flags & ARAPP_V2_FLAG_PROFILE_SOVEREIGN) != 0 ||
                       ml_dsa_65_pubkey != NULL;

    if (pqc_required) {
        if (!ml_dsa_65_pubkey) {
            return ARAPP_V2_ERR_SIGNATURE_INVALID;
        }
        if (alrios_ml_dsa_65_verify(ml_dsa_65_pubkey, digest, pkg->signatures->sig_ml_dsa_65) != ALRIOS_CRYPTO_OK) {
            return ARAPP_V2_ERR_SIGNATURE_INVALID;
        }
    }

    return ARAPP_V2_OK;
}

int arapp_v2_build(const arapp_v2_header_prefix_t *prefix,
                   const arapp_v2_signatures_t *signatures,
                   const char *manifest_json, size_t manifest_len,
                   const uint8_t *ciphertext, size_t ciphertext_len,
                   uint8_t *out_buffer, size_t out_capacity,
                   size_t *out_written) {
    if (!prefix || !out_buffer) {
        return ARAPP_V2_ERR_INVALID_ARGUMENT;
    }
    if (manifest_len > 0U && !manifest_json) {
        return ARAPP_V2_ERR_INVALID_ARGUMENT;
    }
    if (ciphertext_len > 0U && !ciphertext) {
        return ARAPP_V2_ERR_INVALID_ARGUMENT;
    }

    size_t header_size = signatures ? ARAPP_V2_FILE_HEADER_SIZE : ARAPP_V2_HEADER_PREFIX_SIZE;

    if (manifest_len > SIZE_MAX - header_size) {
        return ARAPP_V2_ERR_OFFSET_OVERFLOW;
    }
    size_t manifest_end = header_size + manifest_len;

    if (ciphertext_len > SIZE_MAX - manifest_end) {
        return ARAPP_V2_ERR_OFFSET_OVERFLOW;
    }
    size_t total_size = manifest_end + ciphertext_len;

    if (total_size > out_capacity) {
        return ARAPP_V2_ERR_MEMORY;
    }

    if (buffers_overlap(out_buffer, total_size, prefix, sizeof(*prefix)) ||
        (signatures && buffers_overlap(out_buffer, total_size, signatures, sizeof(*signatures))) ||
        (manifest_len > 0U && buffers_overlap(out_buffer, total_size, manifest_json, manifest_len)) ||
        (ciphertext_len > 0U && buffers_overlap(out_buffer, total_size, ciphertext, ciphertext_len))) {
        return ARAPP_V2_ERR_OVERLAP;
    }

    memcpy(out_buffer, prefix->magic, ARAPP_V2_MAGIC_LEN);
    store_u16_be(out_buffer + 16U, prefix->format_version);
    store_u16_be(out_buffer + 18U, prefix->target_arch);
    store_u32_be(out_buffer + 20U, prefix->container_flags);
    store_u64_be(out_buffer + 24U, prefix->timestamp_issued);
    store_u64_be(out_buffer + 32U, prefix->timestamp_expiry);
    store_u32_be(out_buffer + 40U, (uint32_t)header_size);
    store_u32_be(out_buffer + 44U, (uint32_t)manifest_len);
    store_u32_be(out_buffer + 48U, (uint32_t)ciphertext_len);
    memcpy(out_buffer + 52U, prefix->aes_gcm_iv, ARAPP_V2_GCM_IV_LEN);
    memcpy(out_buffer + 64U, prefix->aes_gcm_tag, ARAPP_V2_GCM_TAG_LEN);
    memcpy(out_buffer + 80U, prefix->cleartext_sha512, ARAPP_V2_SHA512_LEN);

    if (signatures) {
        memcpy(out_buffer + ARAPP_V2_HEADER_PREFIX_SIZE, signatures, sizeof(*signatures));
    }

    if (manifest_len > 0U) {
        memcpy(out_buffer + header_size, manifest_json, manifest_len);
    }

    if (ciphertext_len > 0U) {
        memcpy(out_buffer + manifest_end, ciphertext, ciphertext_len);
    }

    if (out_written) {
        *out_written = total_size;
    }

    return ARAPP_V2_OK;
}
