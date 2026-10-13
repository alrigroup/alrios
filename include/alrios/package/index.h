/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_PACKAGE_INDEX_H
#define ALRIOS_PACKAGE_INDEX_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALRIOS_PACKAGE_INDEX_MAGIC "ARAPPINDEXv1\0\0\0\0"
#define ALRIOS_PACKAGE_INDEX_MAGIC_SIZE 16U
#define ALRIOS_PACKAGE_INDEX_VERSION 1U
#define ALRIOS_PACKAGE_INDEX_HEADER_SIZE 144U
#define ALRIOS_PACKAGE_INDEX_ENTRY_PREFIX_SIZE 52U
#define ALRIOS_PACKAGE_INDEX_SHA256_SIZE 32U
#define ALRIOS_PACKAGE_INDEX_ED25519_PUBLIC_KEY_SIZE 32U
#define ALRIOS_PACKAGE_INDEX_ED25519_PRIVATE_KEY_SIZE 32U
#define ALRIOS_PACKAGE_INDEX_ED25519_SIGNATURE_SIZE 64U
#define ALRIOS_PACKAGE_INDEX_MAX_PATH_SIZE 1024U
#define ALRIOS_PACKAGE_INDEX_MAX_FILES 100000U
#define ALRIOS_PACKAGE_INDEX_MAX_WIRE_SIZE (64U * 1024U * 1024U)

#define ALRIOS_PACKAGE_INDEX_FLAG_ED25519 (1U << 0)
#define ALRIOS_PACKAGE_INDEX_FLAGS_MASK ALRIOS_PACKAGE_INDEX_FLAG_ED25519

#define ALRIOS_PACKAGE_INDEX_OK 0
#define ALRIOS_PACKAGE_INDEX_ERR_ARGUMENT    -4001
#define ALRIOS_PACKAGE_INDEX_ERR_IO          -4002
#define ALRIOS_PACKAGE_INDEX_ERR_MEMORY      -4003
#define ALRIOS_PACKAGE_INDEX_ERR_LIMIT       -4004
#define ALRIOS_PACKAGE_INDEX_ERR_PATH        -4005
#define ALRIOS_PACKAGE_INDEX_ERR_MUTABLE     -4006
#define ALRIOS_PACKAGE_INDEX_ERR_SYMLINK     -4007
#define ALRIOS_PACKAGE_INDEX_ERR_FILE_TYPE   -4008
#define ALRIOS_PACKAGE_INDEX_ERR_CRYPTO      -4009
#define ALRIOS_PACKAGE_INDEX_ERR_FORMAT      -4010
#define ALRIOS_PACKAGE_INDEX_ERR_SIGNATURE   -4011
#define ALRIOS_PACKAGE_INDEX_ERR_HASH        -4012
#define ALRIOS_PACKAGE_INDEX_ERR_UNSUPPORTED -4013
#define ALRIOS_PACKAGE_INDEX_ERR_CHANGED     -4014

typedef struct alrios_package_index_entry {
    char *path;
    uint64_t file_size;
    uint32_t unix_mode;
    uint8_t sha256[ALRIOS_PACKAGE_INDEX_SHA256_SIZE];
} alrios_package_index_entry_t;

typedef struct alrios_package_index {
    uint32_t file_count;
    uint64_t total_uncompressed_size;
    alrios_package_index_entry_t *entries;
} alrios_package_index_t;

/*
 * Wire integers are unsigned big-endian values. Entries are strictly ordered
 * by normalized UTF-8 path. The Ed25519 signature covers bytes [0, 80) of the
 * header followed by the complete canonical entry stream.
 */
int alrios_package_index_build_directory(
    const char *root_directory,
    const uint8_t private_key[ALRIOS_PACKAGE_INDEX_ED25519_PRIVATE_KEY_SIZE],
    uint8_t **out_wire,
    size_t *out_wire_size
);

int alrios_package_index_write_directory(
    const char *root_directory,
    const char *output_path,
    const uint8_t private_key[ALRIOS_PACKAGE_INDEX_ED25519_PRIVATE_KEY_SIZE]
);

int alrios_package_index_load_private_key_file(
    const char *key_path,
    uint8_t out_private_key[ALRIOS_PACKAGE_INDEX_ED25519_PRIVATE_KEY_SIZE]
);

int alrios_package_index_load_public_key_file(
    const char *key_path,
    uint8_t out_public_key[ALRIOS_PACKAGE_INDEX_ED25519_PUBLIC_KEY_SIZE]
);

int alrios_package_index_public_key_from_private(
    const uint8_t private_key[ALRIOS_PACKAGE_INDEX_ED25519_PRIVATE_KEY_SIZE],
    uint8_t out_public_key[ALRIOS_PACKAGE_INDEX_ED25519_PUBLIC_KEY_SIZE]
);

const char *alrios_package_index_error_string(int code);

/* expected_public_key is mandatory; self-authenticating keys are not accepted. */
int alrios_package_index_parse_and_verify(
    const uint8_t *wire,
    size_t wire_size,
    const uint8_t expected_public_key[ALRIOS_PACKAGE_INDEX_ED25519_PUBLIC_KEY_SIZE],
    alrios_package_index_t *out_index
);

/* Verifies size, mode and SHA-256 without following any path component symlink. */
int alrios_package_index_verify_directory(
    const alrios_package_index_t *index,
    const char *root_directory
);

void alrios_package_index_free(alrios_package_index_t *index);

int alrios_package_index_path_is_mutable(const char *normalized_path);

#ifdef __cplusplus
}
#endif

#endif /* ALRIOS_PACKAGE_INDEX_H */
