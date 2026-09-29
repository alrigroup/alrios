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
#include "alrios/crypto_verify.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int arsign_handle_sign_arapp(int argc, char **argv);
int arsign_handle_verify_arapp(int argc, char **argv);

static void print_usage(void) {
    (void)printf("ALRIOS Isolated Cryptographic Notary (arsign)\n");
    (void)printf("Usage:\n");
    (void)printf("  arsign sign --key <key_file> --in <file> --out <sig_file> [--policy <default|strict>]\n");
    (void)printf("  arsign verify --key <key_file_or_cert> --in <file> --sig <sig_file> [--policy <default|strict>]\n");
    (void)printf("  arsign verify-chain --leaf <leaf.bin> --inter <inter.bin> --root <root.bin> --in <file> --sig <sig_file>\n");
    (void)printf("  arsign sign-arapp --in <file.arapp> --out <signed.arapp> --key <ed_key> [--ml-key <ml_key>]\n");
    (void)printf("  arsign verify-arapp --in <signed.arapp> --leaf <leaf.bin> --inter <inter.bin> --root <root.bin> [--ml-pub <ml_pub_key>]\n");
    (void)printf("  arsign status\n");
}

static int read_entire_file(const char *path, uint8_t **out_data, size_t *out_len) {
    FILE *f = NULL;
    long sz;
    uint8_t *buf = NULL;
    size_t read_bytes;

    if (!path || !out_data || !out_len) {
        return ALRIOS_NOTARY_ERR_INVALID_ARGUMENT;
    }
    *out_data = NULL;
    *out_len = 0U;

    f = fopen(path, "rb");
    if (!f) {
        return ALRIOS_NOTARY_ERR_IO_FAILED;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        (void)fclose(f);
        return ALRIOS_NOTARY_ERR_IO_FAILED;
    }
    sz = ftell(f);
    if (sz < 0 || sz > (long)ALRIOS_NOTARY_MAX_PAYLOAD_SIZE) {
        (void)fclose(f);
        return ALRIOS_NOTARY_ERR_CAPACITY;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        (void)fclose(f);
        return ALRIOS_NOTARY_ERR_IO_FAILED;
    }

    if (sz == 0) {
        (void)fclose(f);
        *out_data = NULL;
        *out_len = 0U;
        return ALRIOS_NOTARY_OK;
    }

    buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) {
        (void)fclose(f);
        return ALRIOS_NOTARY_ERR_CAPACITY;
    }

    read_bytes = fread(buf, 1, (size_t)sz, f);
    (void)fclose(f);

    if (read_bytes != (size_t)sz) {
        free(buf);
        return ALRIOS_NOTARY_ERR_IO_FAILED;
    }

    *out_data = buf;
    *out_len = (size_t)sz;
    return ALRIOS_NOTARY_OK;
}

static int is_compiler_argument(const char *arg) {
    if (!arg) return 0;
    if (strcmp(arg, "--compile") == 0 ||
        strcmp(arg, "-c") == 0 ||
        strcmp(arg, "-S") == 0 ||
        strcmp(arg, "--cc") == 0 ||
        strcmp(arg, "--compiler") == 0 ||
        strcmp(arg, "gcc") == 0 ||
        strcmp(arg, "clang") == 0 ||
        strcmp(arg, "cc") == 0 ||
        strcmp(arg, "arcc") == 0 ||
        strcmp(arg, "make") == 0 ||
        strcmp(arg, "ld") == 0) {
        return 1;
    }
    return 0;
}

static int parse_policy_flag(const char *policy_str, uint32_t *out_flags) {
    if (!out_flags) {
        return ALRIOS_NOTARY_ERR_INVALID_ARGUMENT;
    }
    if (!policy_str || strcmp(policy_str, "default") == 0) {
        *out_flags = ALRIOS_NOTARY_POLICY_DEFAULT;
        return ALRIOS_NOTARY_OK;
    }
    if (strcmp(policy_str, "strict") == 0) {
        *out_flags = ALRIOS_NOTARY_POLICY_STRICT;
        return ALRIOS_NOTARY_OK;
    }
    if (strcmp(policy_str, "pqc") == 0 || strcmp(policy_str, "hybrid") == 0 ||
        strcmp(policy_str, "ml-dsa-65") == 0 || strcmp(policy_str, "mldsa65") == 0) {
        return ALRIOS_NOTARY_ERR_POLICY_VIOLATION;
    }
    return ALRIOS_NOTARY_ERR_POLICY_VIOLATION;
}

static int next_arg_is_value(int argc, char **argv, int index) {
    if (!argv || index < 0 || index + 1 >= argc || !argv[index + 1]) {
        return 0;
    }
    if (argv[index + 1][0] == '\0' || strncmp(argv[index + 1], "--", 2U) == 0) {
        return 0;
    }
    return 1;
}

static int set_option_once(const char **slot, const char *value, const char *name) {
    if (!slot || !value || !name) {
        return ALRIOS_NOTARY_ERR_INVALID_ARGUMENT;
    }
    if (*slot) {
        (void)fprintf(stderr, "[ERROR] Duplicate option rejected: %s\n", name);
        return ALRIOS_NOTARY_ERR_POLICY_VIOLATION;
    }
    *slot = value;
    return ALRIOS_NOTARY_OK;
}

static int reject_missing_value(const char *name) {
    (void)fprintf(stderr, "[ERROR] Missing value for option: %s\n", name ? name : "<unknown>");
    return ALRIOS_NOTARY_ERR_INVALID_ARGUMENT;
}

static int reject_unknown_argument(const char *arg) {
    (void)fprintf(stderr, "[ERROR] Unknown or malformed argument rejected: %s\n", arg ? arg : "<null>");
    return ALRIOS_NOTARY_ERR_INVALID_ARGUMENT;
}

int main(int argc, char **argv) {
    int i;
    const char *cmd;
    int confinement_rc;

    if (argc < 2) {
        print_usage();
        return 1;
    }

    confinement_rc = alrios_notary_enable_noexec_confinement();
    if (confinement_rc != 0) {
        (void)fprintf(stderr,
                      "[SECURITY ERROR] arsign failed to enter no-exec confinement (rc=%d)\n",
                      confinement_rc);
        return 2;
    }

    /* Strict check: arsign is strictly prohibited from executing compilers */
    for (i = 1; i < argc; ++i) {
        if (is_compiler_argument(argv[i])) {
            (void)fprintf(stderr,
                          "[SECURITY ERROR] arsign is an isolated notary and cannot invoke compilers (%s rejected)\n",
                          argv[i]);
            return 2;
        }
    }

    cmd = argv[1];

    if (strcmp(cmd, "sign-arapp") == 0) {
        return arsign_handle_sign_arapp(argc, argv);
    }
    if (strcmp(cmd, "verify-arapp") == 0) {
        return arsign_handle_verify_arapp(argc, argv);
    }

    if (strcmp(cmd, "status") == 0) {
        (void)printf("arsign: Sovereign Cryptographic Notary\n");
        (void)printf("  compiler_invocation_policy: noexec confinement required before command dispatch\n");
        (void)printf("  armake_key_access_policy: enforced by armake manifest path validation and build graph gates\n");
        (void)printf("  algorithm_policy: Ed25519 (FIPS 186-5 / RFC 8032); PQC/hybrid fail-closed until MP-004\n");
        return 0;
    }

    if (strcmp(cmd, "sign") == 0) {
        const char *key_file = NULL;
        const char *in_file = NULL;
        const char *out_file = NULL;
        const char *policy_name = NULL;
        EVP_PKEY *privkey = NULL;
        uint8_t *payload = NULL;
        size_t payload_len = 0U;
        uint32_t policy_flags = ALRIOS_NOTARY_POLICY_DEFAULT;
        alrios_notary_policy_t policy;
        alrios_notary_signature_t sig;
        uint8_t wire[ALRIOS_NOTARY_WIRE_MAX];
        size_t wire_len = 0U;
        FILE *f_out = NULL;
        int rc;

        for (i = 2; i < argc; ++i) {
            if (strcmp(argv[i], "--key") == 0) {
                if (!next_arg_is_value(argc, argv, i)) return reject_missing_value(argv[i]);
                rc = set_option_once(&key_file, argv[++i], "--key");
                if (rc != ALRIOS_NOTARY_OK) return 1;
            } else if (strcmp(argv[i], "--in") == 0) {
                if (!next_arg_is_value(argc, argv, i)) return reject_missing_value(argv[i]);
                rc = set_option_once(&in_file, argv[++i], "--in");
                if (rc != ALRIOS_NOTARY_OK) return 1;
            } else if (strcmp(argv[i], "--out") == 0) {
                if (!next_arg_is_value(argc, argv, i)) return reject_missing_value(argv[i]);
                rc = set_option_once(&out_file, argv[++i], "--out");
                if (rc != ALRIOS_NOTARY_OK) return 1;
            } else if (strcmp(argv[i], "--policy") == 0) {
                if (!next_arg_is_value(argc, argv, i)) return reject_missing_value(argv[i]);
                rc = set_option_once(&policy_name, argv[++i], "--policy");
                if (rc != ALRIOS_NOTARY_OK) return 1;
            } else {
                return reject_unknown_argument(argv[i]);
            }
        }

        rc = parse_policy_flag(policy_name, &policy_flags);
        if (rc != ALRIOS_NOTARY_OK || alrios_notary_policy_init(&policy, policy_flags) != ALRIOS_NOTARY_OK) {
            (void)fprintf(stderr, "[ERROR] Invalid or unavailable signing policy: %s\n",
                          policy_name ? policy_name : "default");
            return 1;
        }

        if (!key_file || !in_file || !out_file) {
            (void)fprintf(stderr, "[ERROR] Missing required arguments for 'sign'\n");
            print_usage();
            return 1;
        }

        rc = alrios_notary_load_private_key_file(key_file, &privkey);
        if (rc != ALRIOS_NOTARY_OK || !privkey) {
            (void)fprintf(stderr, "[ERROR] Failed to load signing key from %s (rc=%d)\n", key_file, rc);
            return 1;
        }

        rc = read_entire_file(in_file, &payload, &payload_len);
        if (rc != ALRIOS_NOTARY_OK) {
            (void)fprintf(stderr, "[ERROR] Failed to read input payload from %s (rc=%d)\n", in_file, rc);
            EVP_PKEY_free(privkey);
            return 1;
        }

        rc = alrios_notary_sign_payload(&policy, payload, payload_len, privkey, &sig);
        EVP_PKEY_free(privkey);
        free(payload);

        if (rc != ALRIOS_NOTARY_OK) {
            (void)fprintf(stderr, "[ERROR] Failed to sign payload (rc=%d)\n", rc);
            return 1;
        }

        rc = alrios_notary_signature_encode(&sig, wire, sizeof(wire), &wire_len);
        if (rc != ALRIOS_NOTARY_OK) {
            (void)fprintf(stderr, "[ERROR] Failed to encode signature wire (rc=%d)\n", rc);
            return 1;
        }

        f_out = fopen(out_file, "wb");
        if (!f_out || fwrite(wire, 1, wire_len, f_out) != wire_len) {
            (void)fprintf(stderr, "[ERROR] Failed to write signature to %s\n", out_file);
            if (f_out) (void)fclose(f_out);
            return 1;
        }
        if (fflush(f_out) != 0) {
            (void)fprintf(stderr, "[ERROR] Failed to flush signature to %s\n", out_file);
            (void)fclose(f_out);
            return 1;
        }
        if (fclose(f_out) != 0) {
            (void)fprintf(stderr, "[ERROR] Failed to close signature output %s\n", out_file);
            return 1;
        }

        (void)printf("[OK] Artifact signed successfully: %s (%zu bytes)\n", out_file, wire_len);
        return 0;
    }

    if (strcmp(cmd, "verify") == 0) {
        const char *key_file = NULL;
        const char *in_file = NULL;
        const char *sig_file = NULL;
        const char *policy_name = NULL;
        uint8_t pubkey[ALRIOS_CERTIFICATE_ED25519_KEY_SIZE];
        uint8_t *payload = NULL;
        size_t payload_len = 0U;
        uint8_t *wire = NULL;
        size_t wire_len = 0U;
        uint32_t policy_flags = ALRIOS_NOTARY_POLICY_DEFAULT;
        alrios_notary_policy_t policy;
        alrios_notary_signature_t sig;
        int rc;

        for (i = 2; i < argc; ++i) {
            if (strcmp(argv[i], "--key") == 0) {
                if (!next_arg_is_value(argc, argv, i)) return reject_missing_value(argv[i]);
                rc = set_option_once(&key_file, argv[++i], "--key");
                if (rc != ALRIOS_NOTARY_OK) return 1;
            } else if (strcmp(argv[i], "--in") == 0) {
                if (!next_arg_is_value(argc, argv, i)) return reject_missing_value(argv[i]);
                rc = set_option_once(&in_file, argv[++i], "--in");
                if (rc != ALRIOS_NOTARY_OK) return 1;
            } else if (strcmp(argv[i], "--sig") == 0) {
                if (!next_arg_is_value(argc, argv, i)) return reject_missing_value(argv[i]);
                rc = set_option_once(&sig_file, argv[++i], "--sig");
                if (rc != ALRIOS_NOTARY_OK) return 1;
            } else if (strcmp(argv[i], "--policy") == 0) {
                if (!next_arg_is_value(argc, argv, i)) return reject_missing_value(argv[i]);
                rc = set_option_once(&policy_name, argv[++i], "--policy");
                if (rc != ALRIOS_NOTARY_OK) return 1;
            } else {
                return reject_unknown_argument(argv[i]);
            }
        }

        rc = parse_policy_flag(policy_name, &policy_flags);
        if (rc != ALRIOS_NOTARY_OK || alrios_notary_policy_init(&policy, policy_flags) != ALRIOS_NOTARY_OK) {
            (void)fprintf(stderr, "[ERROR] Invalid or unavailable verification policy: %s\n",
                          policy_name ? policy_name : "default");
            return 1;
        }

        if (!key_file || !in_file || !sig_file) {
            (void)fprintf(stderr, "[ERROR] Missing required arguments for 'verify'\n");
            print_usage();
            return 1;
        }

        rc = alrios_notary_load_public_key_file(key_file, pubkey);
        if (rc != ALRIOS_NOTARY_OK) {
            (void)fprintf(stderr, "[ERROR] Failed to load public key from %s (rc=%d)\n", key_file, rc);
            return 1;
        }

        rc = read_entire_file(in_file, &payload, &payload_len);
        if (rc != ALRIOS_NOTARY_OK) {
            (void)fprintf(stderr, "[ERROR] Failed to read input file %s (rc=%d)\n", in_file, rc);
            return 1;
        }

        rc = read_entire_file(sig_file, &wire, &wire_len);
        if (rc != ALRIOS_NOTARY_OK) {
            (void)fprintf(stderr, "[ERROR] Failed to read signature file %s (rc=%d)\n", sig_file, rc);
            free(payload);
            return 1;
        }

        rc = alrios_notary_signature_decode(wire, wire_len, &sig);
        free(wire);
        if (rc != ALRIOS_NOTARY_OK) {
            (void)fprintf(stderr, "[ERROR] Failed to decode signature wire format (rc=%d)\n", rc);
            free(payload);
            return 1;
        }

        if ((policy.flags & ALRIOS_NOTARY_POLICY_REQUIRE_CERT_BIND) != 0U) {
            (void)fprintf(stderr,
                          "[REJECTED] Strict policy requires certificate-chain verification; raw public keys are insufficient\n");
            free(payload);
            return 1;
        }

        rc = alrios_notary_verify_payload(&policy, payload, payload_len, &sig, pubkey);
        free(payload);

        if (rc != ALRIOS_NOTARY_OK) {
            (void)fprintf(stderr, "[REJECTED] Signature verification failed (tampered payload or invalid signature, rc=%d)\n", rc);
            return 1;
        }

        (void)printf("[OK] Signature verified successfully against %s\n", key_file);
        return 0;
    }

    if (strcmp(cmd, "verify-chain") == 0) {
        const char *leaf_file = NULL;
        const char *inter_file = NULL;
        const char *root_file = NULL;
        const char *in_file = NULL;
        const char *sig_file = NULL;
        uint8_t *leaf_data = NULL; size_t leaf_len = 0U;
        uint8_t *inter_data = NULL; size_t inter_len = 0U;
        uint8_t *root_data = NULL; size_t root_len = 0U;
        uint8_t *payload = NULL; size_t payload_len = 0U;
        uint8_t *wire = NULL; size_t wire_len = 0U;
        alrios_certificate_t leaf_cert;
        alrios_certificate_t inter_cert;
        alrios_certificate_t root_cert;
        alrios_trust_store_t store;
        alrios_notary_signature_t sig;
        alrios_notary_policy_t policy;
        int rc;

        for (i = 2; i < argc; ++i) {
            if (strcmp(argv[i], "--leaf") == 0) {
                if (!next_arg_is_value(argc, argv, i)) return reject_missing_value(argv[i]);
                rc = set_option_once(&leaf_file, argv[++i], "--leaf");
                if (rc != ALRIOS_NOTARY_OK) return 1;
            } else if (strcmp(argv[i], "--inter") == 0) {
                if (!next_arg_is_value(argc, argv, i)) return reject_missing_value(argv[i]);
                rc = set_option_once(&inter_file, argv[++i], "--inter");
                if (rc != ALRIOS_NOTARY_OK) return 1;
            } else if (strcmp(argv[i], "--root") == 0) {
                if (!next_arg_is_value(argc, argv, i)) return reject_missing_value(argv[i]);
                rc = set_option_once(&root_file, argv[++i], "--root");
                if (rc != ALRIOS_NOTARY_OK) return 1;
            } else if (strcmp(argv[i], "--in") == 0) {
                if (!next_arg_is_value(argc, argv, i)) return reject_missing_value(argv[i]);
                rc = set_option_once(&in_file, argv[++i], "--in");
                if (rc != ALRIOS_NOTARY_OK) return 1;
            } else if (strcmp(argv[i], "--sig") == 0) {
                if (!next_arg_is_value(argc, argv, i)) return reject_missing_value(argv[i]);
                rc = set_option_once(&sig_file, argv[++i], "--sig");
                if (rc != ALRIOS_NOTARY_OK) return 1;
            } else {
                return reject_unknown_argument(argv[i]);
            }
        }

        if (!leaf_file || !inter_file || !root_file || !in_file || !sig_file) {
            (void)fprintf(stderr, "[ERROR] Missing required arguments for 'verify-chain'\n");
            print_usage();
            return 1;
        }

        if (read_entire_file(leaf_file, &leaf_data, &leaf_len) != ALRIOS_NOTARY_OK ||
            read_entire_file(inter_file, &inter_data, &inter_len) != ALRIOS_NOTARY_OK ||
            read_entire_file(root_file, &root_data, &root_len) != ALRIOS_NOTARY_OK ||
            read_entire_file(in_file, &payload, &payload_len) != ALRIOS_NOTARY_OK ||
            read_entire_file(sig_file, &wire, &wire_len) != ALRIOS_NOTARY_OK) {
            (void)fprintf(stderr, "[ERROR] Failed to read certificate chain or input files\n");
            free(leaf_data); free(inter_data); free(root_data); free(payload); free(wire);
            return 1;
        }

        if (alrios_certificate_decode(leaf_data, leaf_len, &leaf_cert) != ALRIOS_CERTIFICATE_OK ||
            alrios_certificate_decode(inter_data, inter_len, &inter_cert) != ALRIOS_CERTIFICATE_OK ||
            alrios_certificate_decode(root_data, root_len, &root_cert) != ALRIOS_CERTIFICATE_OK ||
            alrios_notary_signature_decode(wire, wire_len, &sig) != ALRIOS_NOTARY_OK) {
            (void)fprintf(stderr, "[ERROR] Decoding certificate chain or signature wire failed\n");
            free(leaf_data); free(inter_data); free(root_data); free(payload); free(wire);
            return 1;
        }
        free(leaf_data); free(inter_data); free(root_data); free(wire);

        alrios_trust_store_init(&store);
        if (alrios_trust_store_add_anchor(&store, &root_cert) != ALRIOS_CERTIFICATE_OK ||
            alrios_trust_store_seal(&store) != ALRIOS_CERTIFICATE_OK ||
            alrios_notary_policy_init(&policy, ALRIOS_NOTARY_POLICY_STRICT) != ALRIOS_NOTARY_OK) {
            (void)fprintf(stderr, "[ERROR] Failed to initialize strict certificate-bound policy\n");
            free(payload);
            alrios_trust_store_clear(&store);
            return 1;
        }

        rc = alrios_notary_verify_with_certificate(&policy, payload, payload_len, &sig,
                                                   &leaf_cert, &inter_cert, &store,
                                                   (uint64_t)time(NULL));
        free(payload);
        alrios_trust_store_clear(&store);

        if (rc != ALRIOS_NOTARY_OK) {
            (void)fprintf(stderr, "[REJECTED] Certificate chain notary verification failed (rc=%d)\n", rc);
            return 1;
        }

        (void)printf("[OK] Certificate chain and notary signature verified successfully\n");
        return 0;
    }

    print_usage();
    return 1;
}
