/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/pki/issuer.h"
#include "alrios/pki/notary.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/pem.h>

static void print_usage(void) {
    printf("ALRIOS Sovereign Certificate Authority (alrios-ca)\n");
    printf("Usage:\n");
    printf("  alrios-ca root <subject> <days> <out_cert.bin> <out_key.pem>\n");
    printf("  alrios-ca intermediate <subject> <days> <root_cert.bin> <root_key.pem> <out_cert.bin> <out_key.pem>\n");
    printf("  alrios-ca leaf <subject> <days> <inter_cert.bin> <inter_key.pem> <out_cert.bin> <out_key.pem>\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "root") == 0) {
        if (argc < 6) { print_usage(); return 1; }
        const char *subject = argv[2];
        uint64_t days = (uint64_t)atoll(argv[3]);
        const char *out_cert_path = argv[4];
        const char *out_key_path = argv[5];

        EVP_PKEY *pkey = NULL;
        alrios_certificate_t cert;
        uint8_t wire[ALRIOS_CERTIFICATE_WIRE_MAX];
        size_t wire_len = 0;

        int rc = alrios_pki_issue_root(subject, days, &pkey, &cert, wire, sizeof(wire), &wire_len);
        if (rc != ALRIOS_ISSUER_OK) {
            fprintf(stderr, "Error: Failed to issue Root CA (%d)\n", rc);
            return 1;
        }

        FILE *f = fopen(out_cert_path, "wb");
        if (!f || fwrite(wire, 1, wire_len, f) != wire_len) {
            fprintf(stderr, "Error: Failed to write certificate to %s\n", out_cert_path);
            if (f) fclose(f);
            EVP_PKEY_free(pkey);
            return 1;
        }
        fclose(f);

        FILE *kf = fopen(out_key_path, "wb");
        if (!kf || PEM_write_PrivateKey(kf, pkey, NULL, NULL, 0, NULL, NULL) <= 0) {
            fprintf(stderr, "Error: Failed to write private key to %s\n", out_key_path);
            if (kf) fclose(kf);
            EVP_PKEY_free(pkey);
            return 1;
        }
        fclose(kf);

        EVP_PKEY_free(pkey);
        printf("[OK] Root CA certificate issued: %s (%zu bytes)\n", out_cert_path, wire_len);
        printf("[OK] Root CA private key saved: %s\n", out_key_path);
        return 0;
    }

    if (strcmp(cmd, "intermediate") == 0) {
        if (argc < 8) { print_usage(); return 1; }
        const char *subject = argv[2];
        uint64_t days = (uint64_t)atoll(argv[3]);
        const char *root_cert_path = argv[4];
        const char *root_key_path = argv[5];
        const char *out_cert_path = argv[6];
        const char *out_key_path = argv[7];

        uint8_t root_wire[ALRIOS_CERTIFICATE_WIRE_MAX];
        FILE *rf = fopen(root_cert_path, "rb");
        if (!rf) {
            fprintf(stderr, "Error: Failed to open root cert: %s\n", root_cert_path);
            return 1;
        }
        size_t rf_len = fread(root_wire, 1, sizeof(root_wire), rf);
        fclose(rf);

        alrios_certificate_t root_cert;
        if (alrios_certificate_decode(root_wire, rf_len, &root_cert) != 0) {
            fprintf(stderr, "Error: Failed to decode root cert %s\n", root_cert_path);
            return 1;
        }

        EVP_PKEY *root_pkey = NULL;
        if (alrios_notary_load_private_key_file(root_key_path, &root_pkey) != 0 || !root_pkey) {
            fprintf(stderr, "Error: Failed to load root private key: %s\n", root_key_path);
            return 1;
        }

        EVP_PKEY *inter_pkey = NULL;
        alrios_certificate_t inter_cert;
        uint8_t wire[ALRIOS_CERTIFICATE_WIRE_MAX];
        size_t wire_len = 0;

        int rc = alrios_pki_issue_intermediate(subject, days, &root_cert, root_pkey,
                                               &inter_pkey, &inter_cert, wire, sizeof(wire), &wire_len);
        EVP_PKEY_free(root_pkey);
        if (rc != ALRIOS_ISSUER_OK) {
            fprintf(stderr, "Error: Failed to issue Intermediate CA (%d)\n", rc);
            return 1;
        }

        FILE *f = fopen(out_cert_path, "wb");
        if (!f || fwrite(wire, 1, wire_len, f) != wire_len) {
            fprintf(stderr, "Error: Failed to write certificate to %s\n", out_cert_path);
            if (f) fclose(f);
            EVP_PKEY_free(inter_pkey);
            return 1;
        }
        fclose(f);

        FILE *kf = fopen(out_key_path, "wb");
        if (!kf || PEM_write_PrivateKey(kf, inter_pkey, NULL, NULL, 0, NULL, NULL) <= 0) {
            fprintf(stderr, "Error: Failed to write private key to %s\n", out_key_path);
            if (kf) fclose(kf);
            EVP_PKEY_free(inter_pkey);
            return 1;
        }
        fclose(kf);

        EVP_PKEY_free(inter_pkey);
        printf("[OK] Intermediate CA certificate issued: %s (%zu bytes)\n", out_cert_path, wire_len);
        printf("[OK] Intermediate CA private key saved: %s\n", out_key_path);
        return 0;
    }

    if (strcmp(cmd, "leaf") == 0) {
        if (argc < 8) { print_usage(); return 1; }
        const char *subject = argv[2];
        uint64_t days = (uint64_t)atoll(argv[3]);
        const char *inter_cert_path = argv[4];
        const char *inter_key_path = argv[5];
        const char *out_cert_path = argv[6];
        const char *out_key_path = argv[7];

        uint8_t inter_wire[ALRIOS_CERTIFICATE_WIRE_MAX];
        FILE *rf = fopen(inter_cert_path, "rb");
        if (!rf) {
            fprintf(stderr, "Error: Failed to open intermediate cert: %s\n", inter_cert_path);
            return 1;
        }
        size_t rf_len = fread(inter_wire, 1, sizeof(inter_wire), rf);
        fclose(rf);

        alrios_certificate_t inter_cert;
        if (alrios_certificate_decode(inter_wire, rf_len, &inter_cert) != 0) {
            fprintf(stderr, "Error: Failed to decode intermediate cert %s\n", inter_cert_path);
            return 1;
        }

        EVP_PKEY *inter_pkey = NULL;
        if (alrios_notary_load_private_key_file(inter_key_path, &inter_pkey) != 0 || !inter_pkey) {
            fprintf(stderr, "Error: Failed to load intermediate private key: %s\n", inter_key_path);
            return 1;
        }

        EVP_PKEY *leaf_pkey = NULL;
        alrios_certificate_t leaf_cert;
        uint8_t wire[ALRIOS_CERTIFICATE_WIRE_MAX];
        size_t wire_len = 0;

        int rc = alrios_pki_issue_leaf(subject, days, &inter_cert, inter_pkey,
                                       &leaf_pkey, &leaf_cert, wire, sizeof(wire), &wire_len);
        EVP_PKEY_free(inter_pkey);
        if (rc != ALRIOS_ISSUER_OK) {
            fprintf(stderr, "Error: Failed to issue Leaf certificate (%d)\n", rc);
            return 1;
        }

        FILE *f = fopen(out_cert_path, "wb");
        if (!f || fwrite(wire, 1, wire_len, f) != wire_len) {
            fprintf(stderr, "Error: Failed to write certificate to %s\n", out_cert_path);
            if (f) fclose(f);
            EVP_PKEY_free(leaf_pkey);
            return 1;
        }
        fclose(f);

        FILE *kf = fopen(out_key_path, "wb");
        if (!kf || PEM_write_PrivateKey(kf, leaf_pkey, NULL, NULL, 0, NULL, NULL) <= 0) {
            fprintf(stderr, "Error: Failed to write private key to %s\n", out_key_path);
            if (kf) fclose(kf);
            EVP_PKEY_free(leaf_pkey);
            return 1;
        }
        fclose(kf);

        EVP_PKEY_free(leaf_pkey);
        printf("[OK] Leaf certificate issued: %s (%zu bytes)\n", out_cert_path, wire_len);
        printf("[OK] Leaf private key saved: %s\n", out_key_path);
        return 0;
    }

    print_usage();
    return 1;
}
