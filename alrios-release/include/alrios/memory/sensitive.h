/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_MEMORY_SENSITIVE_H
#define ALRIOS_MEMORY_SENSITIVE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALRIOS_SENSITIVE_OK 0
#define ALRIOS_SENSITIVE_ERR_INVALID -2001
#define ALRIOS_SENSITIVE_ERR_CAPACITY -2002
#define ALRIOS_SENSITIVE_ERR_ENCRYPT -2003
#define ALRIOS_SENSITIVE_ERR_AUTH -2004

int alrios_sensitive_register(const void *ptr, size_t size);
int alrios_sensitive_unregister(const void *ptr);
int alrios_sensitive_sanitize_dump(const void *raw_dump, size_t dump_size,
                                   const uint8_t *key, const uint8_t *iv,
                                   uint8_t *out_encrypted_dump, size_t *out_encrypted_size,
                                   uint8_t out_tag[16]);
int alrios_sensitive_decrypt_dump(const uint8_t *encrypted_dump, size_t encrypted_size,
                                  const uint8_t *key, const uint8_t *iv,
                                  const uint8_t tag[16],
                                  uint8_t *out_decrypted_dump, size_t *out_decrypted_size);

#ifdef __cplusplus
}
#endif

#endif
