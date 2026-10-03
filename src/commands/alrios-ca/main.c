/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/pki/issuer.h"
#include <openssl/pem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(void) {
    printf("ALRIOS Sovereign Certificate Authority (alrios-ca)\n");
    printf("Usage:\n");
    printf("  alrios-ca root <subject> <days> <out_cert.bin> <out_key.pem>\n");
    printf("  alrios-ca intermediate <subject> <days> <root_cert.bin> <root_key.pem> <out_cert.bin> <out_key.pem>\n");
    printf("  alrios-ca leaf <subject> <days> <inter_cert.bin> <inter_key.pem> <out_cert.bin> <out_key.pem>\n");
}

static int read_certificate(const char *path, alrios_certificate_t *out_cert) {
    FILE *file;
    long length;
    uint8_t *data;
    size_t read_bytes;
    int result;

    file = fopen(path, "rb");
    if (!file) {
        return -1;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) <= 0 ||
        length > (long)ALRIOS_CERTIFICATE_WIRE_MAX || fseek(file, 0, SEEK_SET) != 0) {
        (void)fclose(file);
        return -1;
    }
    data = malloc((size_t)length);
    if (!data) {
        (void)fclose(file);
        return -1;
    }
    read_bytes = fread(data, 1, (size_t)length, file);
    (void)fclose(file);
    result = read_bytes == (size_t)length ? alrios_certificate_decode(data, (size_t)length, out_cert) : -1;
    free(data);
    return result == ALRIOS_CERTIFICATE_OK ? 0 : -1;
}

static int write_certificate_and_key(const char *certificate_path,
                                     const char *key_path,
                                     const uint8_t *wire,
                                     size_t wire_len,
                                     EVP_PKEY *key) {
    FILE *certificate_file;
    FILE *key_file;

    certificate_file = fopen(certificate_path, "wb");
    if (!certificate_file || fwrite(wire, 1, wire_len, certificate_file) != wire_len ||
        fclose(certificate_file) != 0) {
        if (certificate_file) {
            (void)fclose(certificate_file);
        }
        return -1;
    }
    key_file = fopen(key_path, "wb");
    if (!key_file || PEM_write_PrivateKey(key_file, key, NULL, NULL, 0, NULL, NULL) != 1 ||
        fclose(key_file) != 0) {
        if (key_file) {
            (void)fclose(key_file);
        }
        (void)remove(certificate_path);
        return -1;
    }
    return 0;
}

static EVP_PKEY *read_private_key(const char *path) {
    FILE *file = fopen(path, "rb");
    EVP_PKEY *key;

    if (!file) {
        return NULL;
    }
    key = PEM_read_PrivateKey(file, NULL, NULL, NULL);
    (void)fclose(file);
    return key;
}

int main(int argc, char **argv) {
    const char *command;
    const char *subject;
    uint64_t days;
    EVP_PKEY *issuer_key = NULL;
    EVP_PKEY *issued_key = NULL;
    alrios_certificate_t issuer_cert;
    alrios_certificate_t issued_cert;
    uint8_t wire[ALRIOS_CERTIFICATE_WIRE_MAX];
    size_t wire_len = 0U;
    int result;

    if (argc < 2) {
        print_usage();
        return 1;
    }
    command = argv[1];
    if (strcmp(command, "root") == 0) {
        if (argc != 6) {
            print_usage();
            return 1;
        }
        subject = argv[2];
        days = (uint64_t)strtoull(argv[3], NULL, 10);
        result = alrios_pki_issue_root(subject, days, &issued_key, &issued_cert,
                                       wire, sizeof(wire), &wire_len);
        if (result == ALRIOS_ISSUER_OK) {
            result = write_certificate_and_key(argv[4], argv[5], wire, wire_len, issued_key);
        }
    } else if (strcmp(command, "intermediate") == 0) {
        if (argc != 8 || read_certificate(argv[4], &issuer_cert) != 0) {
            print_usage();
            return 1;
        }
        issuer_key = read_private_key(argv[5]);
        if (!issuer_key) {
            fprintf(stderr, "Error: Failed to load Root private key\n");
            return 1;
        }
        subject = argv[2];
        days = (uint64_t)strtoull(argv[3], NULL, 10);
        result = alrios_pki_issue_intermediate(subject, days, &issuer_cert, issuer_key,
                                               &issued_key, &issued_cert, wire,
                                               sizeof(wire), &wire_len);
        if (result == ALRIOS_ISSUER_OK) {
            result = write_certificate_and_key(argv[6], argv[7], wire, wire_len, issued_key);
        }
    } else if (strcmp(command, "leaf") == 0) {
        if (argc != 8 || read_certificate(argv[4], &issuer_cert) != 0) {
            print_usage();
            return 1;
        }
        issuer_key = read_private_key(argv[5]);
        if (!issuer_key) {
            fprintf(stderr, "Error: Failed to load Intermediate private key\n");
            return 1;
        }
        subject = argv[2];
        days = (uint64_t)strtoull(argv[3], NULL, 10);
        result = alrios_pki_issue_leaf(subject, days, &issuer_cert, issuer_key,
                                       &issued_key, &issued_cert, wire,
                                       sizeof(wire), &wire_len);
        if (result == ALRIOS_ISSUER_OK) {
            result = write_certificate_and_key(argv[6], argv[7], wire, wire_len, issued_key);
        }
    } else {
        print_usage();
        return 1;
    }

    EVP_PKEY_free(issuer_key);
    EVP_PKEY_free(issued_key);
    if (result != ALRIOS_ISSUER_OK && result != 0) {
        fprintf(stderr, "Error: Certificate issuance failed (%d)\n", result);
        return 1;
    }
    printf("[OK] %s certificate and private key issued successfully\n", command);
    return 0;
}
