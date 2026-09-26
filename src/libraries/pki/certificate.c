/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/pki/certificate.h"
#include "alrios/crypto_verify.h"

#include <limits.h>
#include <string.h>

static const uint8_t certificate_magic[8] = {
    0x41U, 0x4cU, 0x52U, 0x49U, 0x43U, 0x45U, 0x52U, 0x54U
};
static const uint8_t tbs_domain[ALRIOS_CERTIFICATE_TBS_DOMAIN_SIZE] = {
    0x41U, 0x4cU, 0x52U, 0x49U, 0x4fU, 0x53U, 0x2dU, 0x43U, 0x45U,
    0x52U, 0x54U, 0x2dU, 0x54U, 0x42U, 0x53U, 0x2dU, 0x56U, 0x31U
};
static const uint8_t key_id_domain[16] = {
    0x41U, 0x4cU, 0x52U, 0x49U, 0x4fU, 0x53U, 0x2dU, 0x4bU,
    0x45U, 0x59U, 0x2dU, 0x49U, 0x44U, 0x2dU, 0x56U, 0x31U
};

static uint16_t load_u16_be(const uint8_t *input) {
    return (uint16_t)(((uint16_t)input[0] << 8U) | (uint16_t)input[1]);
}

static uint32_t load_u32_be(const uint8_t *input) {
    return ((uint32_t)input[0] << 24U) | ((uint32_t)input[1] << 16U) |
           ((uint32_t)input[2] << 8U) | (uint32_t)input[3];
}

static uint64_t load_u64_be(const uint8_t *input) {
    uint64_t value = 0U;
    size_t index;
    for (index = 0U; index < 8U; ++index) {
        value = (value << 8U) | (uint64_t)input[index];
    }
    return value;
}

static void store_u16_be(uint8_t *output, uint16_t value) {
    output[0] = (uint8_t)(value >> 8U);
    output[1] = (uint8_t)value;
}

static void store_u32_be(uint8_t *output, uint32_t value) {
    output[0] = (uint8_t)(value >> 24U);
    output[1] = (uint8_t)(value >> 16U);
    output[2] = (uint8_t)(value >> 8U);
    output[3] = (uint8_t)value;
}

static void store_u64_be(uint8_t *output, uint64_t value) {
    size_t index;
    for (index = 8U; index > 0U; --index) {
        output[index - 1U] = (uint8_t)value;
        value >>= 8U;
    }
}

static int ranges_overlap(const void *first, size_t first_len,
                          const void *second, size_t second_len) {
    uintptr_t first_start;
    uintptr_t second_start;
    uintptr_t first_end;
    uintptr_t second_end;

    if (!first || !second || first_len == 0U || second_len == 0U) {
        return 0;
    }
    first_start = (uintptr_t)first;
    second_start = (uintptr_t)second;
    if (first_len > UINTPTR_MAX - first_start || second_len > UINTPTR_MAX - second_start) {
        return 1;
    }
    first_end = first_start + first_len;
    second_end = second_start + second_len;
    return first_start < second_end && second_start < first_end;
}

static int public_key_length(uint8_t algorithm, size_t *length) {
    if (!length) {
        return ALRIOS_CERTIFICATE_ERR_INVALID_ARGUMENT;
    }
    switch (algorithm) {
        case ALRIOS_PUBLIC_KEY_ALGORITHM_ED25519:
            *length = ALRIOS_CERTIFICATE_ED25519_KEY_SIZE;
            return ALRIOS_CERTIFICATE_OK;
        case ALRIOS_PUBLIC_KEY_ALGORITHM_ML_DSA_65:
            *length = ALRIOS_CERTIFICATE_ML_DSA_65_KEY_SIZE;
            return ALRIOS_CERTIFICATE_OK;
        case ALRIOS_PUBLIC_KEY_ALGORITHM_HYBRID_ED25519_ML_DSA_65:
            *length = ALRIOS_CERTIFICATE_HYBRID_KEY_SIZE;
            return ALRIOS_CERTIFICATE_OK;
        default:
            return ALRIOS_CERTIFICATE_ERR_ALGORITHM;
    }
}

static int signature_length(uint8_t algorithm, size_t *length) {
    if (!length) {
        return ALRIOS_CERTIFICATE_ERR_INVALID_ARGUMENT;
    }
    switch (algorithm) {
        case ALRIOS_SIGNATURE_ALGORITHM_ED25519:
            *length = ALRIOS_CERTIFICATE_ED25519_SIG_SIZE;
            return ALRIOS_CERTIFICATE_OK;
        case ALRIOS_SIGNATURE_ALGORITHM_ML_DSA_65:
            *length = ALRIOS_CERTIFICATE_ML_DSA_65_SIG_SIZE;
            return ALRIOS_CERTIFICATE_OK;
        case ALRIOS_SIGNATURE_ALGORITHM_HYBRID_ED25519_ML_DSA_65:
            *length = ALRIOS_CERTIFICATE_HYBRID_SIG_SIZE;
            return ALRIOS_CERTIFICATE_OK;
        default:
            return ALRIOS_CERTIFICATE_ERR_ALGORITHM;
    }
}

static int valid_subject(const uint8_t *subject, size_t subject_len) {
    size_t index;
    if (!subject || subject_len == 0U || subject_len > ALRIOS_CERTIFICATE_SUBJECT_MAX) {
        return 0;
    }
    for (index = 0U; index < subject_len; ++index) {
        const uint8_t ch = subject[index];
        if (!((ch >= (uint8_t)'a' && ch <= (uint8_t)'z') ||
              (ch >= (uint8_t)'0' && ch <= (uint8_t)'9') ||
              ch == (uint8_t)'.' || ch == (uint8_t)'-' || ch == (uint8_t)'_')) {
            return 0;
        }
    }
    return 1;
}

static int validate_certificate(const alrios_certificate_t *certificate,
                                size_t *out_wire_len) {
    size_t expected_key_len = 0U;
    size_t expected_signature_len = 0U;
    size_t wire_len;
    size_t index;
    int serial_all_zero = 1;
    int key_all_zero = 1;
    int status;

    if (!certificate || !out_wire_len) {
        return ALRIOS_CERTIFICATE_ERR_INVALID_ARGUMENT;
    }
    if (certificate->version != ALRIOS_CERTIFICATE_VERSION_1) {
        return ALRIOS_CERTIFICATE_ERR_VERSION;
    }
    if (certificate->role < ALRIOS_CERTIFICATE_ROLE_ROOT ||
        certificate->role > ALRIOS_CERTIFICATE_ROLE_LEAF) {
        return ALRIOS_CERTIFICATE_ERR_FIELD;
    }
    status = public_key_length(certificate->public_key_algorithm, &expected_key_len);
    if (status != ALRIOS_CERTIFICATE_OK) {
        return status;
    }
    status = signature_length(certificate->signature_algorithm, &expected_signature_len);
    if (status != ALRIOS_CERTIFICATE_OK) {
        return status;
    }
    if (certificate->public_key_len != expected_key_len ||
        certificate->signature_len != expected_signature_len) {
        return ALRIOS_CERTIFICATE_ERR_LENGTH;
    }
    if (!valid_subject(certificate->subject, certificate->subject_len) ||
        certificate->not_before > certificate->not_after) {
        return ALRIOS_CERTIFICATE_ERR_FIELD;
    }
    for (index = 0U; index < ALRIOS_CERTIFICATE_SERIAL_SIZE; ++index) {
        serial_all_zero &= certificate->serial[index] == 0U;
    }
    for (index = 0U; index < certificate->public_key_len; ++index) {
        key_all_zero &= certificate->public_key[index] == 0U;
    }
    if (serial_all_zero || key_all_zero) {
        return ALRIOS_CERTIFICATE_ERR_FIELD;
    }
    if (certificate->subject_len > SIZE_MAX - ALRIOS_CERTIFICATE_HEADER_SIZE_V1) {
        return ALRIOS_CERTIFICATE_ERR_LENGTH;
    }
    wire_len = ALRIOS_CERTIFICATE_HEADER_SIZE_V1 + certificate->subject_len;
    if (certificate->public_key_len > SIZE_MAX - wire_len) {
        return ALRIOS_CERTIFICATE_ERR_LENGTH;
    }
    wire_len += certificate->public_key_len;
    if (certificate->signature_len > SIZE_MAX - wire_len) {
        return ALRIOS_CERTIFICATE_ERR_LENGTH;
    }
    wire_len += certificate->signature_len;
    if (wire_len > ALRIOS_CERTIFICATE_WIRE_MAX || wire_len > UINT32_MAX) {
        return ALRIOS_CERTIFICATE_ERR_LENGTH;
    }
    *out_wire_len = wire_len;
    return ALRIOS_CERTIFICATE_OK;
}

static void encode_header(const alrios_certificate_t *certificate,
                          size_t wire_len,
                          uint8_t header[ALRIOS_CERTIFICATE_HEADER_SIZE_V1]) {
    memset(header, 0, ALRIOS_CERTIFICATE_HEADER_SIZE_V1);
    memcpy(header, certificate_magic, sizeof(certificate_magic));
    store_u16_be(header + 8U, certificate->version);
    header[10U] = certificate->role;
    header[11U] = certificate->public_key_algorithm;
    header[12U] = certificate->signature_algorithm;
    store_u32_be(header + 16U, (uint32_t)wire_len);
    store_u16_be(header + 20U, (uint16_t)certificate->subject_len);
    store_u16_be(header + 22U, (uint16_t)certificate->public_key_len);
    store_u16_be(header + 24U, (uint16_t)certificate->signature_len);
    memcpy(header + 28U, certificate->serial, ALRIOS_CERTIFICATE_SERIAL_SIZE);
    memcpy(header + 44U, certificate->issuer_key_id, ALRIOS_CERTIFICATE_KEY_ID_SIZE);
    store_u64_be(header + 76U, certificate->not_before);
    store_u64_be(header + 84U, certificate->not_after);
}

int alrios_certificate_decode(const uint8_t *wire, size_t wire_len,
                              alrios_certificate_t *out_certificate) {
    alrios_certificate_t decoded;
    size_t expected_key_len = 0U;
    size_t expected_signature_len = 0U;
    size_t subject_len;
    size_t public_key_len;
    size_t signature_len;
    size_t total_len;
    size_t offset;
    int status;

    if (!out_certificate) {
        return ALRIOS_CERTIFICATE_ERR_INVALID_ARGUMENT;
    }
    if (!wire || ranges_overlap(wire, wire_len, out_certificate, sizeof(*out_certificate))) {
        memset(out_certificate, 0, sizeof(*out_certificate));
        return !wire ? ALRIOS_CERTIFICATE_ERR_INVALID_ARGUMENT : ALRIOS_CERTIFICATE_ERR_OVERLAP;
    }
    memset(&decoded, 0, sizeof(decoded));
    if (wire_len < ALRIOS_CERTIFICATE_HEADER_SIZE_V1) {
        status = ALRIOS_CERTIFICATE_ERR_TRUNCATED;
        goto fail;
    }
    if (memcmp(wire, certificate_magic, sizeof(certificate_magic)) != 0) {
        status = ALRIOS_CERTIFICATE_ERR_BAD_MAGIC;
        goto fail;
    }
    decoded.version = load_u16_be(wire + 8U);
    decoded.role = wire[10U];
    decoded.public_key_algorithm = wire[11U];
    decoded.signature_algorithm = wire[12U];
    if (decoded.version != ALRIOS_CERTIFICATE_VERSION_1) {
        status = ALRIOS_CERTIFICATE_ERR_VERSION;
        goto fail;
    }
    if (wire[13U] != 0U || wire[14U] != 0U || wire[15U] != 0U ||
        wire[26U] != 0U || wire[27U] != 0U) {
        status = ALRIOS_CERTIFICATE_ERR_NONCANONICAL;
        goto fail;
    }
    if ((size_t)load_u32_be(wire + 16U) != wire_len ||
        wire_len > ALRIOS_CERTIFICATE_WIRE_MAX) {
        status = ALRIOS_CERTIFICATE_ERR_LENGTH;
        goto fail;
    }
    subject_len = load_u16_be(wire + 20U);
    public_key_len = load_u16_be(wire + 22U);
    signature_len = load_u16_be(wire + 24U);
    status = public_key_length(decoded.public_key_algorithm, &expected_key_len);
    if (status != ALRIOS_CERTIFICATE_OK) {
        goto fail;
    }
    status = signature_length(decoded.signature_algorithm, &expected_signature_len);
    if (status != ALRIOS_CERTIFICATE_OK) {
        goto fail;
    }
    if (subject_len == 0U || subject_len > ALRIOS_CERTIFICATE_SUBJECT_MAX ||
        public_key_len != expected_key_len || signature_len != expected_signature_len) {
        status = ALRIOS_CERTIFICATE_ERR_LENGTH;
        goto fail;
    }
    total_len = ALRIOS_CERTIFICATE_HEADER_SIZE_V1;
    if (subject_len > SIZE_MAX - total_len) {
        status = ALRIOS_CERTIFICATE_ERR_LENGTH;
        goto fail;
    }
    total_len += subject_len;
    if (public_key_len > SIZE_MAX - total_len) {
        status = ALRIOS_CERTIFICATE_ERR_LENGTH;
        goto fail;
    }
    total_len += public_key_len;
    if (signature_len > SIZE_MAX - total_len) {
        status = ALRIOS_CERTIFICATE_ERR_LENGTH;
        goto fail;
    }
    total_len += signature_len;
    if (total_len != wire_len) {
        status = ALRIOS_CERTIFICATE_ERR_LENGTH;
        goto fail;
    }
    memcpy(decoded.serial, wire + 28U, ALRIOS_CERTIFICATE_SERIAL_SIZE);
    memcpy(decoded.issuer_key_id, wire + 44U, ALRIOS_CERTIFICATE_KEY_ID_SIZE);
    decoded.not_before = load_u64_be(wire + 76U);
    decoded.not_after = load_u64_be(wire + 84U);
    decoded.subject_len = subject_len;
    decoded.public_key_len = public_key_len;
    decoded.signature_len = signature_len;
    offset = ALRIOS_CERTIFICATE_HEADER_SIZE_V1;
    memcpy(decoded.subject, wire + offset, subject_len);
    offset += subject_len;
    memcpy(decoded.public_key, wire + offset, public_key_len);
    offset += public_key_len;
    memcpy(decoded.signature, wire + offset, signature_len);
    status = validate_certificate(&decoded, &total_len);
    if (status != ALRIOS_CERTIFICATE_OK || total_len != wire_len) {
        status = status == ALRIOS_CERTIFICATE_OK ? ALRIOS_CERTIFICATE_ERR_LENGTH : status;
        goto fail;
    }
    *out_certificate = decoded;
    alrios_explicit_zeroize(&decoded, sizeof(decoded));
    return ALRIOS_CERTIFICATE_OK;

fail:
    alrios_explicit_zeroize(&decoded, sizeof(decoded));
    memset(out_certificate, 0, sizeof(*out_certificate));
    return status;
}

int alrios_certificate_encode(const alrios_certificate_t *certificate,
                              uint8_t *out_wire, size_t out_capacity,
                              size_t *out_written) {
    uint8_t header[ALRIOS_CERTIFICATE_HEADER_SIZE_V1];
    size_t wire_len = 0U;
    size_t offset;
    int status;

    if (out_written) {
        *out_written = 0U;
    }
    if (!certificate || !out_wire || !out_written) {
        return ALRIOS_CERTIFICATE_ERR_INVALID_ARGUMENT;
    }
    status = validate_certificate(certificate, &wire_len);
    if (status != ALRIOS_CERTIFICATE_OK) {
        return status;
    }
    if (out_capacity < wire_len) {
        return ALRIOS_CERTIFICATE_ERR_CAPACITY;
    }
    if (ranges_overlap(certificate, sizeof(*certificate), out_wire, wire_len)) {
        return ALRIOS_CERTIFICATE_ERR_OVERLAP;
    }
    encode_header(certificate, wire_len, header);
    memcpy(out_wire, header, sizeof(header));
    offset = sizeof(header);
    memcpy(out_wire + offset, certificate->subject, certificate->subject_len);
    offset += certificate->subject_len;
    memcpy(out_wire + offset, certificate->public_key, certificate->public_key_len);
    offset += certificate->public_key_len;
    memcpy(out_wire + offset, certificate->signature, certificate->signature_len);
    *out_written = wire_len;
    return ALRIOS_CERTIFICATE_OK;
}

int alrios_certificate_encode_tbs(const alrios_certificate_t *certificate,
                                  uint8_t *out_tbs, size_t out_capacity,
                                  size_t *out_written) {
    uint8_t header[ALRIOS_CERTIFICATE_HEADER_SIZE_V1];
    size_t wire_len = 0U;
    size_t tbs_len;
    size_t offset;
    int status;

    if (out_written) {
        *out_written = 0U;
    }
    if (!certificate || !out_tbs || !out_written) {
        return ALRIOS_CERTIFICATE_ERR_INVALID_ARGUMENT;
    }
    status = validate_certificate(certificate, &wire_len);
    if (status != ALRIOS_CERTIFICATE_OK) {
        return status;
    }
    tbs_len = ALRIOS_CERTIFICATE_TBS_DOMAIN_SIZE + ALRIOS_CERTIFICATE_HEADER_SIZE_V1 +
              certificate->subject_len + certificate->public_key_len;
    if (tbs_len > ALRIOS_CERTIFICATE_TBS_MAX || out_capacity < tbs_len) {
        return ALRIOS_CERTIFICATE_ERR_CAPACITY;
    }
    if (ranges_overlap(certificate, sizeof(*certificate), out_tbs, tbs_len)) {
        return ALRIOS_CERTIFICATE_ERR_OVERLAP;
    }
    encode_header(certificate, wire_len, header);
    memcpy(out_tbs, tbs_domain, sizeof(tbs_domain));
    offset = sizeof(tbs_domain);
    memcpy(out_tbs + offset, header, sizeof(header));
    offset += sizeof(header);
    memcpy(out_tbs + offset, certificate->subject, certificate->subject_len);
    offset += certificate->subject_len;
    memcpy(out_tbs + offset, certificate->public_key, certificate->public_key_len);
    *out_written = tbs_len;
    return ALRIOS_CERTIFICATE_OK;
}

int alrios_certificate_public_key_id(const alrios_certificate_t *certificate,
                                     uint8_t out_key_id[ALRIOS_CERTIFICATE_KEY_ID_SIZE]) {
    alrios_sha512_ctx_t context;
    uint8_t algorithm_and_length[3];
    uint8_t digest[SHA512_DIGEST_LEN];
    size_t wire_len = 0U;
    int status;

    if (out_key_id) {
        memset(out_key_id, 0, ALRIOS_CERTIFICATE_KEY_ID_SIZE);
    }
    if (!certificate || !out_key_id) {
        return ALRIOS_CERTIFICATE_ERR_INVALID_ARGUMENT;
    }
    status = validate_certificate(certificate, &wire_len);
    if (status != ALRIOS_CERTIFICATE_OK) {
        return status;
    }
    (void)wire_len;
    algorithm_and_length[0] = certificate->public_key_algorithm;
    store_u16_be(algorithm_and_length + 1U, (uint16_t)certificate->public_key_len);
    if (alrios_sha512_init(&context) != ALRIOS_CRYPTO_OK ||
        alrios_sha512_update(&context, key_id_domain, sizeof(key_id_domain)) != ALRIOS_CRYPTO_OK ||
        alrios_sha512_update(&context, algorithm_and_length, sizeof(algorithm_and_length)) != ALRIOS_CRYPTO_OK ||
        alrios_sha512_update(&context, certificate->public_key, certificate->public_key_len) != ALRIOS_CRYPTO_OK ||
        alrios_sha512_final(&context, digest) != ALRIOS_CRYPTO_OK) {
        alrios_explicit_zeroize(&context, sizeof(context));
        alrios_explicit_zeroize(digest, sizeof(digest));
        return ALRIOS_CERTIFICATE_ERR_FIELD;
    }
    memcpy(out_key_id, digest, ALRIOS_CERTIFICATE_KEY_ID_SIZE);
    alrios_explicit_zeroize(&context, sizeof(context));
    alrios_explicit_zeroize(digest, sizeof(digest));
    return ALRIOS_CERTIFICATE_OK;
}

static int validate_store(const alrios_trust_store_t *store) {
    if (!store || store->anchor_count > ALRIOS_TRUST_STORE_MAX_ANCHORS || store->sealed > 1U) {
        return !store ? ALRIOS_CERTIFICATE_ERR_INVALID_ARGUMENT : ALRIOS_CERTIFICATE_ERR_STATE;
    }
    return ALRIOS_CERTIFICATE_OK;
}

void alrios_trust_store_init(alrios_trust_store_t *store) {
    if (store) {
        memset(store, 0, sizeof(*store));
    }
}

int alrios_trust_store_add_anchor(alrios_trust_store_t *store,
                                  const alrios_certificate_t *certificate) {
    uint8_t new_key_id[ALRIOS_CERTIFICATE_KEY_ID_SIZE];
    uint8_t existing_key_id[ALRIOS_CERTIFICATE_KEY_ID_SIZE];
    size_t wire_len = 0U;
    size_t index;
    int status = validate_store(store);

    if (status != ALRIOS_CERTIFICATE_OK || !certificate) {
        return status != ALRIOS_CERTIFICATE_OK ? status : ALRIOS_CERTIFICATE_ERR_INVALID_ARGUMENT;
    }
    if (store->sealed != 0U) {
        return ALRIOS_CERTIFICATE_ERR_SEALED;
    }
    if (certificate->role != ALRIOS_CERTIFICATE_ROLE_ROOT) {
        return ALRIOS_CERTIFICATE_ERR_FIELD;
    }
    status = validate_certificate(certificate, &wire_len);
    if (status != ALRIOS_CERTIFICATE_OK) {
        return status;
    }
    (void)wire_len;
    if (store->anchor_count >= ALRIOS_TRUST_STORE_MAX_ANCHORS) {
        return ALRIOS_CERTIFICATE_ERR_CAPACITY;
    }
    status = alrios_certificate_public_key_id(certificate, new_key_id);
    if (status != ALRIOS_CERTIFICATE_OK) {
        return status;
    }
    for (index = 0U; index < store->anchor_count; ++index) {
        status = alrios_certificate_public_key_id(&store->anchors[index], existing_key_id);
        if (status != ALRIOS_CERTIFICATE_OK) {
            goto cleanup;
        }
        if (alrios_constant_time_memcmp(new_key_id, existing_key_id,
                                        ALRIOS_CERTIFICATE_KEY_ID_SIZE) == 0) {
            status = ALRIOS_CERTIFICATE_ERR_DUPLICATE;
            goto cleanup;
        }
    }
    store->anchors[store->anchor_count] = *certificate;
    ++store->anchor_count;
    status = ALRIOS_CERTIFICATE_OK;
cleanup:
    alrios_explicit_zeroize(new_key_id, sizeof(new_key_id));
    alrios_explicit_zeroize(existing_key_id, sizeof(existing_key_id));
    return status;
}

int alrios_trust_store_seal(alrios_trust_store_t *store) {
    size_t wire_len = 0U;
    size_t index;
    int status = validate_store(store);

    if (status != ALRIOS_CERTIFICATE_OK) {
        return status;
    }
    if (store->anchor_count == 0U) {
        return ALRIOS_CERTIFICATE_ERR_FIELD;
    }
    for (index = 0U; index < store->anchor_count; ++index) {
        if (store->anchors[index].role != ALRIOS_CERTIFICATE_ROLE_ROOT) {
            return ALRIOS_CERTIFICATE_ERR_STATE;
        }
        status = validate_certificate(&store->anchors[index], &wire_len);
        if (status != ALRIOS_CERTIFICATE_OK) {
            return status;
        }
    }
    store->sealed = 1U;
    return ALRIOS_CERTIFICATE_OK;
}

int alrios_trust_store_lookup(const alrios_trust_store_t *store,
                              const uint8_t key_id[ALRIOS_CERTIFICATE_KEY_ID_SIZE],
                              const alrios_certificate_t **out_certificate) {
    uint8_t candidate[ALRIOS_CERTIFICATE_KEY_ID_SIZE];
    size_t index;
    int status;

    if (out_certificate) {
        *out_certificate = NULL;
    }
    if (!store || !key_id || !out_certificate) {
        return ALRIOS_CERTIFICATE_ERR_INVALID_ARGUMENT;
    }
    status = validate_store(store);
    if (status != ALRIOS_CERTIFICATE_OK) {
        return status;
    }
    if (store->sealed == 0U) {
        return ALRIOS_CERTIFICATE_ERR_SEALED;
    }
    for (index = 0U; index < store->anchor_count; ++index) {
        status = alrios_certificate_public_key_id(&store->anchors[index], candidate);
        if (status != ALRIOS_CERTIFICATE_OK) {
            alrios_explicit_zeroize(candidate, sizeof(candidate));
            return status;
        }
        if (alrios_constant_time_memcmp(candidate, key_id,
                                        ALRIOS_CERTIFICATE_KEY_ID_SIZE) == 0) {
            *out_certificate = &store->anchors[index];
            alrios_explicit_zeroize(candidate, sizeof(candidate));
            return ALRIOS_CERTIFICATE_OK;
        }
    }
    alrios_explicit_zeroize(candidate, sizeof(candidate));
    return ALRIOS_CERTIFICATE_ERR_NOT_FOUND;
}

void alrios_trust_store_clear(alrios_trust_store_t *store) {
    if (store) {
        alrios_explicit_zeroize(store, sizeof(*store));
    }
}
