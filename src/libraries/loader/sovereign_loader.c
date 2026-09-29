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

#include "alrios/loader/sovereign_loader.h"
#include <sys/mman.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

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
) {
    if (!arapp_stream || stream_len == 0 || !leaf_cert || !inter_cert || !store || !decryption_key || !out_memfd) {
        return ALRIOS_SOVEREIGN_LOADER_ERR_INVALID_PARAM;
    }

    *out_memfd = -1;

    uint64_t eval_time = (current_time != 0U) ? current_time : (uint64_t)time(NULL);

    arapp_v2_package_t pkg;
    int verify_rc = arapp_v2_verify_package_with_chain(
        arapp_stream,
        stream_len,
        leaf_cert,
        inter_cert,
        store,
        ml_dsa_65_pubkey,
        eval_time,
        &pkg
    );

    if (verify_rc != ARAPP_V2_OK) {
        return ALRIOS_SOVEREIGN_LOADER_ERR_VERIFY_FAIL;
    }

    if (pkg.ciphertext_len == 0U || !pkg.ciphertext) {
        return ALRIOS_SOVEREIGN_LOADER_ERR_INVALID_PARAM;
    }

    uint8_t *decrypted_elf = (uint8_t *)malloc(pkg.ciphertext_len);
    if (!decrypted_elf) {
        return ALRIOS_SOVEREIGN_LOADER_ERR_MEMFD_FAIL;
    }

    size_t aad_len = pkg.manifest_len;
    const uint8_t *aad_ptr = (const uint8_t *)pkg.manifest_json;
    if (aad_len > 0U && !aad_ptr) {
        free(decrypted_elf);
        return ALRIOS_SOVEREIGN_LOADER_ERR_INVALID_PARAM;
    }

    int decrypt_rc = alrios_aes256_gcm_decrypt(
        pkg.ciphertext,
        pkg.ciphertext_len,
        aad_ptr,
        aad_len,
        pkg.prefix.aes_gcm_tag,
        decryption_key,
        pkg.prefix.aes_gcm_iv,
        decrypted_elf
    );

    if (decrypt_rc != ALRIOS_CRYPTO_OK) {
        alrios_explicit_zeroize(decrypted_elf, pkg.ciphertext_len);
        free(decrypted_elf);
        return ALRIOS_SOVEREIGN_LOADER_ERR_DECRYPT_FAIL;
    }

    uint8_t computed_hash[SHA512_DIGEST_LEN];
    if (alrios_sha512(decrypted_elf, pkg.ciphertext_len, computed_hash) != ALRIOS_CRYPTO_OK) {
        alrios_explicit_zeroize(decrypted_elf, pkg.ciphertext_len);
        free(decrypted_elf);
        return ALRIOS_SOVEREIGN_LOADER_ERR_DECRYPT_FAIL;
    }

    if (alrios_constant_time_memcmp(computed_hash, pkg.prefix.cleartext_sha512, SHA512_DIGEST_LEN) != 0) {
        alrios_explicit_zeroize(decrypted_elf, pkg.ciphertext_len);
        free(decrypted_elf);
        return ALRIOS_SOVEREIGN_LOADER_ERR_HASH_MISMATCH;
    }

    int memfd = (int)syscall(SYS_memfd_create, "alrios_sovereign_exec", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (memfd < 0) {
        alrios_explicit_zeroize(decrypted_elf, pkg.ciphertext_len);
        free(decrypted_elf);
        return ALRIOS_SOVEREIGN_LOADER_ERR_MEMFD_FAIL;
    }

    size_t total_written = 0U;
    while (total_written < pkg.ciphertext_len) {
        ssize_t w = write(memfd, decrypted_elf + total_written, pkg.ciphertext_len - total_written);
        if (w < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(memfd);
            alrios_explicit_zeroize(decrypted_elf, pkg.ciphertext_len);
            free(decrypted_elf);
            return ALRIOS_SOVEREIGN_LOADER_ERR_IO_FAIL;
        }
        if (w == 0) {
            close(memfd);
            alrios_explicit_zeroize(decrypted_elf, pkg.ciphertext_len);
            free(decrypted_elf);
            return ALRIOS_SOVEREIGN_LOADER_ERR_IO_FAIL;
        }
        total_written += (size_t)w;
    }

    alrios_explicit_zeroize(decrypted_elf, pkg.ciphertext_len);
    free(decrypted_elf);

    if (fcntl(memfd, F_ADD_SEALS, F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE) < 0) {
        close(memfd);
        return ALRIOS_SOVEREIGN_LOADER_ERR_SEALING_FAIL;
    }

    *out_memfd = memfd;
    return ALRIOS_SOVEREIGN_LOADER_OK;
}

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
) {
    int memfd = -1;
    int rc = alrios_sovereign_loader_prepare(
        arapp_stream,
        stream_len,
        leaf_cert,
        inter_cert,
        store,
        ml_dsa_65_pubkey,
        current_time,
        decryption_key,
        &memfd
    );

    if (rc != ALRIOS_SOVEREIGN_LOADER_OK) {
        return rc;
    }

    fexecve(memfd, argv, envp);

    int err = errno;
    close(memfd);
    (void)err;
    return ALRIOS_SOVEREIGN_LOADER_ERR_EXEC_FAIL;
}
