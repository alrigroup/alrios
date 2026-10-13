/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_VAULT_STORE_H
#define ALRIOS_VAULT_STORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALRIOS_VAULT_STORE_VERSION 1U
#define ALRIOS_VAULT_SCHEMA_VERSION 1U
#define ALRIOS_VAULT_MAX_ENTRIES 128U
#define ALRIOS_VAULT_MAX_KEY_SIZE 64U
#define ALRIOS_VAULT_MAX_VALUE_SIZE 4096U
#define ALRIOS_VAULT_MAX_PAYLOAD_SIZE (1024U * 1024U)
#define ALRIOS_VAULT_MIN_PASSPHRASE_SIZE 12U

#define ALRIOS_VAULT_SCHEMA_REQUIRED (1U << 0)
#define ALRIOS_VAULT_SCHEMA_FLAGS_MASK ALRIOS_VAULT_SCHEMA_REQUIRED

#define ALRIOS_VAULT_OK 0
#define ALRIOS_VAULT_ERR_ARGUMENT       -6001
#define ALRIOS_VAULT_ERR_SCHEMA         -6002
#define ALRIOS_VAULT_ERR_CONFIG         -6003
#define ALRIOS_VAULT_ERR_LIMIT          -6004
#define ALRIOS_VAULT_ERR_MEMORY         -6005
#define ALRIOS_VAULT_ERR_CRYPTO         -6006
#define ALRIOS_VAULT_ERR_AUTH           -6007
#define ALRIOS_VAULT_ERR_FORMAT         -6008
#define ALRIOS_VAULT_ERR_IO             -6009
#define ALRIOS_VAULT_ERR_PERMISSIONS    -6010
#define ALRIOS_VAULT_ERR_UNSUPPORTED    -6011
#define ALRIOS_VAULT_ERR_NOT_AUTHORIZED -6012

/* Values are byte strings and may contain NUL bytes. Keys are never secret. */
typedef struct alrios_vault_secret {
    const char *key;
    const uint8_t *value;
    size_t value_len;
} alrios_vault_secret_t;

typedef struct alrios_vault_schema_entry {
    const char *key;
    size_t max_value_len;
    uint32_t flags;
} alrios_vault_schema_entry_t;

typedef struct alrios_vault_schema {
    uint16_t version;
    size_t count;
    const alrios_vault_schema_entry_t *entries;
} alrios_vault_schema_t;

/* Documents returned by load/read APIs own all pointed-to memory. */
typedef struct alrios_vault_document {
    size_t count;
    alrios_vault_secret_t *entries;
} alrios_vault_document_t;

int alrios_vault_schema_validate(const alrios_vault_schema_t *schema);

int alrios_vault_config_validate(
    const alrios_vault_schema_t *schema,
    const alrios_vault_secret_t *secrets,
    size_t secret_count
);

/*
 * Writes an AES-256-GCM envelope using scrypt-derived key material. The final
 * file is atomically replaced, mode 0600, and no plaintext staging file exists.
 */
int alrios_vault_store_write(
    const char *path,
    const uint8_t *passphrase,
    size_t passphrase_len,
    const alrios_vault_schema_t *schema,
    const alrios_vault_secret_t *secrets,
    size_t secret_count
);

/* Refuses symlinks, non-regular files, foreign ownership and group/world bits. */
int alrios_vault_store_load(
    const char *path,
    const uint8_t *passphrase,
    size_t passphrase_len,
    const alrios_vault_schema_t *schema,
    alrios_vault_document_t *out_document
);

const alrios_vault_secret_t *alrios_vault_document_find(
    const alrios_vault_document_t *document,
    const char *key
);

void alrios_vault_document_clear(alrios_vault_document_t *document);

#ifdef __cplusplus
}
#endif

#endif /* ALRIOS_VAULT_STORE_H */
