/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "arapp_sign.h"
#include "alrios/package/arapp_v2.h"
#include "alrios/pki/notary.h"
#include "alrios/pki/issuer.h"
#include "alrios/pki/certificate.h"
#include "alrios/crypto_verify.h"

#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ARAPP_MAX_FILE_SIZE (64U * 1024U * 1024U)

static int read_file(const char *path, uint8_t **out_data, size_t *out_len) {
    FILE *f;
    long sz;
    uint8_t *buf;
    size_t read_bytes;

    if (!path || !out_data || !out_len) {
        return -1;
    }
    *out_data = NULL;
    *out_len = 0U;

    f = fopen(path, "rb");
    if (!f) {
        return -1;
    }
    if (fseek(f, 0L, SEEK_END) != 0) {
        (void)fclose(f);
        return -1;
    }
    sz = ftell(f);
    if (sz <= 0 || (size_t)sz > ARAPP_MAX_FILE_SIZE) {
        (void)fclose(f);
        return -1;
    }
    if (fseek(f, 0L, SEEK_SET) != 0) {
        (void)fclose(f);
        return -1;
    }
    buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) {
        (void)fclose(f);
        return -1;
    }
    read_bytes = fread(buf, 1U, (size_t)sz, f);
    (void)fclose(f);
    if (read_bytes != (size_t)sz) {
        free(buf);
        return -1;
    }
    *out_data = buf;
    *out_len = (size_t)sz;
    return 0;
}

static int write_file(const char *path, const uint8_t *data, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        return -1;
    }
    if (fwrite(data, 1U, len, f) != len) {
        (void)fclose(f);
        return -1;
    }
    if (fflush(f) != 0 || fclose(f) != 0) {
        return -1;
    }
    return 0;
}

static int next_is_value(int argc, char **argv, int idx) {
    if (!argv || idx < 0 || idx + 1 >= argc || !argv[idx + 1]) {
        return 0;
    }
    if (argv[idx + 1][0] == '\0' || strncmp(argv[idx + 1], "--", 2U) == 0) {
        return 0;
    }
    return 1;
}

int arsign_cmd_sign_arapp(int argc, char **argv) {
    const char *key_file = NULL;
    const char *in_file = NULL;
    const char *out_file = NULL;
    const char *leaf_file = NULL;
    const char *inter_file = NULL;
    const char *root_file = NULL;
    int i;
    int rc;

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--key") == 0) {
            if (!next_is_value(argc, argv, i)) return 1;
            key_file = argv[++i];
        } else if (strcmp(argv[i], "--in") == 0) {
            if (!next_is_value(argc, argv, i)) return 1;
            in_file = argv[++i];
        } else if (strcmp(argv[i], "--out") == 0) {
            if (!next_is_value(argc, argv, i)) return 1;
            out_file = argv[++i];
        } else if (strcmp(argv[i], "--leaf") == 0) {
            if (!next_is_value(argc, argv, i)) return 1;
            leaf_file = argv[++i];
        } else if (strcmp(argv[i], "--inter") == 0) {
            if (!next_is_value(argc, argv, i)) return 1;
            inter_file = argv[++i];
        } else if (strcmp(argv[i], "--root") == 0) {
            if (!next_is_value(argc, argv, i)) return 1;
            root_file = argv[++i];
        } else {
            (void)fprintf(stderr, "[ERROR] Unknown option for sign-arapp: %s\n", argv[i]);
            return 1;
        }
    }

    if (!key_file || !in_file) {
        (void)fprintf(stderr, "Usage: arsign sign-arapp --key <key_file> --in <arapp_file> [--out <output_file>] [--leaf <leaf.bin> --inter <inter.bin> --root <root.bin>]\n");
        return 1;
    }

    if (!out_file) {
        out_file = in_file;
    }

    /* Certificate chain validation if certificates provided */
    if (leaf_file || inter_file || root_file) {
        if (!leaf_file || !inter_file || !root_file) {
            (void)fprintf(stderr, "[ERROR] Complete chain (--leaf, --inter, --root) must be provided\n");
            return 1;
        }

        uint8_t *leaf_bytes = NULL; size_t leaf_len = 0U;
        uint8_t *inter_bytes = NULL; size_t inter_len = 0U;
        uint8_t *root_bytes = NULL; size_t root_len = 0U;

        if (read_file(leaf_file, &leaf_bytes, &leaf_len) != 0 ||
            read_file(inter_file, &inter_bytes, &inter_len) != 0 ||
            read_file(root_file, &root_bytes, &root_len) != 0) {
            (void)fprintf(stderr, "[ERROR] Failed to read certificate files\n");
            free(leaf_bytes); free(inter_bytes); free(root_bytes);
            return 1;
        }

        alrios_certificate_t leaf_cert;
        alrios_certificate_t inter_cert;
        alrios_certificate_t root_cert;
        alrios_trust_store_t store;

        if (alrios_certificate_decode(leaf_bytes, leaf_len, &leaf_cert) != ALRIOS_CERTIFICATE_OK ||
            alrios_certificate_decode(inter_bytes, inter_len, &inter_cert) != ALRIOS_CERTIFICATE_OK ||
            alrios_certificate_decode(root_bytes, root_len, &root_cert) != ALRIOS_CERTIFICATE_OK) {
            (void)fprintf(stderr, "[ERROR] Failed to decode certificate chain\n");
            free(leaf_bytes); free(inter_bytes); free(root_bytes);
            return 1;
        }
        free(leaf_bytes); free(inter_bytes); free(root_bytes);

        alrios_trust_store_init(&store);
        if (alrios_trust_store_add_anchor(&store, &root_cert) != ALRIOS_CERTIFICATE_OK ||
            alrios_trust_store_seal(&store) != ALRIOS_CERTIFICATE_OK) {
            (void)fprintf(stderr, "[ERROR] Failed to initialize trust store anchor\n");
            alrios_trust_store_clear(&store);
            return 1;
        }

        rc = alrios_pki_verify_chain(&store, &leaf_cert, &inter_cert, (uint64_t)time(NULL));
        alrios_trust_store_clear(&store);
        if (rc != ALRIOS_ISSUER_OK) {
            (void)fprintf(stderr, "[ERROR] Certificate chain verification failed (rc=%d)\n", rc);
            return 1;
        }
    }

    EVP_PKEY *privkey = NULL;
    rc = alrios_notary_load_private_key_file(key_file, &privkey);
    if (rc != ALRIOS_NOTARY_OK || !privkey) {
        (void)fprintf(stderr, "[ERROR] Failed to load private key from %s (rc=%d)\n", key_file, rc);
        return 1;
    }

    uint8_t *pkg_data = NULL;
    size_t pkg_len = 0U;
    if (read_file(in_file, &pkg_data, &pkg_len) != 0 || !pkg_data) {
        (void)fprintf(stderr, "[ERROR] Failed to read arapp package from %s\n", in_file);
        EVP_PKEY_free(privkey);
        return 1;
    }

    if (pkg_len < ALRIOS_ARAPP_V2_HEADER_SIZE) {
        (void)fprintf(stderr, "[ERROR] Package is smaller than arapp v2 header size\n");
        free(pkg_data);
        EVP_PKEY_free(privkey);
        return 1;
    }

    rc = alrios_arapp_v2_sign_ed25519(pkg_data, pkg_len, privkey);
    EVP_PKEY_free(privkey);

    if (rc != ALRIOS_ARAPP_V2_SIG_OK) {
        (void)fprintf(stderr, "[ERROR] Failed to sign arapp package (rc=%d)\n", rc);
        free(pkg_data);
        return 1;
    }

    if (write_file(out_file, pkg_data, pkg_len) != 0) {
        (void)fprintf(stderr, "[ERROR] Failed to write signed package to %s\n", out_file);
        free(pkg_data);
        return 1;
    }

    free(pkg_data);
    (void)printf("[OK] Package signed successfully: %s\n", out_file);
    return 0;
}
