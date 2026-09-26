/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/pki/issuer.h"
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
        EVP_PKEY_free(pkey);
        printf("[OK] Root CA certificate issued: %s (%zu bytes)\n", out_cert_path, wire_len);
        return 0;
    }

    print_usage();
    return 1;
}
