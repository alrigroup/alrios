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
#include "alrios/package/arapp_v2.h"

#include <openssl/evp.h>
#include <openssl/sha.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>

/* External signature verification from arapp_v2_signatures.c */
int alrios_arapp_v2_verify_hybrid(const uint8_t *package, size_t package_len,
                                   EVP_PKEY *ed25519_pubkey,
                                   EVP_PKEY *ml_dsa_pubkey,
                                   int require_pqc);

#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif
#ifndef MFD_ALLOW_SEALING
#define MFD_ALLOW_SEALING 0x0002U
#endif
#ifndef F_ADD_SEALS
#define F_ADD_SEALS 1033
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

static int decrypt_aes_gcm_256(
    const uint8_t *ciphertext, size_t ciphertext_len,
    const uint8_t *key, const uint8_t *iv, const uint8_t *tag,
    uint8_t **out_plaintext, size_t *out_len
) {
    EVP_CIPHER_CTX *ctx = NULL;
    int len = 0;
    int plaintext_len = 0;
    uint8_t *plaintext = NULL;
    int ret = ALRIOS_SOVEREIGN_ERR_DECRYPT;

    if (!ciphertext || !key || !iv || !tag || !out_plaintext || !out_len) {
        return ALRIOS_SOVEREIGN_ERR_ARGUMENT;
    }
    *out_plaintext = NULL;
    *out_len = 0U;

    plaintext = malloc(ciphertext_len > 0U ? ciphertext_len : 1U);
    if (!plaintext) {
        return ALRIOS_SOVEREIGN_ERR_DECRYPT;
    }

    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        free(plaintext);
        return ALRIOS_SOVEREIGN_ERR_DECRYPT;
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, NULL) != 1 ||
        EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
        goto cleanup;
    }

    if (ciphertext_len > 0U) {
        if (EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext,
                              (int)ciphertext_len) != 1) {
            goto cleanup;
        }
        plaintext_len = len;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, (void *)tag) != 1) {
        goto cleanup;
    }

    if (EVP_DecryptFinal_ex(ctx, plaintext + plaintext_len, &len) != 1) {
        goto cleanup;
    }
    plaintext_len += len;

    *out_plaintext = plaintext;
    *out_len = (size_t)plaintext_len;
    ret = ALRIOS_SOVEREIGN_OK;
    plaintext = NULL;

cleanup:
    EVP_CIPHER_CTX_free(ctx);
    free(plaintext);
    return ret;
}

int alrios_sovereign_execute_package(
    const uint8_t *package_data,
    size_t package_len,
    uint64_t now,
    int require_pqc,
    const alrios_sovereign_keys_t *keys,
    char *const argv[],
    char *const envp[],
    int *out_child_status
) {
#if !defined(__linux__)
    (void)package_data; (void)package_len; (void)now; (void)require_pqc;
    (void)keys; (void)argv; (void)envp; (void)out_child_status;
    return ALRIOS_SOVEREIGN_ERR_PLATFORM;
#else
    alrios_arapp_v2_view_t view;
    int validation_status;
    int sig_status;
    uint8_t computed_sha[64];
    uint8_t *plaintext = NULL;
    size_t plaintext_len = 0U;
    int memfd = -1;
    pid_t pid;
    int status;
    size_t written = 0U;

    if (!package_data || !keys || !keys->ed25519_pubkey ||
        !keys->aes_key || !out_child_status) {
        return ALRIOS_SOVEREIGN_ERR_ARGUMENT;
    }
    *out_child_status = -1;

    /* 1. VERIFY STRUCTURAL INTEGRITY AND TIMESTAMPS FIRST (Fail-Closed) */
    validation_status = alrios_arapp_v2_validate(
        package_data,
        package_len,
        now,
        ALRIOS_ARAPP_V2_MAX_CLOCK_SKEW_SECONDS,
        &view
    );
    if (validation_status != ALRIOS_ARAPP_V2_OK) {
        return ALRIOS_SOVEREIGN_ERR_VERIFY;
    }

    /* 2. VERIFY CRYPTOGRAPHIC SIGNATURES BEFORE DECRYPT */
    sig_status = alrios_arapp_v2_verify_hybrid(
        package_data,
        package_len,
        keys->ed25519_pubkey,
        keys->ml_dsa_pubkey,
        require_pqc
    );
    if (sig_status != 0) {
        return ALRIOS_SOVEREIGN_ERR_VERIFY;
    }

    /* 3. AUTHENTICATED DECRYPT (AES-256-GCM) */
    validation_status = decrypt_aes_gcm_256(
        view.ciphertext,
        view.ciphertext_len,
        keys->aes_key,
        view.gcm_iv,
        view.gcm_tag,
        &plaintext,
        &plaintext_len
    );
    if (validation_status != ALRIOS_SOVEREIGN_OK) {
        return ALRIOS_SOVEREIGN_ERR_DECRYPT;
    }

    /* 4. VERIFY SHA-512 CLEARTEXT HASH AGAINST MANIFEST */
    SHA512(plaintext, plaintext_len, computed_sha);
    if (memcmp(computed_sha, view.cleartext_sha512, 64U) != 0) {
        free(plaintext);
        return ALRIOS_SOVEREIGN_ERR_VERIFY;
    }

    /* 5. CREATE MEMFD AND WRITE PLAINTEXT (NEVER WRITTEN TO DISK) */
    memfd = (int)syscall(SYS_memfd_create, "alrios_sovereign_exec",
                         MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (memfd < 0) {
        free(plaintext);
        memset(plaintext, 0, plaintext_len);
        return ALRIOS_SOVEREIGN_ERR_MEMFD;
    }

    while (written < plaintext_len) {
        ssize_t w = write(memfd, plaintext + written, plaintext_len - written);
        if (w < 0) {
            if (errno == EINTR) continue;
            close(memfd);
            free(plaintext);
            return ALRIOS_SOVEREIGN_ERR_IO;
        }
        written += (size_t)w;
    }

    /* Zeroize plaintext copy in memory immediately */
    memset(plaintext, 0, plaintext_len);
    free(plaintext);
    plaintext = NULL;

    /* 6. SEAL MEMFD (F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE) */
    if (fcntl(memfd, F_ADD_SEALS,
              F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE) < 0) {
        close(memfd);
        return ALRIOS_SOVEREIGN_ERR_SEAL;
    }

    /* 7. EXECUTE VIA fexecve */
    pid = fork();
    if (pid < 0) {
        close(memfd);
        return ALRIOS_SOVEREIGN_ERR_PLATFORM;
    }
    if (pid == 0) {
        char *default_argv[] = {(char *)"sovereign_app", NULL};
        char *default_envp[] = {NULL};
        char *const *exec_argv = argv ? argv : default_argv;
        char *const *exec_envp = envp ? envp : default_envp;

        fexecve(memfd, exec_argv, exec_envp);
        _exit(127);
    }

    close(memfd);

    if (waitpid(pid, &status, 0) < 0) {
        return ALRIOS_SOVEREIGN_ERR_PLATFORM;
    }
    *out_child_status = status;
    return ALRIOS_SOVEREIGN_OK;
#endif
}
