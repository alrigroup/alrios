/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#ifndef ALRIOS_MEMLOADER_H
#define ALRIOS_MEMLOADER_H

#include <stdint.h>
#include <stddef.h>

#define ALRIOS_MEMLOADER_OK                 0
#define ALRIOS_MEMLOADER_ERR_INVALID_PARAM -101
#define ALRIOS_MEMLOADER_ERR_BAD_MAGIC     -102
#define ALRIOS_MEMLOADER_ERR_CORRUPT_HEADER -103
#define ALRIOS_MEMLOADER_ERR_LENGTH_MISMATCH -104
#define ALRIOS_MEMLOADER_ERR_EXPIRED       -105
#define ALRIOS_MEMLOADER_ERR_OOM           -106
#define ALRIOS_MEMLOADER_ERR_CRYPTO_FAIL   -107
#define ALRIOS_MEMLOADER_ERR_SIG_FAIL_CLASSIC -108
#define ALRIOS_MEMLOADER_ERR_SIG_FAIL_PQC  -109
#define ALRIOS_MEMLOADER_ERR_DECRYPT_FAIL  -110
#define ALRIOS_MEMLOADER_ERR_ELF_HASH_MISMATCH -111
#define ALRIOS_MEMLOADER_ERR_MEMFD_FAIL    -112
#define ALRIOS_MEMLOADER_ERR_IO_FAIL       -113
#define ALRIOS_MEMLOADER_ERR_SEALING_FAIL  -114
#define ALRIOS_MEMLOADER_ERR_EXEC_DENIED   -115
#define ALRIOS_MEMLOADER_ERR_EXEC_FAIL     -116

int alrios_memfd_create_sealed(const char *name, const uint8_t *payload, size_t size);
int alrios_memloader_run(const uint8_t *arapp_stream, size_t stream_len, const uint8_t key[32], char *const argv[], char *const envp[]);

#endif /* ALRIOS_MEMLOADER_H */
