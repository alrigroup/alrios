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

#include "alrios/vault/store.h"
#include "alrios/crypto_verify.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if defined(__linux__)
#include <sys/syscall.h>
#include <unistd.h>
#endif

#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif
#ifndef MFD_ALLOW_SEALING
#define MFD_ALLOW_SEALING 0x0002U
#endif
#ifndef F_ADD_SEALS
#define F_ADD_SEALS 1033
#endif
#ifndef F_GET_SEALS
#define F_GET_SEALS 1034
#endif
#ifndef F_SEAL_SEAL
#define F_SEAL_SEAL 0x0001
#endif
#ifndef F_SEAL_SHRINK
#define F_SEAL_SHRINK 0x0002
#endif
#ifndef F_SEAL_GROW
#define F_SEAL_GROW 0x0004
#endif
#ifndef F_SEAL_WRITE
#define F_SEAL_WRITE 0x0008
#endif

#define VAULT_FILE_MAGIC "ALRVLT01"
#define VAULT_FILE_MAGIC_LEN 8U
#define VAULT_FILE_HEADER_LEN \
    (VAULT_FILE_MAGIC_LEN + 8U + ALRIOS_VAULT_SALT_LEN + ALRIOS_VAULT_IV_LEN + ALRIOS_VAULT_TAG_LEN)
#define VAULT_MAX_BLOB_LEN (16U * 1024U * 1024U)
#define VAULT_MAX_APP_ID_LEN 255U
#define VAULT_MAX_KEY_LEN 255U
#define VAULT_MEMFD_SEALS \
    (F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE)

typedef struct vault_record {
    char *app_id;
    char *key;
    char *value;
} vault_record_t;

typedef struct vault_records {
    vault_record_t *records;
    size_t count;
    size_t capacity;
} vault_records_t;

static int checked_size_to_int(size_t value, int *out_value) {
    if (!out_value || value > (size_t)INT_MAX) {
        return ALRIOS_VAULT_ERR_ARGUMENT;
    }
    *out_value = (int)value;
    return ALRIOS_VAULT_OK;
}

static int is_valid_app_id(const char *app_id) {
    size_t index;
    size_t length;

    if (!app_id) {
        return 0;
    }
    length = strlen(app_id);
    if (length == 0U || length > VAULT_MAX_APP_ID_LEN) {
        return 0;
    }
    for (index = 0U; index < length; ++index) {
        unsigned char c = (unsigned char)app_id[index];
        if (!(isalnum(c) || c == '.' || c == '-' || c == '_')) {
            return 0;
        }
    }
    return 1;
}

static int is_valid_secret_key(const char *key) {
    size_t index;
    size_t length;

    if (!key) {
        return 0;
    }
    length = strlen(key);
    if (length == 0U || length > VAULT_MAX_KEY_LEN) {
        return 0;
    }
    if (!(isalpha((unsigned char)key[0]) || key[0] == '_')) {
        return 0;
    }
    for (index = 1U; index < length; ++index) {
        unsigned char c = (unsigned char)key[index];
        if (!(isalnum(c) || c == '_')) {
            return 0;
        }
    }
    return 1;
}

static void record_clear(vault_record_t *record) {
    if (!record) {
        return;
    }
    if (record->app_id) {
        alrios_explicit_zeroize(record->app_id, strlen(record->app_id));
        free(record->app_id);
    }
    if (record->key) {
        alrios_explicit_zeroize(record->key, strlen(record->key));
        free(record->key);
    }
    if (record->value) {
        alrios_explicit_zeroize(record->value, strlen(record->value));
        free(record->value);
    }
    alrios_explicit_zeroize(record, sizeof(*record));
}

static void records_clear(vault_records_t *records) {
    size_t index;

    if (!records) {
        return;
    }
    for (index = 0U; index < records->count; ++index) {
        record_clear(&records->records[index]);
    }
    if (records->records) {
        alrios_explicit_zeroize(records->records,
                                records->capacity * sizeof(*records->records));
        free(records->records);
    }
    alrios_explicit_zeroize(records, sizeof(*records));
}

static int records_add(vault_records_t *records,
                       const char *app_id,
                       const char *key,
                       const char *value) {
    vault_record_t *new_records;
    vault_record_t *record;
    size_t new_capacity;

    if (!records || !key || !value || !is_valid_secret_key(key) ||
        (app_id && !is_valid_app_id(app_id))) {
        return ALRIOS_VAULT_ERR_SCHEMA;
    }
    if (records->count == records->capacity) {
        new_capacity = records->capacity == 0U ? 8U : records->capacity * 2U;
        if (new_capacity < records->capacity ||
            new_capacity > SIZE_MAX / sizeof(*new_records)) {
            return ALRIOS_VAULT_ERR_IO;
        }
        new_records = realloc(records->records,
                              new_capacity * sizeof(*new_records));
        if (!new_records) {
            return ALRIOS_VAULT_ERR_IO;
        }
        memset(new_records + records->capacity, 0,
               (new_capacity - records->capacity) * sizeof(*new_records));
        records->records = new_records;
        records->capacity = new_capacity;
    }

    record = &records->records[records->count];
    if (app_id) {
        record->app_id = strdup(app_id);
        if (!record->app_id) {
            return ALRIOS_VAULT_ERR_IO;
        }
    }
    record->key = strdup(key);
    record->value = strdup(value);
    if (!record->key || !record->value) {
        record_clear(record);
        return ALRIOS_VAULT_ERR_IO;
    }
    records->count++;
    return ALRIOS_VAULT_OK;
}

static char *trim_ascii(char *text) {
    char *end;

    while (*text != '\0' && isspace((unsigned char)*text)) {
        ++text;
    }
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        --end;
    }
    *end = '\0';
    return text;
}

static int parse_line_records(char *plaintext, vault_records_t *out_records) {
    char *save = NULL;
    char *line;

    for (line = strtok_r(plaintext, "\r\n", &save);
         line != NULL;
         line = strtok_r(NULL, "\r\n", &save)) {
        char *entry = trim_ascii(line);
        char *equals;
        char *separator;
        char *app_id = NULL;
        char *key;
        char *value;
        int result;

        if (*entry == '\0' || *entry == '#') {
            continue;
        }
        equals = strchr(entry, '=');
        if (!equals) {
            return ALRIOS_VAULT_ERR_FORMAT;
        }
        *equals = '\0';
        value = equals + 1;
        key = trim_ascii(entry);
        separator = strchr(key, ':');
        if (separator) {
            *separator = '\0';
            app_id = trim_ascii(key);
            key = trim_ascii(separator + 1);
        }
        result = records_add(out_records, app_id, key, value);
        if (result != ALRIOS_VAULT_OK) {
            return result;
        }
    }
    return out_records->count > 0U ? ALRIOS_VAULT_OK : ALRIOS_VAULT_ERR_SCHEMA;
}

static const char *json_skip_ws(const char *cursor) {
    while (*cursor != '\0' && isspace((unsigned char)*cursor)) {
        ++cursor;
    }
    return cursor;
}

static int json_hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

static int json_append_utf8(char *out, size_t capacity, size_t *length,
                            uint32_t codepoint) {
    if (!out || !length || codepoint == 0U || codepoint > 0x10ffffU ||
        (codepoint >= 0xd800U && codepoint <= 0xdfffU)) {
        return ALRIOS_VAULT_ERR_FORMAT;
    }
    if (codepoint <= 0x7fU) {
        if (*length + 1U >= capacity) {
            return ALRIOS_VAULT_ERR_SCHEMA;
        }
        out[(*length)++] = (char)codepoint;
    } else if (codepoint <= 0x7ffU) {
        if (*length + 2U >= capacity) {
            return ALRIOS_VAULT_ERR_SCHEMA;
        }
        out[(*length)++] = (char)(0xc0U | (codepoint >> 6U));
        out[(*length)++] = (char)(0x80U | (codepoint & 0x3fU));
    } else if (codepoint <= 0xffffU) {
        if (*length + 3U >= capacity) {
            return ALRIOS_VAULT_ERR_SCHEMA;
        }
        out[(*length)++] = (char)(0xe0U | (codepoint >> 12U));
        out[(*length)++] = (char)(0x80U | ((codepoint >> 6U) & 0x3fU));
        out[(*length)++] = (char)(0x80U | (codepoint & 0x3fU));
    } else {
        if (*length + 4U >= capacity) {
            return ALRIOS_VAULT_ERR_SCHEMA;
        }
        out[(*length)++] = (char)(0xf0U | (codepoint >> 18U));
        out[(*length)++] = (char)(0x80U | ((codepoint >> 12U) & 0x3fU));
        out[(*length)++] = (char)(0x80U | ((codepoint >> 6U) & 0x3fU));
        out[(*length)++] = (char)(0x80U | (codepoint & 0x3fU));
    }
    return ALRIOS_VAULT_OK;
}

static int json_parse_string(const char **cursor,
                             char **out_string,
                             size_t max_length) {
    const char *input;
    char *output;
    size_t length = 0U;
    int result = ALRIOS_VAULT_OK;

    if (!cursor || !*cursor || !out_string || **cursor != '"') {
        return ALRIOS_VAULT_ERR_FORMAT;
    }
    input = *cursor + 1;
    output = calloc(max_length + 1U, 1U);
    if (!output) {
        return ALRIOS_VAULT_ERR_IO;
    }

    while (*input != '\0' && *input != '"') {
        unsigned char c = (unsigned char)*input++;
        if (c < 0x20U) {
            result = ALRIOS_VAULT_ERR_FORMAT;
            break;
        }
        if (c == '\\') {
            uint32_t codepoint;
            int h0;
            int h1;
            int h2;
            int h3;

            c = (unsigned char)*input++;
            switch (c) {
                case '"':
                case '\\':
                case '/':
                    break;
                case 'b':
                    c = '\b';
                    break;
                case 'f':
                    c = '\f';
                    break;
                case 'n':
                    c = '\n';
                    break;
                case 'r':
                    c = '\r';
                    break;
                case 't':
                    c = '\t';
                    break;
                case 'u':
                    h0 = json_hex_value(input[0]);
                    h1 = json_hex_value(input[1]);
                    h2 = json_hex_value(input[2]);
                    h3 = json_hex_value(input[3]);
                    if (h0 < 0 || h1 < 0 || h2 < 0 || h3 < 0) {
                        result = ALRIOS_VAULT_ERR_FORMAT;
                        break;
                    }
                    codepoint = ((uint32_t)h0 << 12U) |
                                ((uint32_t)h1 << 8U) |
                                ((uint32_t)h2 << 4U) | (uint32_t)h3;
                    input += 4;
                    result = json_append_utf8(output, max_length + 1U,
                                              &length, codepoint);
                    continue;
                default:
                    result = ALRIOS_VAULT_ERR_FORMAT;
                    break;
            }
            if (result != ALRIOS_VAULT_OK) {
                break;
            }
        }
        if (length >= max_length) {
            result = ALRIOS_VAULT_ERR_SCHEMA;
            break;
        }
        output[length++] = (char)c;
    }

    if (result == ALRIOS_VAULT_OK && *input == '"') {
        output[length] = '\0';
        *cursor = input + 1;
        *out_string = output;
        return ALRIOS_VAULT_OK;
    }

    alrios_explicit_zeroize(output, max_length + 1U);
    free(output);
    return result == ALRIOS_VAULT_OK ? ALRIOS_VAULT_ERR_FORMAT : result;
}

static int json_expect(const char **cursor, char expected) {
    const char *input;

    if (!cursor || !*cursor) {
        return ALRIOS_VAULT_ERR_FORMAT;
    }
    input = json_skip_ws(*cursor);
    if (*input != expected) {
        return ALRIOS_VAULT_ERR_FORMAT;
    }
    *cursor = input + 1;
    return ALRIOS_VAULT_OK;
}

static int parse_json_secret_object(const char **cursor,
                                    const char *app_id,
                                    vault_records_t *records) {
    int result;

    result = json_expect(cursor, '{');
    if (result != ALRIOS_VAULT_OK) {
        return result;
    }
    *cursor = json_skip_ws(*cursor);
    if (**cursor == '}') {
        ++(*cursor);
        return ALRIOS_VAULT_ERR_SCHEMA;
    }

    for (;;) {
        char *key = NULL;
        char *value = NULL;

        *cursor = json_skip_ws(*cursor);
        result = json_parse_string(cursor, &key, VAULT_MAX_KEY_LEN);
        if (result == ALRIOS_VAULT_OK) {
            result = json_expect(cursor, ':');
        }
        if (result == ALRIOS_VAULT_OK) {
            *cursor = json_skip_ws(*cursor);
            result = json_parse_string(cursor, &value, VAULT_MAX_BLOB_LEN);
        }
        if (result == ALRIOS_VAULT_OK) {
            result = records_add(records, app_id, key, value);
        }
        if (key) {
            alrios_explicit_zeroize(key, strlen(key));
            free(key);
        }
        if (value) {
            alrios_explicit_zeroize(value, strlen(value));
            free(value);
        }
        if (result != ALRIOS_VAULT_OK) {
            return result;
        }

        *cursor = json_skip_ws(*cursor);
        if (**cursor == '}') {
            ++(*cursor);
            return ALRIOS_VAULT_OK;
        }
        result = json_expect(cursor, ',');
        if (result != ALRIOS_VAULT_OK) {
            return result;
        }
    }
}

static int parse_json_records(const char *plaintext,
                              vault_records_t *out_records) {
    const char *cursor = plaintext;
    int result;

    result = json_expect(&cursor, '{');
    if (result != ALRIOS_VAULT_OK) {
        return result;
    }
    cursor = json_skip_ws(cursor);
    if (*cursor == '}') {
        return ALRIOS_VAULT_ERR_SCHEMA;
    }

    for (;;) {
        char *outer_key = NULL;

        cursor = json_skip_ws(cursor);
        result = json_parse_string(&cursor, &outer_key, VAULT_MAX_APP_ID_LEN);
        if (result == ALRIOS_VAULT_OK) {
            result = json_expect(&cursor, ':');
        }
        if (result == ALRIOS_VAULT_OK) {
            cursor = json_skip_ws(cursor);
            if (*cursor == '{') {
                if (!is_valid_app_id(outer_key)) {
                    result = ALRIOS_VAULT_ERR_SCHEMA;
                } else {
                    result = parse_json_secret_object(&cursor, outer_key,
                                                      out_records);
                }
            } else if (*cursor == '"') {
                char *value = NULL;
                result = json_parse_string(&cursor, &value,
                                           VAULT_MAX_BLOB_LEN);
                if (result == ALRIOS_VAULT_OK) {
                    result = records_add(out_records, NULL, outer_key, value);
                }
                if (value) {
                    alrios_explicit_zeroize(value, strlen(value));
                    free(value);
                }
            } else {
                result = ALRIOS_VAULT_ERR_FORMAT;
            }
        }
        if (outer_key) {
            alrios_explicit_zeroize(outer_key, strlen(outer_key));
            free(outer_key);
        }
        if (result != ALRIOS_VAULT_OK) {
            return result;
        }

        cursor = json_skip_ws(cursor);
        if (*cursor == '}') {
            ++cursor;
            cursor = json_skip_ws(cursor);
            return *cursor == '\0' ? ALRIOS_VAULT_OK : ALRIOS_VAULT_ERR_FORMAT;
        }
        result = json_expect(&cursor, ',');
        if (result != ALRIOS_VAULT_OK) {
            return result;
        }
    }
}

static int parse_records(char *plaintext, vault_records_t *out_records) {
    char *cursor;

    if (!plaintext || !out_records) {
        return ALRIOS_VAULT_ERR_ARGUMENT;
    }
    cursor = trim_ascii(plaintext);
    if (*cursor == '{') {
        return parse_json_records(cursor, out_records);
    }
    return parse_line_records(cursor, out_records);
}

static int is_authorized_key(const char *key,
                             const char *const *authorized_keys,
                             size_t num_authorized) {
    size_t index;

    if (num_authorized == 0U) {
        return 1;
    }
    if (!authorized_keys) {
        return 0;
    }
    for (index = 0U; index < num_authorized; ++index) {
        if (authorized_keys[index] &&
            strcmp(key, authorized_keys[index]) == 0) {
            return 1;
        }
    }
    return 0;
}

static int record_belongs_to_app(const vault_record_t *record,
                                 const char *app_id) {
    if (!app_id) {
        return record->app_id == NULL;
    }
    return record->app_id && strcmp(record->app_id, app_id) == 0;
}

static int decrypt_store(const alrios_vault_store_t *store,
                         const uint8_t *key,
                         size_t key_len,
                         uint8_t **out_plaintext,
                         size_t *out_plaintext_len) {
    EVP_CIPHER_CTX *ctx = NULL;
    uint8_t *plaintext = NULL;
    int input_len;
    int output_len = 0;
    int final_len = 0;
    int result = ALRIOS_VAULT_ERR_CRYPTO;

    if (!store || !store->encrypted_blob || store->blob_len == 0U ||
        store->blob_len > VAULT_MAX_BLOB_LEN || !key ||
        key_len != ALRIOS_VAULT_KEY_LEN || !out_plaintext ||
        !out_plaintext_len ||
        checked_size_to_int(store->blob_len, &input_len) != ALRIOS_VAULT_OK) {
        return ALRIOS_VAULT_ERR_ARGUMENT;
    }
    *out_plaintext = NULL;
    *out_plaintext_len = 0U;

    plaintext = calloc(store->blob_len + 1U, 1U);
    if (!plaintext) {
        return ALRIOS_VAULT_ERR_IO;
    }
    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        goto cleanup;
    }
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN,
                            (int)ALRIOS_VAULT_IV_LEN, NULL) != 1 ||
        EVP_DecryptInit_ex(ctx, NULL, NULL, key, store->iv) != 1 ||
        EVP_DecryptUpdate(ctx, plaintext, &output_len,
                          store->encrypted_blob, input_len) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG,
                            (int)ALRIOS_VAULT_TAG_LEN,
                            (void *)store->tag) != 1 ||
        EVP_DecryptFinal_ex(ctx, plaintext + output_len, &final_len) != 1) {
        goto cleanup;
    }
    if (output_len < 0 || final_len < 0 ||
        (size_t)output_len + (size_t)final_len != store->blob_len) {
        result = ALRIOS_VAULT_ERR_FORMAT;
        goto cleanup;
    }

    plaintext[store->blob_len] = '\0';
    *out_plaintext = plaintext;
    *out_plaintext_len = store->blob_len;
    plaintext = NULL;
    result = ALRIOS_VAULT_OK;

cleanup:
    EVP_CIPHER_CTX_free(ctx);
    if (plaintext) {
        alrios_explicit_zeroize(plaintext, store->blob_len + 1U);
        free(plaintext);
    }
    return result;
}

static int derive_store_key(const alrios_vault_store_t *store,
                            const uint8_t *key_material,
                            size_t key_material_len,
                            uint32_t iterations,
                            uint8_t out_key[32]) {
    if (!store || !key_material || key_material_len == 0U || !out_key) {
        return ALRIOS_VAULT_ERR_ARGUMENT;
    }
    if (key_material_len == ALRIOS_VAULT_KEY_LEN && iterations == 0U) {
        memcpy(out_key, key_material, ALRIOS_VAULT_KEY_LEN);
        return ALRIOS_VAULT_OK;
    }
    return alrios_vault_derive_key_pbkdf2(
        key_material, key_material_len, store->salt,
        iterations == 0U ? ALRIOS_VAULT_DEFAULT_PBKDF2_ITERS : iterations,
        out_key);
}

int alrios_vault_derive_key_pbkdf2(
    const void *passphrase,
    size_t passphrase_len,
    const uint8_t salt[16],
    uint32_t iterations,
    uint8_t out_key[32]
) {
    int password_len;

    if (!passphrase || passphrase_len == 0U || !salt || !out_key ||
        iterations == 0U ||
        checked_size_to_int(passphrase_len, &password_len) != ALRIOS_VAULT_OK ||
        iterations > (uint32_t)INT_MAX) {
        return ALRIOS_VAULT_ERR_ARGUMENT;
    }
    if (PKCS5_PBKDF2_HMAC((const char *)passphrase, password_len,
                          salt, (int)ALRIOS_VAULT_SALT_LEN,
                          (int)iterations, EVP_sha256(),
                          (int)ALRIOS_VAULT_KEY_LEN, out_key) != 1) {
        alrios_explicit_zeroize(out_key, ALRIOS_VAULT_KEY_LEN);
        return ALRIOS_VAULT_ERR_CRYPTO;
    }
    return ALRIOS_VAULT_OK;
}

int alrios_vault_derive_key_hkdf(
    const uint8_t *master_key,
    size_t master_key_len,
    const uint8_t salt[16],
    const char *info,
    uint8_t out_key[32]
) {
    EVP_PKEY_CTX *context = NULL;
    int result = ALRIOS_VAULT_ERR_CRYPTO;
    size_t output_len = ALRIOS_VAULT_KEY_LEN;

    if (!master_key || master_key_len == 0U || !salt || !info ||
        !out_key || master_key_len > (size_t)INT_MAX ||
        strlen(info) > (size_t)INT_MAX) {
        return ALRIOS_VAULT_ERR_ARGUMENT;
    }
    context = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL);
    if (!context) {
        return ALRIOS_VAULT_ERR_CRYPTO;
    }
    if (EVP_PKEY_derive_init(context) <= 0 ||
        EVP_PKEY_CTX_hkdf_mode(context, EVP_PKEY_HKDEF_MODE_EXTRACT_AND_EXPAND) <= 0 ||
        EVP_PKEY_CTX_set_hkdf_md(context, EVP_sha256()) <= 0 ||
        EVP_PKEY_CTX_set1_hkdf_salt(context, salt,
                                    (int)ALRIOS_VAULT_SALT_LEN) <= 0 ||
        EVP_PKEY_CTX_set1_hkdf_key(context, master_key,
                                   (int)master_key_len) <= 0 ||
        EVP_PKEY_CTX_add1_hkdf_info(context,
                                    (const unsigned char *)info,
                                    (int)strlen(info)) <= 0 ||
        EVP_PKEY_derive(context, out_key, &output_len) <= 0 ||
        output_len != ALRIOS_VAULT_KEY_LEN) {
        alrios_explicit_zeroize(out_key, ALRIOS_VAULT_KEY_LEN);
        goto cleanup;
    }
    result = ALRIOS_VAULT_OK;

cleanup:
    EVP_PKEY_CTX_free(context);
    return result;
}

static int encrypt_store_with_key(const char *secrets,
                                  const uint8_t key[32],
                                  alrios_vault_store_t *out_store) {
    EVP_CIPHER_CTX *ctx = NULL;
    uint8_t *ciphertext = NULL;
    size_t secrets_len;
    int input_len;
    int output_len = 0;
    int final_len = 0;
    int result = ALRIOS_VAULT_ERR_CRYPTO;

    if (!secrets || !key || !out_store) {
        return ALRIOS_VAULT_ERR_ARGUMENT;
    }
    secrets_len = strlen(secrets);
    if (secrets_len == 0U || secrets_len > VAULT_MAX_BLOB_LEN ||
        checked_size_to_int(secrets_len, &input_len) != ALRIOS_VAULT_OK) {
        return ALRIOS_VAULT_ERR_SCHEMA;
    }
    ciphertext = malloc(secrets_len);
    if (!ciphertext) {
        return ALRIOS_VAULT_ERR_IO;
    }
    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        goto cleanup;
    }
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN,
                            (int)ALRIOS_VAULT_IV_LEN, NULL) != 1 ||
        EVP_EncryptInit_ex(ctx, NULL, NULL, key, out_store->iv) != 1 ||
        EVP_EncryptUpdate(ctx, ciphertext, &output_len,
                          (const unsigned char *)secrets, input_len) != 1 ||
        EVP_EncryptFinal_ex(ctx, ciphertext + output_len, &final_len) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG,
                            (int)ALRIOS_VAULT_TAG_LEN, out_store->tag) != 1) {
        goto cleanup;
    }
    if (output_len < 0 || final_len < 0 ||
        (size_t)output_len + (size_t)final_len != secrets_len) {
        result = ALRIOS_VAULT_ERR_CRYPTO;
        goto cleanup;
    }

    out_store->encrypted_blob = ciphertext;
    out_store->blob_len = secrets_len;
    ciphertext = NULL;
    result = ALRIOS_VAULT_OK;

cleanup:
    EVP_CIPHER_CTX_free(ctx);
    if (ciphertext) {
        alrios_explicit_zeroize(ciphertext, secrets_len);
        free(ciphertext);
    }
    return result;
}

int alrios_vault_encrypt_store(
    const char *json_secrets,
    const uint8_t *master_key,
    size_t key_len,
    alrios_vault_store_t *out_store
) {
    uint8_t derived_key[ALRIOS_VAULT_KEY_LEN];
    int result;

    if (!json_secrets || !master_key || key_len == 0U || !out_store) {
        return ALRIOS_VAULT_ERR_ARGUMENT;
    }
    memset(out_store, 0, sizeof(*out_store));
    if (RAND_bytes(out_store->salt, (int)sizeof(out_store->salt)) != 1 ||
        RAND_bytes(out_store->iv, (int)sizeof(out_store->iv)) != 1) {
        alrios_vault_store_free(out_store);
        return ALRIOS_VAULT_ERR_CRYPTO;
    }
    result = derive_store_key(out_store, master_key, key_len, 0U, derived_key);
    if (result == ALRIOS_VAULT_OK) {
        result = encrypt_store_with_key(json_secrets, derived_key, out_store);
    }
    alrios_explicit_zeroize(derived_key, sizeof(derived_key));
    if (result != ALRIOS_VAULT_OK) {
        alrios_vault_store_free(out_store);
    }
    return result;
}

int alrios_vault_seal_with_passphrase(
    const char *json_secrets,
    const uint8_t *passphrase,
    size_t passphrase_len,
    uint32_t iterations,
    alrios_vault_store_t *out_store
) {
    uint8_t derived_key[ALRIOS_VAULT_KEY_LEN];
    int result;

    if (!json_secrets || !passphrase || passphrase_len == 0U ||
        iterations == 0U || !out_store) {
        return ALRIOS_VAULT_ERR_ARGUMENT;
    }
    memset(out_store, 0, sizeof(*out_store));
    if (RAND_bytes(out_store->salt, (int)sizeof(out_store->salt)) != 1 ||
        RAND_bytes(out_store->iv, (int)sizeof(out_store->iv)) != 1) {
        alrios_vault_store_free(out_store);
        return ALRIOS_VAULT_ERR_CRYPTO;
    }
    result = derive_store_key(out_store, passphrase, passphrase_len,
                              iterations, derived_key);
    if (result == ALRIOS_VAULT_OK) {
        result = encrypt_store_with_key(json_secrets, derived_key, out_store);
    }
    alrios_explicit_zeroize(derived_key, sizeof(derived_key));
    if (result != ALRIOS_VAULT_OK) {
        alrios_vault_store_free(out_store);
    }
    return result;
}

static int render_records(const vault_records_t *records,
                          const char *app_id,
                          const char *const *authorized_keys,
                          size_t num_authorized,
                          char *out_env_buf,
                          size_t env_buf_len,
                          size_t *out_length,
                          size_t *out_count) {
    size_t offset = 0U;
    size_t count = 0U;
    size_t index;

    if (!records || !out_env_buf || env_buf_len == 0U) {
        return ALRIOS_VAULT_ERR_ARGUMENT;
    }
    out_env_buf[0] = '\0';

    for (index = 0U; index < records->count; ++index) {
        const vault_record_t *record = &records->records[index];
        size_t key_len;
        size_t value_len;
        size_t required;

        if (!record_belongs_to_app(record, app_id) ||
            !is_authorized_key(record->key, authorized_keys, num_authorized)) {
            continue;
        }
        key_len = strlen(record->key);
        value_len = strlen(record->value);
        if (key_len > SIZE_MAX - value_len - 3U) {
            alrios_explicit_zeroize(out_env_buf, env_buf_len);
            return ALRIOS_VAULT_ERR_IO;
        }
        required = key_len + value_len + 2U;
        if (required > env_buf_len - offset) {
            alrios_explicit_zeroize(out_env_buf, env_buf_len);
            return ALRIOS_VAULT_ERR_IO;
        }
        memcpy(out_env_buf + offset, record->key, key_len);
        offset += key_len;
        out_env_buf[offset++] = '=';
        memcpy(out_env_buf + offset, record->value, value_len);
        offset += value_len;
        out_env_buf[offset++] = '\n';
        out_env_buf[offset] = '\0';
        count++;
    }

    if (app_id && count == 0U) {
        alrios_explicit_zeroize(out_env_buf, env_buf_len);
        return ALRIOS_VAULT_ERR_NOT_FOUND;
    }
    if (out_length) {
        *out_length = offset;
    }
    if (out_count) {
        *out_count = count;
    }
    return ALRIOS_VAULT_OK;
}

int alrios_vault_decrypt_and_inject_for_app(
    const alrios_vault_store_t *store,
    const uint8_t *master_key,
    size_t key_len,
    const char *app_id,
    const char *const *authorized_keys,
    size_t num_authorized,
    char *out_env_buf,
    size_t env_buf_len
) {
    uint8_t derived_key[ALRIOS_VAULT_KEY_LEN];
    uint8_t *plaintext = NULL;
    size_t plaintext_len = 0U;
    vault_records_t records;
    int result;

    memset(&records, 0, sizeof(records));
    if (!store || !master_key || key_len == 0U ||
        (app_id && !is_valid_app_id(app_id)) ||
        (num_authorized > 0U && !authorized_keys) ||
        !out_env_buf || env_buf_len == 0U) {
        return ALRIOS_VAULT_ERR_ARGUMENT;
    }
    out_env_buf[0] = '\0';
    result = derive_store_key(store, master_key, key_len, 0U, derived_key);
    if (result == ALRIOS_VAULT_OK) {
        result = decrypt_store(store, derived_key, sizeof(derived_key),
                               &plaintext, &plaintext_len);
    }
    alrios_explicit_zeroize(derived_key, sizeof(derived_key));
    if (result != ALRIOS_VAULT_OK) {
        return result;
    }

    result = parse_records((char *)plaintext, &records);
    if (result == ALRIOS_VAULT_OK) {
        result = render_records(&records, app_id, authorized_keys,
                                num_authorized, out_env_buf, env_buf_len,
                                NULL, NULL);
    }
    records_clear(&records);
    alrios_explicit_zeroize(plaintext, plaintext_len + 1U);
    free(plaintext);
    if (result != ALRIOS_VAULT_OK) {
        alrios_explicit_zeroize(out_env_buf, env_buf_len);
    }
    return result;
}

int alrios_vault_decrypt_and_inject(
    const alrios_vault_store_t *store,
    const uint8_t *master_key,
    size_t key_len,
    const char *const *authorized_keys,
    size_t num_authorized,
    char *out_env_buf,
    size_t env_buf_len
) {
    return alrios_vault_decrypt_and_inject_for_app(
        store, master_key, key_len, NULL, authorized_keys, num_authorized,
        out_env_buf, env_buf_len);
}

int alrios_vault_inject_envp_for_app(
    const alrios_vault_store_t *store,
    const uint8_t *master_key,
    size_t key_len,
    const char *app_id,
    const char *const *authorized_keys,
    size_t num_authorized,
    char ***out_envp,
    size_t *out_count
) {
    char *buffer = NULL;
    char **envp = NULL;
    char *save = NULL;
    char *line;
    size_t buffer_size;
    size_t count = 0U;
    size_t index = 0U;
    int result;

    if (!store || !out_envp || !out_count || store->blob_len == 0U ||
        store->blob_len > VAULT_MAX_BLOB_LEN) {
        return ALRIOS_VAULT_ERR_ARGUMENT;
    }
    *out_envp = NULL;
    *out_count = 0U;
    if (store->blob_len > (SIZE_MAX - 2U) / 2U) {
        return ALRIOS_VAULT_ERR_IO;
    }
    buffer_size = (store->blob_len * 2U) + 2U;
    buffer = calloc(buffer_size, 1U);
    if (!buffer) {
        return ALRIOS_VAULT_ERR_IO;
    }
    result = alrios_vault_decrypt_and_inject_for_app(
        store, master_key, key_len, app_id, authorized_keys, num_authorized,
        buffer, buffer_size);
    if (result != ALRIOS_VAULT_OK) {
        goto cleanup;
    }
    for (index = 0U; buffer[index] != '\0'; ++index) {
        if (buffer[index] == '\n') {
            count++;
        }
    }
    if (count > (SIZE_MAX / sizeof(*envp)) - 1U) {
        result = ALRIOS_VAULT_ERR_IO;
        goto cleanup;
    }
    envp = calloc(count + 1U, sizeof(*envp));
    if (!envp) {
        result = ALRIOS_VAULT_ERR_IO;
        goto cleanup;
    }
    index = 0U;
    for (line = strtok_r(buffer, "\n", &save);
         line != NULL;
         line = strtok_r(NULL, "\n", &save)) {
        envp[index] = strdup(line);
        if (!envp[index]) {
            result = ALRIOS_VAULT_ERR_IO;
            goto cleanup;
        }
        index++;
    }
    envp[index] = NULL;
    *out_envp = envp;
    *out_count = index;
    envp = NULL;
    result = ALRIOS_VAULT_OK;

cleanup:
    if (envp) {
        alrios_vault_store_free_envp(envp, index);
    }
    alrios_explicit_zeroize(buffer, buffer_size);
    free(buffer);
    return result;
}

void alrios_vault_store_free_envp(char **envp, size_t count) {
    size_t index;

    if (!envp) {
        return;
    }
    for (index = 0U; index < count; ++index) {
        if (envp[index]) {
            alrios_explicit_zeroize(envp[index], strlen(envp[index]));
            free(envp[index]);
        }
    }
    alrios_explicit_zeroize(envp, (count + 1U) * sizeof(*envp));
    free(envp);
}

#if defined(__linux__) && defined(SYS_memfd_create)
static int write_all(int fd, const uint8_t *buffer, size_t length) {
    size_t offset = 0U;

    while (offset < length) {
        ssize_t written = write(fd, buffer + offset, length - offset);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return ALRIOS_VAULT_ERR_MEMFD;
        }
        if (written == 0) {
            return ALRIOS_VAULT_ERR_MEMFD;
        }
        offset += (size_t)written;
    }
    return ALRIOS_VAULT_OK;
}
#endif

int alrios_vault_inject_memfd_for_app(
    const alrios_vault_store_t *store,
    const uint8_t *master_key,
    size_t key_len,
    const char *app_id,
    const char *const *authorized_keys,
    size_t num_authorized,
    int *out_memfd
) {
#if !defined(__linux__) || !defined(SYS_memfd_create)
    (void)store;
    (void)master_key;
    (void)key_len;
    (void)app_id;
    (void)authorized_keys;
    (void)num_authorized;
    if (out_memfd) {
        *out_memfd = -1;
    }
    return ALRIOS_VAULT_ERR_MEMFD;
#else
    char *buffer = NULL;
    size_t buffer_size;
    size_t data_len;
    int memfd = -1;
    int result;

    if (!store || !out_memfd || store->blob_len == 0U ||
        store->blob_len > VAULT_MAX_BLOB_LEN ||
        store->blob_len > (SIZE_MAX - 2U) / 2U) {
        return ALRIOS_VAULT_ERR_ARGUMENT;
    }
    *out_memfd = -1;
    buffer_size = (store->blob_len * 2U) + 2U;
    buffer = calloc(buffer_size, 1U);
    if (!buffer) {
        return ALRIOS_VAULT_ERR_IO;
    }
    result = alrios_vault_decrypt_and_inject_for_app(
        store, master_key, key_len, app_id, authorized_keys, num_authorized,
        buffer, buffer_size);
    if (result != ALRIOS_VAULT_OK) {
        goto cleanup;
    }
    data_len = strlen(buffer);
    memfd = (int)syscall(SYS_memfd_create, "alrios_vault_secrets",
                         MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (memfd < 0) {
        result = ALRIOS_VAULT_ERR_MEMFD;
        goto cleanup;
    }
    result = write_all(memfd, (const uint8_t *)buffer, data_len);
    if (result != ALRIOS_VAULT_OK ||
        lseek(memfd, 0, SEEK_SET) < 0 ||
        fcntl(memfd, F_ADD_SEALS, VAULT_MEMFD_SEALS) != 0 ||
        (fcntl(memfd, F_GET_SEALS) & VAULT_MEMFD_SEALS) !=
            VAULT_MEMFD_SEALS) {
        result = ALRIOS_VAULT_ERR_MEMFD;
        goto cleanup;
    }
    *out_memfd = memfd;
    memfd = -1;
    result = ALRIOS_VAULT_OK;

cleanup:
    if (memfd >= 0) {
        (void)close(memfd);
    }
    alrios_explicit_zeroize(buffer, buffer_size);
    free(buffer);
    return result;
#endif
}

static void write_u64_be(uint8_t bytes[8], uint64_t value) {
    size_t index;

    for (index = 0U; index < 8U; ++index) {
        bytes[index] = (uint8_t)(value >> (56U - (index * 8U)));
    }
}

static uint64_t read_u64_be(const uint8_t bytes[8]) {
    uint64_t value = 0U;
    size_t index;

    for (index = 0U; index < 8U; ++index) {
        value = (value << 8U) | (uint64_t)bytes[index];
    }
    return value;
}

static int file_write_all(FILE *file, const uint8_t *buffer, size_t length) {
    size_t offset = 0U;

    while (offset < length) {
        size_t written = fwrite(buffer + offset, 1U, length - offset, file);
        if (written == 0U) {
            return ALRIOS_VAULT_ERR_IO;
        }
        offset += written;
    }
    return ALRIOS_VAULT_OK;
}

int alrios_vault_store_save_file(
    const char *path,
    const alrios_vault_store_t *store
) {
    uint8_t header[VAULT_FILE_HEADER_LEN];
    FILE *file = NULL;
    int result = ALRIOS_VAULT_ERR_IO;

    if (!path || !store || !store->encrypted_blob || store->blob_len == 0U ||
        store->blob_len > VAULT_MAX_BLOB_LEN) {
        return ALRIOS_VAULT_ERR_ARGUMENT;
    }
    memset(header, 0, sizeof(header));
    memcpy(header, VAULT_FILE_MAGIC, VAULT_FILE_MAGIC_LEN);
    write_u64_be(header + VAULT_FILE_MAGIC_LEN, (uint64_t)store->blob_len);
    memcpy(header + VAULT_FILE_MAGIC_LEN + 8U, store->salt,
           ALRIOS_VAULT_SALT_LEN);
    memcpy(header + VAULT_FILE_MAGIC_LEN + 8U + ALRIOS_VAULT_SALT_LEN,
           store->iv, ALRIOS_VAULT_IV_LEN);
    memcpy(header + VAULT_FILE_MAGIC_LEN + 8U + ALRIOS_VAULT_SALT_LEN +
           ALRIOS_VAULT_IV_LEN, store->tag, ALRIOS_VAULT_TAG_LEN);

#if defined(_WIN32)
    file = fopen(path, "wb");
#else
    {
        int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
        if (fd >= 0) {
            file = fdopen(fd, "wb");
            if (!file) {
                (void)close(fd);
            }
        }
    }
#endif
    if (!file) {
        goto cleanup;
    }
    if (file_write_all(file, header, sizeof(header)) != ALRIOS_VAULT_OK ||
        file_write_all(file, store->encrypted_blob, store->blob_len) !=
            ALRIOS_VAULT_OK ||
        fflush(file) != 0) {
        goto cleanup;
    }
    result = ALRIOS_VAULT_OK;

cleanup:
    if (file && fclose(file) != 0) {
        result = ALRIOS_VAULT_ERR_IO;
    }
    alrios_explicit_zeroize(header, sizeof(header));
    return result;
}

int alrios_vault_store_load_file(
    const char *path,
    alrios_vault_store_t *out_store
) {
    uint8_t header[VAULT_FILE_HEADER_LEN];
    uint8_t *ciphertext = NULL;
    FILE *file = NULL;
    uint64_t encoded_len;
    size_t blob_len;
    int result = ALRIOS_VAULT_ERR_IO;

    if (!path || !out_store) {
        return ALRIOS_VAULT_ERR_ARGUMENT;
    }
    memset(out_store, 0, sizeof(*out_store));
    memset(header, 0, sizeof(header));
    file = fopen(path, "rb");
    if (!file) {
        return ALRIOS_VAULT_ERR_IO;
    }
    if (fread(header, 1U, sizeof(header), file) != sizeof(header) ||
        memcmp(header, VAULT_FILE_MAGIC, VAULT_FILE_MAGIC_LEN) != 0) {
        result = ALRIOS_VAULT_ERR_FORMAT;
        goto cleanup;
    }
    encoded_len = read_u64_be(header + VAULT_FILE_MAGIC_LEN);
    if (encoded_len == 0U || encoded_len > VAULT_MAX_BLOB_LEN ||
        encoded_len > SIZE_MAX) {
        result = ALRIOS_VAULT_ERR_FORMAT;
        goto cleanup;
    }
    blob_len = (size_t)encoded_len;
    ciphertext = malloc(blob_len);
    if (!ciphertext) {
        result = ALRIOS_VAULT_ERR_IO;
        goto cleanup;
    }
    if (fread(ciphertext, 1U, blob_len, file) != blob_len ||
        fgetc(file) != EOF) {
        result = ALRIOS_VAULT_ERR_FORMAT;
        goto cleanup;
    }

    out_store->encrypted_blob = ciphertext;
    out_store->blob_len = blob_len;
    memcpy(out_store->salt, header + VAULT_FILE_MAGIC_LEN + 8U,
           ALRIOS_VAULT_SALT_LEN);
    memcpy(out_store->iv,
           header + VAULT_FILE_MAGIC_LEN + 8U + ALRIOS_VAULT_SALT_LEN,
           ALRIOS_VAULT_IV_LEN);
    memcpy(out_store->tag,
           header + VAULT_FILE_MAGIC_LEN + 8U + ALRIOS_VAULT_SALT_LEN +
           ALRIOS_VAULT_IV_LEN,
           ALRIOS_VAULT_TAG_LEN);
    ciphertext = NULL;
    result = ALRIOS_VAULT_OK;

cleanup:
    if (file) {
        (void)fclose(file);
    }
    if (ciphertext) {
        alrios_explicit_zeroize(ciphertext, (size_t)encoded_len);
        free(ciphertext);
    }
    alrios_explicit_zeroize(header, sizeof(header));
    if (result != ALRIOS_VAULT_OK) {
        alrios_vault_store_free(out_store);
    }
    return result;
}

void alrios_vault_store_free(alrios_vault_store_t *store) {
    if (!store) {
        return;
    }
    if (store->encrypted_blob) {
        alrios_explicit_zeroize(store->encrypted_blob, store->blob_len);
        free(store->encrypted_blob);
    }
    alrios_explicit_zeroize(store, sizeof(*store));
}
