/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "alrios/crash_report.h"
#include "alrios/memory/sensitive.h"
#include "alrios/crypto_verify.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(__linux__)
#include <sys/random.h>
#endif

static int get_secure_random(uint8_t *buf, size_t len) {
#if defined(__linux__)
    ssize_t ret = getrandom(buf, len, 0);
    if (ret == (ssize_t)len) {
        return 0;
    }
#endif
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    ssize_t r = read(fd, buf, len);
    close(fd);
    return (r == (ssize_t)len) ? 0 : -1;
}

int alrios_crash_scrub_state(alrios_crash_state_t *state) {
    if (!state) {
        return ALRIOS_CRASH_ERR_INVALID_ARGUMENT;
    }

    /* Check if memory sample falls into sensitive range */
    if (state->memory_sample_len > 0 && state->memory_sample_address != 0) {
        if (alrios_sensitive_is_registered((const void *)(uintptr_t)state->memory_sample_address,
                                           state->memory_sample_len)) {
            alrios_explicit_zeroize(state->memory_sample, state->memory_sample_len);
            state->memory_sample_len = 0;
            state->memory_sample_address = 0;
        }
    }

    /* Check stack pointer and instruction pointer */
    if (state->stack_pointer != 0 &&
        alrios_sensitive_is_registered((const void *)(uintptr_t)state->stack_pointer, 8)) {
        state->stack_pointer = 0;
    }

    if (state->fault_address != 0 &&
        alrios_sensitive_is_registered((const void *)(uintptr_t)state->fault_address, 8)) {
        state->fault_address = 0;
    }

    /* Scrub register values that point into sensitive ranges */
    for (size_t i = 0; i < state->register_count && i < ALRIOS_CRASH_MAX_REGISTERS; ++i) {
        if (state->registers[i] != 0 &&
            alrios_sensitive_is_registered((const void *)(uintptr_t)state->registers[i], 8)) {
            state->registers[i] = 0;
        }
    }

    /* Scrub stack frame return addresses if pointing to sensitive ranges */
    for (size_t i = 0; i < state->frame_count && i < ALRIOS_CRASH_MAX_FRAMES; ++i) {
        if (state->stack_frames[i] != 0 &&
            alrios_sensitive_is_registered((const void *)(uintptr_t)state->stack_frames[i], 8)) {
            state->stack_frames[i] = 0;
        }
    }

    return ALRIOS_CRASH_OK;
}

int alrios_crash_serialize_state(const alrios_crash_state_t *state,
                                 uint8_t *out_buf,
                                 size_t buf_capacity,
                                 size_t *out_len) {
    if (!state || !out_buf || buf_capacity == 0) {
        return ALRIOS_CRASH_ERR_INVALID_ARGUMENT;
    }

    /* Structured JSON serialization */
    char temp[4096];
    size_t offset = 0;

    int n = snprintf(temp + offset, sizeof(temp) - offset,
                     "{\n"
                     "  \"app_id\": \"%s\",\n"
                     "  \"signal\": %d,\n"
                     "  \"error_code\": %d,\n"
                     "  \"pid\": %d,\n"
                     "  \"fault_address\": \"0x%" PRIx64 "\",\n"
                     "  \"ip\": \"0x%" PRIx64 "\",\n"
                     "  \"sp\": \"0x%" PRIx64 "\",\n"
                     "  \"registers\": [",
                     state->app_id,
                     state->signal_number,
                     state->error_code,
                     state->faulting_pid,
                     state->fault_address,
                     state->instruction_pointer,
                     state->stack_pointer);
    if (n < 0 || (size_t)n >= sizeof(temp) - offset) {
        return ALRIOS_CRASH_ERR_CORRUPT;
    }
    offset += (size_t)n;

    for (size_t i = 0; i < state->register_count && i < ALRIOS_CRASH_MAX_REGISTERS; ++i) {
        n = snprintf(temp + offset, sizeof(temp) - offset,
                     "%s\"0x%" PRIx64 "\"",
                     (i == 0) ? "" : ",",
                     state->registers[i]);
        if (n < 0 || (size_t)n >= sizeof(temp) - offset) {
            return ALRIOS_CRASH_ERR_CORRUPT;
        }
        offset += (size_t)n;
    }

    n = snprintf(temp + offset, sizeof(temp) - offset, "],\n  \"frames\": [");
    if (n < 0 || (size_t)n >= sizeof(temp) - offset) {
        return ALRIOS_CRASH_ERR_CORRUPT;
    }
    offset += (size_t)n;

    for (size_t i = 0; i < state->frame_count && i < ALRIOS_CRASH_MAX_FRAMES; ++i) {
        n = snprintf(temp + offset, sizeof(temp) - offset,
                     "%s\"0x%" PRIx64 "\"",
                     (i == 0) ? "" : ",",
                     state->stack_frames[i]);
        if (n < 0 || (size_t)n >= sizeof(temp) - offset) {
            return ALRIOS_CRASH_ERR_CORRUPT;
        }
        offset += (size_t)n;
    }

    n = snprintf(temp + offset, sizeof(temp) - offset,
                 "],\n  \"memory_sample_address\": \"0x%" PRIx64 "\",\n"
                 "  \"memory_sample_hex\": \"",
                 state->memory_sample_address);
    if (n < 0 || (size_t)n >= sizeof(temp) - offset) {
        return ALRIOS_CRASH_ERR_CORRUPT;
    }
    offset += (size_t)n;

    for (size_t i = 0; i < state->memory_sample_len && i < sizeof(state->memory_sample); ++i) {
        n = snprintf(temp + offset, sizeof(temp) - offset, "%02x", state->memory_sample[i]);
        if (n < 0 || (size_t)n >= sizeof(temp) - offset) {
            return ALRIOS_CRASH_ERR_CORRUPT;
        }
        offset += (size_t)n;
    }

    n = snprintf(temp + offset, sizeof(temp) - offset, "\"\n}\n");
    if (n < 0 || (size_t)n >= sizeof(temp) - offset) {
        return ALRIOS_CRASH_ERR_CORRUPT;
    }
    offset += (size_t)n;

    if (offset > buf_capacity) {
        return ALRIOS_CRASH_ERR_IO;
    }

    memcpy(out_buf, temp, offset);
    if (out_len) {
        *out_len = offset;
    }
    return ALRIOS_CRASH_OK;
}

int alrios_crash_deserialize_state(const uint8_t *in_buf,
                                   size_t in_len,
                                   alrios_crash_state_t *out_state) {
    if (!in_buf || in_len == 0 || !out_state) {
        return ALRIOS_CRASH_ERR_INVALID_ARGUMENT;
    }

    memset(out_state, 0, sizeof(*out_state));

    /* Simple parser for our known JSON structure */
    char *text = malloc(in_len + 1U);
    if (!text) {
        return ALRIOS_CRASH_ERR_IO;
    }
    memcpy(text, in_buf, in_len);
    text[in_len] = '\0';

    char *p = strstr(text, "\"app_id\": \"");
    if (p) {
        p += 11;
        char *end = strchr(p, '"');
        if (end) {
            size_t l = (size_t)(end - p);
            if (l >= sizeof(out_state->app_id)) {
                l = sizeof(out_state->app_id) - 1;
            }
            memcpy(out_state->app_id, p, l);
            out_state->app_id[l] = '\0';
        }
    }

    p = strstr(text, "\"signal\": ");
    if (p) {
        out_state->signal_number = atoi(p + 10);
    }

    p = strstr(text, "\"error_code\": ");
    if (p) {
        out_state->error_code = atoi(p + 14);
    }

    p = strstr(text, "\"pid\": ");
    if (p) {
        out_state->faulting_pid = atoi(p + 7);
    }

    p = strstr(text, "\"fault_address\": \"0x");
    if (p) {
        out_state->fault_address = strtoull(p + 20, NULL, 16);
    }

    p = strstr(text, "\"ip\": \"0x");
    if (p) {
        out_state->instruction_pointer = strtoull(p + 9, NULL, 16);
    }

    p = strstr(text, "\"sp\": \"0x");
    if (p) {
        out_state->stack_pointer = strtoull(p + 9, NULL, 16);
    }

    p = strstr(text, "\"registers\": [");
    if (p) {
        p += 14;
        while (*p && *p != ']' && out_state->register_count < ALRIOS_CRASH_MAX_REGISTERS) {
            char *quote = strstr(p, "\"0x");
            if (!quote || quote > strchr(p, ']')) break;
            out_state->registers[out_state->register_count++] = strtoull(quote + 3, &p, 16);
        }
    }

    p = strstr(text, "\"frames\": [");
    if (p) {
        p += 11;
        while (*p && *p != ']' && out_state->frame_count < ALRIOS_CRASH_MAX_FRAMES) {
            char *quote = strstr(p, "\"0x");
            if (!quote || quote > strchr(p, ']')) break;
            out_state->stack_frames[out_state->frame_count++] = strtoull(quote + 3, &p, 16);
        }
    }

    p = strstr(text, "\"memory_sample_address\": \"0x");
    if (p) {
        out_state->memory_sample_address = strtoull(p + 28, NULL, 16);
    }

    p = strstr(text, "\"memory_sample_hex\": \"");
    if (p) {
        p += 22;
        char *end = strchr(p, '"');
        if (end) {
            size_t hex_len = (size_t)(end - p);
            size_t byte_len = hex_len / 2;
            if (byte_len > sizeof(out_state->memory_sample)) {
                byte_len = sizeof(out_state->memory_sample);
            }
            for (size_t i = 0; i < byte_len; ++i) {
                unsigned int val = 0;
                if (sscanf(p + (i * 2), "%02x", &val) == 1) {
                    out_state->memory_sample[i] = (uint8_t)val;
                }
            }
            out_state->memory_sample_len = byte_len;
        }
    }

    free(text);
    return ALRIOS_CRASH_OK;
}

int alrios_crash_report_encrypt(const alrios_crash_state_t *state,
                                const uint8_t key[ALRIOS_CRASH_KEY_LEN],
                                uint8_t **out_container,
                                size_t *out_container_len) {
    if (!state || !key || !out_container || !out_container_len) {
        return ALRIOS_CRASH_ERR_INVALID_ARGUMENT;
    }

    /* First, make a copy and scrub any sensitive memory */
    alrios_crash_state_t sanitized = *state;
    int scrub_res = alrios_crash_scrub_state(&sanitized);
    if (scrub_res != ALRIOS_CRASH_OK) {
        return scrub_res;
    }

    /* Serialize to buffer */
    uint8_t plaintext[8192];
    size_t plaintext_len = 0;
    int ser_res = alrios_crash_serialize_state(&sanitized, plaintext, sizeof(plaintext), &plaintext_len);
    if (ser_res != ALRIOS_CRASH_OK) {
        return ser_res;
    }

    size_t header_size = sizeof(alrios_crash_header_t);
    size_t total_size = header_size + plaintext_len;
    uint8_t *container = malloc(total_size);
    if (!container) {
        alrios_explicit_zeroize(plaintext, sizeof(plaintext));
        return ALRIOS_CRASH_ERR_IO;
    }

    alrios_crash_header_t *header = (alrios_crash_header_t *)container;
    memcpy(header->magic, ALRIOS_CRASH_MAGIC, ALRIOS_CRASH_MAGIC_LEN);
    header->version = (uint8_t)ALRIOS_CRASH_CONTAINER_VERSION;
    header->reserved = 0;
    header->payload_len = (uint32_t)plaintext_len;

    if (get_secure_random(header->iv, sizeof(header->iv)) != 0) {
        free(container);
        alrios_explicit_zeroize(plaintext, sizeof(plaintext));
        return ALRIOS_CRASH_ERR_CRYPTO;
    }

    /* AAD is the header up to IV: magic(9) + version(1) + reserved(2) + payload_len(4) = 16 bytes */
    size_t aad_len = offsetof(alrios_crash_header_t, iv);
    const uint8_t *aad = (const uint8_t *)header;

    uint8_t *ciphertext = container + header_size;
    int enc_res = alrios_aes256_gcm_encrypt(plaintext, plaintext_len,
                                           aad, aad_len,
                                           key, header->iv,
                                           ciphertext, header->tag);
    alrios_explicit_zeroize(plaintext, sizeof(plaintext));

    if (enc_res != ALRIOS_CRYPTO_OK) {
        free(container);
        return ALRIOS_CRASH_ERR_CRYPTO;
    }

    *out_container = container;
    *out_container_len = total_size;
    return ALRIOS_CRASH_OK;
}

int alrios_crash_report_decrypt(const uint8_t *container,
                                size_t container_len,
                                const uint8_t key[ALRIOS_CRASH_KEY_LEN],
                                alrios_crash_state_t *out_state) {
    if (!container || !key || !out_state) {
        return ALRIOS_CRASH_ERR_INVALID_ARGUMENT;
    }

    size_t header_size = sizeof(alrios_crash_header_t);
    if (container_len < header_size) {
        return ALRIOS_CRASH_ERR_CORRUPT;
    }

    const alrios_crash_header_t *header = (const alrios_crash_header_t *)container;
    if (memcmp(header->magic, ALRIOS_CRASH_MAGIC, ALRIOS_CRASH_MAGIC_LEN) != 0) {
        return ALRIOS_CRASH_ERR_BAD_MAGIC;
    }

    if (header->version != ALRIOS_CRASH_CONTAINER_VERSION) {
        return ALRIOS_CRASH_ERR_CORRUPT;
    }

    size_t payload_len = header->payload_len;
    if (container_len != header_size + payload_len) {
        return ALRIOS_CRASH_ERR_CORRUPT;
    }

    uint8_t *plaintext = malloc(payload_len ? payload_len : 1);
    if (!plaintext) {
        return ALRIOS_CRASH_ERR_IO;
    }

    size_t aad_len = offsetof(alrios_crash_header_t, iv);
    const uint8_t *aad = (const uint8_t *)header;
    const uint8_t *ciphertext = container + header_size;

    int dec_res = alrios_aes256_gcm_decrypt(ciphertext, payload_len,
                                           aad, aad_len,
                                           header->tag,
                                           key, header->iv,
                                           plaintext);
    if (dec_res != ALRIOS_CRYPTO_OK) {
        free(plaintext);
        return ALRIOS_CRASH_ERR_AUTH_FAIL;
    }

    int parse_res = alrios_crash_deserialize_state(plaintext, payload_len, out_state);
    alrios_explicit_zeroize(plaintext, payload_len);
    free(plaintext);

    return parse_res;
}
