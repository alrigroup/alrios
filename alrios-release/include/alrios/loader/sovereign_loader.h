/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_SOVEREIGN_LOADER_H
#define ALRIOS_SOVEREIGN_LOADER_H

#include <stdint.h>
#include <stddef.h>
#include "alrios/package/arapp_v2.h"
#include "alrios/pki/certificate.h"
#include "alrios/crypto_verify.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ALRIOS_SOVEREIGN_LOADER_OK                  0
#define ALRIOS_SOVEREIGN_LOADER_ERR_INVALID_PARAM  -7001
#define ALRIOS_SOVEREIGN_LOADER_ERR_VERIFY_FAIL    -7002
#define ALRIOS_SOVEREIGN_LOADER_ERR_DECRYPT_FAIL   -7003
#define ALRIOS_SOVEREIGN_LOADER_ERR_HASH_MISMATCH  -7004
#define ALRIOS_SOVEREIGN_LOADER_ERR_MEMFD_FAIL     -7005
#define ALRIOS_SOVEREIGN_LOADER_ERR_IO_FAIL        -7006
#define ALRIOS_SOVEREIGN_LOADER_ERR_SEALING_FAIL   -7007
#define ALRIOS_SOVEREIGN_LOADER_ERR_EXEC_FAIL      -7008

int alrios_sovereign_loader_prepare(
    const uint8_t *arapp_stream,
    size_t stream_len,
    const alrios_certificate_t *leaf_cert,
    const alrios_certificate_t *inter_cert,
    const alrios_trust_store_t *store,
    const uint8_t *ml_dsa_65_pubkey,
    uint64_t current_time,
    const uint8_t decryption_key[AES256_KEY_LEN],
    int *out_memfd
);

int alrios_sovereign_loader_run(
    const uint8_t *arapp_stream,
    size_t stream_len,
    const alrios_certificate_t *leaf_cert,
    const alrios_certificate_t *inter_cert,
    const alrios_trust_store_t *store,
    const uint8_t *ml_dsa_65_pubkey,
    uint64_t current_time,
    const uint8_t decryption_key[AES256_KEY_LEN],
    char *const argv[],
    char *const envp[]
);

#ifdef __cplusplus
}
#endif

#endif
