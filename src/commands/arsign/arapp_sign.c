/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/pki/notary.h"
#include "alrios/pki/issuer.h"
#include "alrios/package/arapp_v2.h"
#include "alrios/crypto_verify.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int read_entire_file_arsign(const char *path, uint8_t **out_data, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    long sz;
    size_t read_bytes;
    uint8_t *buf;

    if (!path || !out_data || !out_len) return -1;
    *out_data = NULL;
    *out_len = 0U;
    if (!f) return -1;

    if (fseek(f, 0, SEEK_END) != 0) {
        (void)fclose(f);
        return -1;
    }
    sz = ftell(f);
    if (sz < 0 || sz > (long)(256U * 1024U * 1024U)) {
        (void)fclose(f);
        return -1;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        (void)fclose(f);
        return -1;
    }
    if (sz == 0) {
        (void)fclose(f);
        return 0;
    }
    buf = malloc((size_t)sz);
    if (!buf) {
        (void)fclose(f);
        return -1;
    }
    read_bytes = fread(buf, 1, (size_t)sz, f);
    (void)fclose(f);
    if (read_bytes != (size_t)sz) {
        free(buf);
        return -1;
    }
    *out_data = buf;
    *out_len = (size_t)sz;
    return 0;
}

int arsign_handle_sign_arapp(int argc, char **argv) {
    const char *in_file = NULL;
    const char *out_file = NULL;
    const char *key_file = NULL;
    const char *ml_key_file = NULL;
    int i;
    uint8_t *file_data = NULL;
    size_t file_len = 0U;
    arapp_v2_package_t pkg;
    EVP_PKEY *privkey = NULL;
    uint8_t ml_priv[ML_DSA_65_SECRET_KEY_LEN];
    arapp_v2_signatures_t sigs;
    uint8_t *out_buf = NULL;
    size_t out_written = 0U;
    FILE *f_out = NULL;
    int rc;

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--in") == 0 && i + 1 < argc) {
            in_file = argv[++i];
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out_file = argv[++i];
        } else if (strcmp(argv[i], "--key") == 0 && i + 1 < argc) {
            key_file = argv[++i];
        } else if (strcmp(argv[i], "--ml-key") == 0 && i + 1 < argc) {
            ml_key_file = argv[++i];
        }
    }

    if (!in_file || !out_file || !key_file) {
        (void)fprintf(stderr, "[ERROR] Missing arguments for sign-arapp\n");
        return 1;
    }

    if (read_entire_file_arsign(in_file, &file_data, &file_len) != 0) {
        (void)fprintf(stderr, "[ERROR] Failed to read input .arapp file: %s\n", in_file);
        return 1;
    }

    rc = arapp_v2_read(file_data, file_len, &pkg);
    if (rc != ARAPP_V2_OK) {
        (void)fprintf(stderr, "[ERROR] Invalid .arapp v2 package header (rc=%d)\n", rc);
        free(file_data);
        return 1;
    }

    rc = alrios_notary_load_private_key_file(key_file, &privkey);
    if (rc != ALRIOS_NOTARY_OK || !privkey) {
        (void)fprintf(stderr, "[ERROR] Failed to load Ed25519 private key from %s\n", key_file);
        free(file_data);
        return 1;
    }

    if (ml_key_file) {
        uint8_t *ml_data = NULL;
        size_t ml_len = 0U;
        if (read_entire_file_arsign(ml_key_file, &ml_data, &ml_len) != 0 || ml_len != ML_DSA_65_SECRET_KEY_LEN) {
            (void)fprintf(stderr, "[ERROR] Failed to load ML-DSA-65 secret key from %s\n", ml_key_file);
            EVP_PKEY_free(privkey);
            free(file_data);
            free(ml_data);
            return 1;
        }
        memcpy(ml_priv, ml_data, ML_DSA_65_SECRET_KEY_LEN);
        free(ml_data);
    } else {
        memset(ml_priv, 0, sizeof(ml_priv));
    }

    rc = arapp_v2_sign(&pkg, privkey, ml_key_file ? ml_priv : NULL, &sigs);
    EVP_PKEY_free(privkey);
    if (rc != ARAPP_V2_OK) {
        (void)fprintf(stderr, "[ERROR] Failed to sign .arapp v2 package (rc=%d)\n", rc);
        free(file_data);
        return 1;
    }

    out_buf = malloc(file_len + ARAPP_V2_SIGNATURES_SIZE);
    if (!out_buf) {
        free(file_data);
        return 1;
    }

    rc = arapp_v2_build(&pkg.prefix, &sigs, pkg.manifest_json, pkg.manifest_len, pkg.ciphertext, pkg.ciphertext_len, out_buf, file_len + ARAPP_V2_SIGNATURES_SIZE, &out_written);
    free(file_data);
    if (rc != ARAPP_V2_OK) {
        (void)fprintf(stderr, "[ERROR] Failed to build signed .arapp v2 package (rc=%d)\n", rc);
        free(out_buf);
        return 1;
    }

    f_out = fopen(out_file, "wb");
    if (!f_out || fwrite(out_buf, 1, out_written, f_out) != out_written) {
        (void)fprintf(stderr, "[ERROR] Failed to write signed .arapp to %s\n", out_file);
        if (f_out) (void)fclose(f_out);
        free(out_buf);
        return 1;
    }
    (void)fclose(f_out);
    free(out_buf);

    (void)printf("[OK] Signed .arapp v2 package written to %s (%zu bytes)\n", out_file, out_written);
    return 0;
}

int arsign_handle_verify_arapp(int argc, char **argv) {
    const char *in_file = NULL;
    const char *leaf_file = NULL;
    const char *inter_file = NULL;
    const char *root_file = NULL;
    const char *ml_pub_file = NULL;
    int i;
    uint8_t *file_data = NULL;
    size_t file_len = 0U;
    uint8_t *leaf_data = NULL; size_t leaf_len = 0U;
    uint8_t *inter_data = NULL; size_t inter_len = 0U;
    uint8_t *root_data = NULL; size_t root_len = 0U;
    alrios_certificate_t leaf_cert;
    alrios_certificate_t inter_cert;
    alrios_certificate_t root_cert;
    alrios_trust_store_t store;
    arapp_v2_package_t pkg;
    uint8_t ml_pub[ML_DSA_65_PUBLIC_KEY_LEN];
    const uint8_t *ml_pub_ptr = NULL;
    int rc;

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--in") == 0 && i + 1 < argc) {
            in_file = argv[++i];
        } else if (strcmp(argv[i], "--leaf") == 0 && i + 1 < argc) {
            leaf_file = argv[++i];
        } else if (strcmp(argv[i], "--inter") == 0 && i + 1 < argc) {
            inter_file = argv[++i];
        } else if (strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
            root_file = argv[++i];
        } else if (strcmp(argv[i], "--ml-pub") == 0 && i + 1 < argc) {
            ml_pub_file = argv[++i];
        }
    }

    if (!in_file || !leaf_file || !inter_file || !root_file) {
        (void)fprintf(stderr, "[ERROR] Missing arguments for verify-arapp\n");
        return 1;
    }

    if (read_entire_file_arsign(in_file, &file_data, &file_len) != 0 ||
        read_entire_file_arsign(leaf_file, &leaf_data, &leaf_len) != 0 ||
        read_entire_file_arsign(inter_file, &inter_data, &inter_len) != 0 ||
        read_entire_file_arsign(root_file, &root_data, &root_len) != 0) {
        (void)fprintf(stderr, "[ERROR] Failed to read input files for verify-arapp\n");
        free(file_data); free(leaf_data); free(inter_data); free(root_data);
        return 1;
    }

    if (ml_pub_file) {
        uint8_t *ml_data = NULL;
        size_t ml_len = 0U;
        if (read_entire_file_arsign(ml_pub_file, &ml_data, &ml_len) != 0 || ml_len != ML_DSA_65_PUBLIC_KEY_LEN) {
            (void)fprintf(stderr, "[ERROR] Failed to load ML-DSA-65 public key from %s\n", ml_pub_file);
            free(file_data); free(leaf_data); free(inter_data); free(root_data);
            free(ml_data);
            return 1;
        }
        memcpy(ml_pub, ml_data, ML_DSA_65_PUBLIC_KEY_LEN);
        free(ml_data);
        ml_pub_ptr = ml_pub;
    }

    if (alrios_certificate_decode(leaf_data, leaf_len, &leaf_cert) != ALRIOS_CERTIFICATE_OK ||
        alrios_certificate_decode(inter_data, inter_len, &inter_cert) != ALRIOS_CERTIFICATE_OK ||
        alrios_certificate_decode(root_data, root_len, &root_cert) != ALRIOS_CERTIFICATE_OK) {
        (void)fprintf(stderr, "[ERROR] Failed to decode certificate chain\n");
        free(file_data); free(leaf_data); free(inter_data); free(root_data);
        return 1;
    }
    free(leaf_data); free(inter_data); free(root_data);

    alrios_trust_store_init(&store);
    if (alrios_trust_store_add_anchor(&store, &root_cert) != ALRIOS_CERTIFICATE_OK ||
        alrios_trust_store_seal(&store) != ALRIOS_CERTIFICATE_OK) {
        (void)fprintf(stderr, "[ERROR] Failed to seal trust store\n");
        free(file_data);
        alrios_trust_store_clear(&store);
        return 1;
    }

    rc = arapp_v2_verify_package_with_chain(file_data, file_len, &leaf_cert, &inter_cert, &store, ml_pub_ptr, (uint64_t)time(NULL), &pkg);
    free(file_data);
    alrios_trust_store_clear(&store);

    if (rc != ARAPP_V2_OK) {
        (void)fprintf(stderr, "[REJECTED] .arapp v2 signature or certificate chain verification failed (rc=%d)\n", rc);
        return 1;
    }

    (void)printf("[OK] .arapp v2 package verified successfully with valid certificate chain\n");
    return 0;
}
