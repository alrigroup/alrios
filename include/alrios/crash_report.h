/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_CRASH_REPORT_H
#define ALRIOS_CRASH_REPORT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALRIOS_CRASH_OK                     0
#define ALRIOS_CRASH_ERR_INVALID_ARGUMENT  -9101
#define ALRIOS_CRASH_ERR_CRYPTO            -9102
#define ALRIOS_CRASH_ERR_BAD_MAGIC         -9103
#define ALRIOS_CRASH_ERR_CORRUPT           -9104
#define ALRIOS_CRASH_ERR_IO                -9105
#define ALRIOS_CRASH_ERR_AUTH_FAIL         -9106

#define ALRIOS_CRASH_MAGIC                 "ALRCRSH01"
#define ALRIOS_CRASH_MAGIC_LEN             9U
#define ALRIOS_CRASH_CONTAINER_VERSION     1U

#define ALRIOS_CRASH_MAX_FRAMES            64U
#define ALRIOS_CRASH_MAX_REGISTERS         32U
#define ALRIOS_CRASH_APP_ID_MAX            64U

#define ALRIOS_CRASH_KEY_LEN               32U
#define ALRIOS_CRASH_IV_LEN                12U
#define ALRIOS_CRASH_TAG_LEN               16U

/* Process crash raw state */
typedef struct alrios_crash_state {
    int signal_number;
    int error_code;
    int faulting_pid;
    uint64_t fault_address;
    uint64_t instruction_pointer;
    uint64_t stack_pointer;
    char app_id[ALRIOS_CRASH_APP_ID_MAX];
    uint64_t registers[ALRIOS_CRASH_MAX_REGISTERS];
    size_t register_count;
    uint64_t stack_frames[ALRIOS_CRASH_MAX_FRAMES];
    size_t frame_count;
    uint8_t memory_sample[256];
    size_t memory_sample_len;
    uint64_t memory_sample_address;
} alrios_crash_state_t;

/* Header prefix for encrypted crash container */
typedef struct alrios_crash_header {
    uint8_t magic[ALRIOS_CRASH_MAGIC_LEN]; /* "ALRCRSH01" */
    uint8_t version;                       /* 1 */
    uint16_t reserved;                     /* 0 */
    uint32_t payload_len;                  /* Length of encrypted ciphertext */
    uint8_t iv[ALRIOS_CRASH_IV_LEN];       /* AES-GCM IV (12B) */
    uint8_t tag[ALRIOS_CRASH_TAG_LEN];     /* AES-GCM Auth Tag (16B) */
} __attribute__((packed)) alrios_crash_header_t;

/*
 * Scrubs process crash state against registered sensitive memory ranges.
 * If any memory sample or pointer falls within a sensitive range, it is scrubbed (zeroized).
 */
int alrios_crash_scrub_state(alrios_crash_state_t *state);

/*
 * Serializes crash state into a plaintext JSON/binary buffer.
 * Returns bytes written or negative error code.
 */
int alrios_crash_serialize_state(const alrios_crash_state_t *state,
                                 uint8_t *out_buf,
                                 size_t buf_capacity,
                                 size_t *out_len);

/*
 * Deserializes plaintext buffer back into crash state.
 */
int alrios_crash_deserialize_state(const uint8_t *in_buf,
                                   size_t in_len,
                                   alrios_crash_state_t *out_state);

/*
 * Packs and encrypts crash state into authenticated container format.
 * Container wire format:
 *   [Header: ALRCRSH01 (9B) | Version: 1B | Reserved: 2B | PayloadLen: 4B | IV: 12B | Tag: 16B]
 *   [Ciphertext: PayloadLen bytes]
 * AAD covers header bytes up to IV (magic + version + reserved + payload_len).
 */
int alrios_crash_report_encrypt(const alrios_crash_state_t *state,
                                const uint8_t key[ALRIOS_CRASH_KEY_LEN],
                                uint8_t **out_container,
                                size_t *out_container_len);

/*
 * Decrypts authenticated crash container and verifies tag using key.
 * Fails closed on any modification, invalid key, or invalid tag.
 */
int alrios_crash_report_decrypt(const uint8_t *container,
                                size_t container_len,
                                const uint8_t key[ALRIOS_CRASH_KEY_LEN],
                                alrios_crash_state_t *out_state);

#ifdef __cplusplus
}
#endif

#endif /* ALRIOS_CRASH_REPORT_H */
