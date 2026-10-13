/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/package/arapp_v2.h"

#include <stdlib.h>
#include <string.h>

int alrios_manifest_canonicalize(const char *in_json, char *out_canon,
                                 size_t max_out);

static const uint8_t arapp_v2_magic[ALRIOS_ARAPP_V2_MAGIC_SIZE] = {
    'A', 'L', 'R', 'I', 'G', 'R', 'O', 'U',
    'P', '@', 'A', 'R', 'A', 'P', 'P', 0U
};

static uint16_t read_u16_be(const uint8_t *value) {
    return (uint16_t)(((uint16_t)value[0] << 8U) | (uint16_t)value[1]);
}

static uint32_t read_u32_be(const uint8_t *value) {
    return ((uint32_t)value[0] << 24U) |
           ((uint32_t)value[1] << 16U) |
           ((uint32_t)value[2] << 8U) |
           (uint32_t)value[3];
}

static uint64_t read_u64_be(const uint8_t *value) {
    uint64_t result = 0U;
    size_t index;

    for (index = 0U; index < 8U; ++index) {
        result = (result << 8U) | (uint64_t)value[index];
    }
    return result;
}

static int checked_add_size(size_t left, size_t right, size_t *out) {
    if (!out || right > SIZE_MAX - left) {
        return 0;
    }
    *out = left + right;
    return 1;
}

static int bytes_all_zero(const uint8_t *data, size_t length) {
    uint8_t accumulator = 0U;
    size_t index;

    for (index = 0U; index < length; ++index) {
        accumulator |= data[index];
    }
    return accumulator == 0U;
}

static int valid_architecture(uint16_t architecture) {
    return architecture == ALRIOS_ARAPP_V2_ARCH_X86_64 ||
           architecture == ALRIOS_ARAPP_V2_ARCH_AARCH64 ||
           architecture == ALRIOS_ARAPP_V2_ARCH_RISCV64;
}

static int valid_flags(uint32_t flags) {
    uint32_t profiles = flags & (ALRIOS_ARAPP_V2_FLAG_PROFILE_SOVEREIGN |
                                  ALRIOS_ARAPP_V2_FLAG_PROFILE_ENTERPRISE |
                                  ALRIOS_ARAPP_V2_FLAG_PROFILE_PERFORMANCE);
    uint32_t rings = flags & (ALRIOS_ARAPP_V2_FLAG_RING_SOVEREIGN |
                              ALRIOS_ARAPP_V2_FLAG_RING_DEVMODE);

    if ((flags & ~ALRIOS_ARAPP_V2_FLAGS_MASK) != 0U) {
        return 0;
    }
    if (profiles == 0U || (profiles & (profiles - 1U)) != 0U) {
        return 0;
    }
    if (rings == 0U || (rings & (rings - 1U)) != 0U) {
        return 0;
    }
    if ((flags & ALRIOS_ARAPP_V2_FLAG_PROFILE_SOVEREIGN) != 0U &&
        (flags & ALRIOS_ARAPP_V2_FLAG_RING_SOVEREIGN) == 0U) {
        return 0;
    }
    if ((flags & ALRIOS_ARAPP_V2_FLAG_RING_SOVEREIGN) != 0U &&
        (flags & ALRIOS_ARAPP_V2_FLAG_PQC_HYBRID_SIG) == 0U) {
        return 0;
    }
    return 1;
}

static int valid_time_window(uint64_t issued_at, uint64_t expires_at,
                             uint64_t now, uint64_t max_clock_skew) {
    uint64_t latest_acceptable_issue;

    if (issued_at == 0U || expires_at == 0U || issued_at >= expires_at) {
        return 0;
    }
    latest_acceptable_issue = now;
    if (max_clock_skew > UINT64_MAX - latest_acceptable_issue) {
        latest_acceptable_issue = UINT64_MAX;
    } else {
        latest_acceptable_issue += max_clock_skew;
    }
    if (issued_at > latest_acceptable_issue || now > expires_at) {
        return 0;
    }
    return 1;
}

static int validate_canonical_manifest(const uint8_t *manifest,
                                       size_t manifest_len) {
    char *input;
    char *canonical;
    size_t capacity;
    int canonical_status;
    int result = ALRIOS_ARAPP_V2_ERR_MANIFEST;

    if (!manifest || manifest_len == 0U ||
        manifest_len > ALRIOS_ARAPP_V2_MAX_MANIFEST_SIZE ||
        memchr(manifest, '\0', manifest_len) != NULL ||
        manifest[0] != (uint8_t)'{' ||
        !checked_add_size(manifest_len, 1U, &capacity)) {
        return ALRIOS_ARAPP_V2_ERR_MANIFEST;
    }

    input = (char *)malloc(capacity);
    canonical = (char *)malloc(capacity);
    if (!input || !canonical) {
        free(canonical);
        free(input);
        return ALRIOS_ARAPP_V2_ERR_MEMORY;
    }

    memcpy(input, manifest, manifest_len);
    input[manifest_len] = '\0';
    canonical_status = alrios_manifest_canonicalize(input, canonical, capacity);
    if (canonical_status == 0 &&
        strlen(canonical) == manifest_len &&
        memcmp(canonical, manifest, manifest_len) == 0) {
        result = ALRIOS_ARAPP_V2_OK;
    }

    free(canonical);
    free(input);
    return result;
}

static int input_range_fits(const uint8_t *data, size_t data_len) {
    uintptr_t start = (uintptr_t)data;

    return data_len <= UINTPTR_MAX - start;
}

int alrios_arapp_v2_validate(
    const uint8_t *data,
    size_t data_len,
    uint64_t now,
    uint64_t max_clock_skew,
    alrios_arapp_v2_view_t *out_view
) {
    uint16_t version;
    uint16_t architecture;
    uint32_t flags;
    uint64_t issued_at;
    uint64_t expires_at;
    uint32_t header_size;
    uint32_t manifest_len_u32;
    uint32_t ciphertext_len_u32;
    size_t manifest_end;
    size_t expected_size;
    size_t manifest_len;
    size_t ciphertext_len;
    const uint8_t *padding;
    int result;

    if (!out_view) {
        return ALRIOS_ARAPP_V2_ERR_ARGUMENT;
    }
    memset(out_view, 0, sizeof(*out_view));
    if (!data) {
        return ALRIOS_ARAPP_V2_ERR_ARGUMENT;
    }
    if (!input_range_fits(data, data_len)) {
        return ALRIOS_ARAPP_V2_ERR_LAYOUT;
    }
    if (data_len < ALRIOS_ARAPP_V2_HEADER_SIZE) {
        return ALRIOS_ARAPP_V2_ERR_TRUNCATED;
    }
    if (memcmp(data, arapp_v2_magic, sizeof(arapp_v2_magic)) != 0) {
        return ALRIOS_ARAPP_V2_ERR_MAGIC;
    }

    version = read_u16_be(data + 16U);
    architecture = read_u16_be(data + 18U);
    flags = read_u32_be(data + 20U);
    issued_at = read_u64_be(data + 24U);
    expires_at = read_u64_be(data + 32U);
    header_size = read_u32_be(data + 40U);
    manifest_len_u32 = read_u32_be(data + 44U);
    ciphertext_len_u32 = read_u32_be(data + 48U);

    if (version != ALRIOS_ARAPP_V2_VERSION) {
        return ALRIOS_ARAPP_V2_ERR_VERSION;
    }
    if (!valid_architecture(architecture)) {
        return ALRIOS_ARAPP_V2_ERR_ARCH;
    }
    if (!valid_flags(flags)) {
        return ALRIOS_ARAPP_V2_ERR_FLAGS;
    }
    if (header_size != ALRIOS_ARAPP_V2_HEADER_SIZE ||
        manifest_len_u32 == 0U ||
        manifest_len_u32 > ALRIOS_ARAPP_V2_MAX_MANIFEST_SIZE) {
        return ALRIOS_ARAPP_V2_ERR_LAYOUT;
    }

    manifest_len = (size_t)manifest_len_u32;
    ciphertext_len = (size_t)ciphertext_len_u32;
    if (!checked_add_size((size_t)header_size, manifest_len, &manifest_end) ||
        !checked_add_size(manifest_end, ciphertext_len, &expected_size) ||
        expected_size != data_len) {
        return ALRIOS_ARAPP_V2_ERR_LAYOUT;
    }
    if (!valid_time_window(issued_at, expires_at, now, max_clock_skew)) {
        return ALRIOS_ARAPP_V2_ERR_TIME;
    }

    padding = data + ALRIOS_ARAPP_V2_PREFIX_SIZE +
              ALRIOS_ARAPP_V2_ED25519_SIGNATURE_SIZE +
              ALRIOS_ARAPP_V2_ML_DSA_65_SIGNATURE_SIZE;
    if (!bytes_all_zero(padding, ALRIOS_ARAPP_V2_SIGNATURE_PADDING_SIZE)) {
        return ALRIOS_ARAPP_V2_ERR_PADDING;
    }

    result = validate_canonical_manifest(data + header_size, manifest_len);
    if (result != ALRIOS_ARAPP_V2_OK) {
        return result;
    }

    out_view->target_arch = architecture;
    out_view->flags = flags;
    out_view->issued_at = issued_at;
    out_view->expires_at = expires_at;
    out_view->prefix = data;
    out_view->prefix_len = ALRIOS_ARAPP_V2_PREFIX_SIZE;
    out_view->header_len = header_size;
    out_view->gcm_iv = data + 52U;
    out_view->gcm_tag = data + 64U;
    out_view->cleartext_sha512 = data + 80U;
    out_view->ed25519_signature = data + ALRIOS_ARAPP_V2_PREFIX_SIZE;
    out_view->ml_dsa_65_signature = out_view->ed25519_signature +
                                    ALRIOS_ARAPP_V2_ED25519_SIGNATURE_SIZE;
    out_view->manifest = data + header_size;
    out_view->manifest_len = manifest_len;
    out_view->ciphertext = data + manifest_end;
    out_view->ciphertext_len = ciphertext_len;
    return ALRIOS_ARAPP_V2_OK;
}
