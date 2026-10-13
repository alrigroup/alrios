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
#include "alrios/crypto_verify.h"
#include "alrios/package/arapp_v2.h"
#include "alrios/pki/issuer.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

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

#define ALRIOS_REQUIRED_MEMFD_SEALS \
    (F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE)

static int bytes_all_zero(const uint8_t *bytes, size_t length) {
    uint8_t combined = 0U;
    size_t index;

    for (index = 0U; index < length; ++index) {
        combined |= bytes[index];
    }
    return combined == 0U;
}

static int verify_certificate_chain(const alrios_sovereign_keys_t *keys,
                                    uint64_t now) {
    uint8_t leaf_key[ALRIOS_CERTIFICATE_ED25519_KEY_SIZE];
    size_t leaf_key_len = sizeof(leaf_key);

    if (!keys->leaf_certificate || !keys->intermediate_certificate ||
        !keys->trust_store ||
        alrios_pki_verify_chain(keys->trust_store, keys->leaf_certificate,
                               keys->intermediate_certificate, now) !=
            ALRIOS_ISSUER_OK ||
        keys->leaf_certificate->public_key_algorithm !=
            ALRIOS_PUBLIC_KEY_ALGORITHM_ED25519 ||
        keys->leaf_certificate->public_key_len != sizeof(leaf_key) ||
        EVP_PKEY_get_raw_public_key(keys->ed25519_pubkey, leaf_key,
                                    &leaf_key_len) != 1 ||
        leaf_key_len != keys->leaf_certificate->public_key_len ||
        CRYPTO_memcmp(leaf_key, keys->leaf_certificate->public_key,
                      leaf_key_len) != 0) {
        alrios_explicit_zeroize(leaf_key, sizeof(leaf_key));
        return ALRIOS_SOVEREIGN_ERR_VERIFY;
    }

    alrios_explicit_zeroize(leaf_key, sizeof(leaf_key));
    return ALRIOS_SOVEREIGN_OK;
}

static int verify_envelope(const uint8_t *package_data,
                           size_t package_len,
                           uint64_t now,
                           uint32_t policy_flags,
                           const alrios_sovereign_keys_t *keys,
                           alrios_arapp_v2_view_t *out_view) {
    int has_ed25519_signature;
    int has_ml_dsa_signature;

    if (alrios_arapp_v2_validate(package_data, package_len, now,
                                 ALRIOS_ARAPP_V2_MAX_CLOCK_SKEW_SECONDS,
                                 out_view) != ALRIOS_ARAPP_V2_OK) {
        return ALRIOS_SOVEREIGN_ERR_VERIFY;
    }

    has_ed25519_signature = !bytes_all_zero(
        out_view->ed25519_signature, ALRIOS_ARAPP_V2_ED25519_SIGNATURE_SIZE);
    has_ml_dsa_signature = !bytes_all_zero(
        out_view->ml_dsa_65_signature,
        ALRIOS_ARAPP_V2_ML_DSA_65_SIGNATURE_SIZE);
    if (!has_ed25519_signature) {
        if (has_ml_dsa_signature ||
            (out_view->flags & ALRIOS_ARAPP_V2_FLAG_PQC_HYBRID_SIG) != 0U) {
            return ALRIOS_SOVEREIGN_ERR_VERIFY;
        }
        if ((policy_flags & ALRIOS_SOVEREIGN_POLICY_ALLOW_UNSIGNED) == 0U) {
            return ALRIOS_SOVEREIGN_ERR_POLICY;
        }
        return ALRIOS_SOVEREIGN_OK;
    }

    if (!keys->ed25519_pubkey ||
        verify_certificate_chain(keys, now) != ALRIOS_SOVEREIGN_OK) {
        return ALRIOS_SOVEREIGN_ERR_VERIFY;
    }
    if ((policy_flags & ALRIOS_SOVEREIGN_POLICY_REQUIRE_PQC) != 0U &&
        (out_view->flags & ALRIOS_ARAPP_V2_FLAG_PQC_HYBRID_SIG) == 0U) {
        return ALRIOS_SOVEREIGN_ERR_POLICY;
    }
    if ((out_view->flags & ALRIOS_ARAPP_V2_FLAG_PQC_HYBRID_SIG) != 0U &&
        (!has_ml_dsa_signature || !keys->ml_dsa_pubkey)) {
        return ALRIOS_SOVEREIGN_ERR_VERIFY;
    }
    if ((out_view->flags & ALRIOS_ARAPP_V2_FLAG_PQC_HYBRID_SIG) == 0U &&
        has_ml_dsa_signature) {
        return ALRIOS_SOVEREIGN_ERR_VERIFY;
    }
    if (alrios_arapp_v2_verify_hybrid(
            package_data, package_len, keys->ed25519_pubkey,
            keys->ml_dsa_pubkey,
            (policy_flags & ALRIOS_SOVEREIGN_POLICY_REQUIRE_PQC) != 0U ||
            (out_view->flags & ALRIOS_ARAPP_V2_FLAG_PQC_HYBRID_SIG) != 0U) !=
        ALRIOS_ARAPP_V2_SIG_OK) {
        return ALRIOS_SOVEREIGN_ERR_VERIFY;
    }
    return ALRIOS_SOVEREIGN_OK;
}

static int decrypt_payload(const alrios_arapp_v2_view_t *view,
                           const alrios_sovereign_keys_t *keys,
                           uint8_t **out_plaintext,
                           size_t *out_plaintext_len) {
    uint8_t computed_sha[ALRIOS_ARAPP_V2_SHA512_SIZE];
    uint8_t *plaintext;
    int encrypted;

    *out_plaintext = NULL;
    *out_plaintext_len = 0U;
    if (view->ciphertext_len == 0U || view->ciphertext_len > (size_t)INT_MAX) {
        return ALRIOS_SOVEREIGN_ERR_VERIFY;
    }

    plaintext = (uint8_t *)malloc(view->ciphertext_len);
    if (!plaintext) {
        return ALRIOS_SOVEREIGN_ERR_DECRYPT;
    }

    encrypted = (view->flags & ALRIOS_ARAPP_V2_FLAG_ENCRYPTED_GCM) != 0U;
    if (encrypted) {
        if (!keys->aes_key || keys->aes_key_len != AES256_KEY_LEN ||
            alrios_aes256_gcm_decrypt(
                view->ciphertext, view->ciphertext_len,
                view->manifest, view->manifest_len, view->gcm_tag,
                keys->aes_key, view->gcm_iv, plaintext) != ALRIOS_CRYPTO_OK) {
            alrios_explicit_zeroize(plaintext, view->ciphertext_len);
            free(plaintext);
            return ALRIOS_SOVEREIGN_ERR_DECRYPT;
        }
    } else {
        memcpy(plaintext, view->ciphertext, view->ciphertext_len);
    }

    if (SHA512(plaintext, view->ciphertext_len, computed_sha) == NULL ||
        CRYPTO_memcmp(computed_sha, view->cleartext_sha512,
                      sizeof(computed_sha)) != 0) {
        alrios_explicit_zeroize(computed_sha, sizeof(computed_sha));
        alrios_explicit_zeroize(plaintext, view->ciphertext_len);
        free(plaintext);
        return ALRIOS_SOVEREIGN_ERR_VERIFY;
    }

    alrios_explicit_zeroize(computed_sha, sizeof(computed_sha));
    *out_plaintext = plaintext;
    *out_plaintext_len = view->ciphertext_len;
    return ALRIOS_SOVEREIGN_OK;
}

static int create_sealed_memfd(const uint8_t *plaintext,
                               size_t plaintext_len,
                               int *out_memfd) {
#if !defined(__linux__) || !defined(SYS_memfd_create)
    (void)plaintext;
    (void)plaintext_len;
    (void)out_memfd;
    return ALRIOS_SOVEREIGN_ERR_PLATFORM;
#else
    size_t written = 0U;
    int memfd;
    int seals;

    memfd = (int)syscall(SYS_memfd_create, "alrios_sovereign_exec",
                         MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (memfd < 0) {
        return ALRIOS_SOVEREIGN_ERR_MEMFD;
    }

    while (written < plaintext_len) {
        ssize_t count = write(memfd, plaintext + written,
                              plaintext_len - written);
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            (void)close(memfd);
            return ALRIOS_SOVEREIGN_ERR_IO;
        }
        if (count == 0) {
            (void)close(memfd);
            return ALRIOS_SOVEREIGN_ERR_IO;
        }
        written += (size_t)count;
    }

    if (fcntl(memfd, F_ADD_SEALS, ALRIOS_REQUIRED_MEMFD_SEALS) < 0) {
        (void)close(memfd);
        return ALRIOS_SOVEREIGN_ERR_SEAL;
    }
    seals = fcntl(memfd, F_GET_SEALS);
    if (seals < 0 || (seals & ALRIOS_REQUIRED_MEMFD_SEALS) !=
                         ALRIOS_REQUIRED_MEMFD_SEALS) {
        (void)close(memfd);
        return ALRIOS_SOVEREIGN_ERR_SEAL;
    }

    *out_memfd = memfd;
    return ALRIOS_SOVEREIGN_OK;
#endif
}

int alrios_sovereign_loader_stage_memfd(
    const uint8_t *package_data,
    size_t package_len,
    uint64_t now,
    uint32_t policy_flags,
    const alrios_sovereign_keys_t *keys,
    int *out_memfd
) {
    alrios_arapp_v2_view_t view;
    uint8_t *plaintext = NULL;
    size_t plaintext_len = 0U;
    int result;

    if (!out_memfd) {
        return ALRIOS_SOVEREIGN_ERR_ARGUMENT;
    }
    *out_memfd = -1;
    if (!package_data || !keys ||
        (policy_flags & ~ALRIOS_SOVEREIGN_POLICY_MASK) != 0U ||
        ((policy_flags & ALRIOS_SOVEREIGN_POLICY_ALLOW_UNSIGNED) != 0U &&
         (policy_flags & ALRIOS_SOVEREIGN_POLICY_REQUIRE_PQC) != 0U)) {
        return ALRIOS_SOVEREIGN_ERR_ARGUMENT;
    }

    result = verify_envelope(package_data, package_len, now, policy_flags,
                             keys, &view);
    if (result != ALRIOS_SOVEREIGN_OK) {
        return result;
    }

    result = decrypt_payload(&view, keys, &plaintext, &plaintext_len);
    if (result != ALRIOS_SOVEREIGN_OK) {
        return result;
    }

    result = create_sealed_memfd(plaintext, plaintext_len, out_memfd);
    alrios_explicit_zeroize(plaintext, plaintext_len);
    free(plaintext);
    return result;
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
    (void)package_data;
    (void)package_len;
    (void)now;
    (void)require_pqc;
    (void)keys;
    (void)argv;
    (void)envp;
    (void)out_child_status;
    return ALRIOS_SOVEREIGN_ERR_PLATFORM;
#else
    uint32_t policy_flags = require_pqc != 0 ?
        ALRIOS_SOVEREIGN_POLICY_REQUIRE_PQC : 0U;
    int memfd = -1;
    pid_t pid;
    int status;
    int result;

    if (!out_child_status) {
        return ALRIOS_SOVEREIGN_ERR_ARGUMENT;
    }
    *out_child_status = -1;
    result = alrios_sovereign_loader_stage_memfd(
        package_data, package_len, now, policy_flags, keys, &memfd);
    if (result != ALRIOS_SOVEREIGN_OK) {
        return result;
    }

    pid = fork();
    if (pid < 0) {
        (void)close(memfd);
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

    (void)close(memfd);
    do {
        result = waitpid(pid, &status, 0) < 0 ?
            ALRIOS_SOVEREIGN_ERR_PLATFORM : ALRIOS_SOVEREIGN_OK;
    } while (result != ALRIOS_SOVEREIGN_OK && errno == EINTR);
    if (result != ALRIOS_SOVEREIGN_OK) {
        return result;
    }
    *out_child_status = status;
    return ALRIOS_SOVEREIGN_OK;
#endif
}
