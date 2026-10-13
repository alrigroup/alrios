/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/supervisor/policy.h"
#include "alrios/crypto_verify.h"
#include "alrios/package/arapp_v2.h"
#include "alrios/pki/issuer.h"

#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALRIOS_SUPERVISOR_POLICY_MAGIC UINT32_C(0x4152504c)
#define ALRIOS_POLICY_MAX_PACKAGE_SIZE (64U * 1024U * 1024U)
#define ALRIOS_POLICY_LEGACY_MAGIC "ALRIGROUP@APP"
#define ALRIOS_POLICY_LEGACY_MAGIC_SIZE 13U

struct alrios_supervisor_policy {
    uint32_t magic;
    alrios_node_policy_t node_policy;
    alrios_trust_store_t trust_store;
    alrios_certificate_t leaf_certificate;
    alrios_certificate_t intermediate_certificate;
    EVP_PKEY *ed25519_public_key;
    EVP_PKEY *ml_dsa_public_key;
    int trust_ready;
    int sealed;
};

static int is_boolean(int value) {
    return value == 0 || value == 1;
}

static int is_valid_profile(alrios_profile_t profile) {
    return profile == PROFILE_SOVEREIGN_MAX ||
           profile == PROFILE_ENTERPRISE_BAL ||
           profile == PROFILE_EDGE_PERF;
}

static int canonical_policy_matches(const alrios_node_policy_t *policy) {
    alrios_node_policy_t expected;

    if (!policy || !is_valid_profile(policy->profile) ||
        alrios_profile_init(&expected, policy->profile) != 0) {
        return 0;
    }
    return policy->require_pqc == expected.require_pqc &&
           policy->require_zero_disk == expected.require_zero_disk &&
           policy->seccomp_strict == expected.seccomp_strict &&
           policy->log_hashchain == expected.log_hashchain &&
           policy->guard_pages_enabled == expected.guard_pages_enabled &&
           policy->net_timeout_sec == expected.net_timeout_sec &&
           policy->ipc_mode == expected.ipc_mode &&
           policy->envelope_crypto == expected.envelope_crypto &&
           policy->sig_enforcement == expected.sig_enforcement &&
           policy->exec_mode == expected.exec_mode &&
           policy->isolation_mode == expected.isolation_mode;
}

static int validate_requirements(const alrios_app_requirements_t *app_req) {
    if (!app_req ||
        (app_req->container_flags & ~ALRIOS_ARAPP_V2_FLAGS_MASK) != 0U ||
        !is_boolean(app_req->require_pqc) ||
        !is_boolean(app_req->package_signed) ||
        !is_boolean(app_req->certificate_chain_valid) ||
        !is_boolean(app_req->ed25519_signature_present) ||
        !is_boolean(app_req->ed25519_signature_valid) ||
        !is_boolean(app_req->ml_dsa_signature_present) ||
        !is_boolean(app_req->ml_dsa_signature_valid) ||
        !is_boolean(app_req->developer_mode_verified)) {
        return ALRIOS_POLICY_ERR_ARGUMENT;
    }
    if ((app_req->ed25519_signature_valid != 0 &&
         app_req->ed25519_signature_present == 0) ||
        (app_req->ml_dsa_signature_valid != 0 &&
         app_req->ml_dsa_signature_present == 0) ||
        (app_req->certificate_chain_valid != 0 &&
         app_req->package_signed == 0) ||
        (app_req->package_signed == 0 &&
         (app_req->ed25519_signature_present != 0 ||
          app_req->ml_dsa_signature_present != 0))) {
        return ALRIOS_POLICY_ERR_ARGUMENT;
    }
    return ALRIOS_POLICY_OK;
}

static int check_capabilities(const alrios_app_requirements_t *app_req,
                              char *missing_caps_buf,
                              size_t missing_caps_len) {
    const uint32_t known_capabilities =
        ALRIOS_CRYPTO_CAP_ED25519 | ALRIOS_CRYPTO_CAP_AES_256_GCM |
        ALRIOS_CRYPTO_CAP_ML_DSA_65 | ALRIOS_CRYPTO_CAP_ML_KEM_768;
    uint32_t missing;

    if ((app_req->required_capabilities & ~known_capabilities) != 0U) {
        missing = app_req->required_capabilities & ~known_capabilities;
    } else {
        missing = app_req->required_capabilities &
                  ~alrios_crypto_capabilities();
    }
    if (missing == 0U) {
        return ALRIOS_POLICY_OK;
    }
    if (missing_caps_buf && missing_caps_len > 0U) {
        (void)snprintf(missing_caps_buf, missing_caps_len,
                       "missing hardware/crypto capabilities mask: 0x%08x",
                       (unsigned int)missing);
    }
    return ALRIOS_POLICY_ERR_CAPABILITY;
}

static int require_classical_signature(
    const alrios_app_requirements_t *app_req
) {
    if (app_req->package_signed == 0 ||
        app_req->ed25519_signature_present == 0 ||
        app_req->ed25519_signature_valid == 0) {
        return ALRIOS_POLICY_ERR_SIGNATURE;
    }
    if (app_req->certificate_chain_valid == 0) {
        return ALRIOS_POLICY_ERR_CERTIFICATE;
    }
    return ALRIOS_POLICY_OK;
}

static int require_hybrid_signature(const alrios_app_requirements_t *app_req) {
    int result = require_classical_signature(app_req);

    if (result != ALRIOS_POLICY_OK) {
        return result;
    }
    if (app_req->ml_dsa_signature_present == 0 ||
        app_req->ml_dsa_signature_valid == 0 ||
        (app_req->container_flags &
         ALRIOS_ARAPP_V2_FLAG_PQC_HYBRID_SIG) == 0U) {
        return ALRIOS_POLICY_ERR_SIGNATURE;
    }
    return ALRIOS_POLICY_OK;
}

static int read_bounded_file(const char *path,
                             size_t max_size,
                             uint8_t **out_data,
                             size_t *out_length) {
    FILE *file;
    long length;
    uint8_t *data;

    if (!path || !out_data || !out_length) {
        return ALRIOS_POLICY_ERR_ARGUMENT;
    }
    *out_data = NULL;
    *out_length = 0U;
    file = fopen(path, "rb");
    if (!file) {
        return ALRIOS_POLICY_ERR_IO;
    }
    if (fseek(file, 0L, SEEK_END) != 0 ||
        (length = ftell(file)) <= 0L || (size_t)length > max_size ||
        fseek(file, 0L, SEEK_SET) != 0) {
        (void)fclose(file);
        return ALRIOS_POLICY_ERR_IO;
    }
    data = (uint8_t *)malloc((size_t)length);
    if (!data) {
        (void)fclose(file);
        return ALRIOS_POLICY_ERR_MEMORY;
    }
    if (fread(data, 1U, (size_t)length, file) != (size_t)length ||
        fclose(file) != 0) {
        free(data);
        return ALRIOS_POLICY_ERR_IO;
    }
    *out_data = data;
    *out_length = (size_t)length;
    return ALRIOS_POLICY_OK;
}

static int load_certificate_file(const char *path,
                                 alrios_certificate_t *certificate) {
    uint8_t *wire = NULL;
    size_t wire_length = 0U;
    int result;

    result = read_bounded_file(path, ALRIOS_CERTIFICATE_WIRE_MAX,
                               &wire, &wire_length);
    if (result != ALRIOS_POLICY_OK) {
        return result;
    }
    result = alrios_certificate_decode(wire, wire_length, certificate);
    free(wire);
    return result == ALRIOS_CERTIFICATE_OK ?
        ALRIOS_POLICY_OK : ALRIOS_POLICY_ERR_CERTIFICATE;
}

static int make_trust_path(char *out_path,
                           size_t capacity,
                           const char *directory,
                           const char *name) {
    int written;

    if (!out_path || capacity == 0U || !directory || !name ||
        directory[0] == '\0') {
        return ALRIOS_POLICY_ERR_ARGUMENT;
    }
#if defined(_WIN32)
    written = snprintf(out_path, capacity, "%s\\%s", directory, name);
#else
    written = snprintf(out_path, capacity, "%s/%s", directory, name);
#endif
    return written > 0 && (size_t)written < capacity ?
        ALRIOS_POLICY_OK : ALRIOS_POLICY_ERR_ARGUMENT;
}

static int load_trust_configuration(alrios_supervisor_policy_t *policy,
                                    const char *trust_directory) {
    char root_path[1024];
    char intermediate_path[1024];
    char leaf_path[1024];
    char ml_dsa_path[1024];
    alrios_certificate_t root_certificate;
    int result;

    if (!policy || !trust_directory) {
        return ALRIOS_POLICY_ERR_ARGUMENT;
    }
    if (make_trust_path(root_path, sizeof(root_path), trust_directory,
                        "root.cert") != ALRIOS_POLICY_OK ||
        make_trust_path(intermediate_path, sizeof(intermediate_path),
                        trust_directory, "intermediate.cert") !=
            ALRIOS_POLICY_OK ||
        make_trust_path(leaf_path, sizeof(leaf_path), trust_directory,
                        "leaf.cert") != ALRIOS_POLICY_OK) {
        return ALRIOS_POLICY_ERR_ARGUMENT;
    }
    result = load_certificate_file(root_path, &root_certificate);
    if (result != ALRIOS_POLICY_OK) {
        return result;
    }
    result = load_certificate_file(intermediate_path,
                                   &policy->intermediate_certificate);
    if (result != ALRIOS_POLICY_OK) {
        return result;
    }
    result = load_certificate_file(leaf_path, &policy->leaf_certificate);
    if (result != ALRIOS_POLICY_OK) {
        return result;
    }
    if (policy->leaf_certificate.public_key_algorithm !=
            ALRIOS_PUBLIC_KEY_ALGORITHM_ED25519 ||
        policy->leaf_certificate.public_key_len !=
            ALRIOS_CERTIFICATE_ED25519_KEY_SIZE) {
        return ALRIOS_POLICY_ERR_CERTIFICATE;
    }

    alrios_trust_store_init(&policy->trust_store);
    if (alrios_trust_store_add_anchor(&policy->trust_store,
                                      &root_certificate) !=
            ALRIOS_CERTIFICATE_OK ||
        alrios_trust_store_seal(&policy->trust_store) !=
            ALRIOS_CERTIFICATE_OK) {
        alrios_trust_store_clear(&policy->trust_store);
        return ALRIOS_POLICY_ERR_CERTIFICATE;
    }
    policy->ed25519_public_key = EVP_PKEY_new_raw_public_key(
        EVP_PKEY_ED25519, NULL, policy->leaf_certificate.public_key,
        policy->leaf_certificate.public_key_len);
    if (!policy->ed25519_public_key) {
        return ALRIOS_POLICY_ERR_CERTIFICATE;
    }

    if (make_trust_path(ml_dsa_path, sizeof(ml_dsa_path), trust_directory,
                        "ml-dsa-65.pub") != ALRIOS_POLICY_OK) {
        return ALRIOS_POLICY_ERR_ARGUMENT;
    }
    {
        uint8_t *ml_dsa_wire = NULL;
        size_t ml_dsa_wire_length = 0U;

        result = read_bounded_file(ml_dsa_path,
                                   ALRIOS_CERTIFICATE_ML_DSA_65_KEY_SIZE,
                                   &ml_dsa_wire, &ml_dsa_wire_length);
        if (result == ALRIOS_POLICY_OK) {
            if (ml_dsa_wire_length !=
                ALRIOS_CERTIFICATE_ML_DSA_65_KEY_SIZE) {
                free(ml_dsa_wire);
                return ALRIOS_POLICY_ERR_CERTIFICATE;
            }
            policy->ml_dsa_public_key = EVP_PKEY_new_raw_public_key_ex(
                NULL, "ML-DSA-65", NULL, ml_dsa_wire, ml_dsa_wire_length);
            free(ml_dsa_wire);
            if (!policy->ml_dsa_public_key) {
                return ALRIOS_POLICY_ERR_CERTIFICATE;
            }
        } else if (policy->node_policy.profile == PROFILE_SOVEREIGN_MAX) {
            return result;
        }
    }
    policy->trust_ready = 1;
    return ALRIOS_POLICY_OK;
}

int alrios_supervisor_policy_startup(
    alrios_supervisor_policy_t **out_policy,
    alrios_profile_t profile
) {
    alrios_supervisor_policy_t *policy;
    int result;

    if (!out_policy) {
        return ALRIOS_POLICY_ERR_ARGUMENT;
    }
    *out_policy = NULL;
    if (!is_valid_profile(profile)) {
        return ALRIOS_POLICY_ERR_ARGUMENT;
    }

    policy = (alrios_supervisor_policy_t *)calloc(1U, sizeof(*policy));
    if (!policy) {
        return ALRIOS_POLICY_ERR_MEMORY;
    }
    result = alrios_profile_init(&policy->node_policy, profile);
    if (result != 0) {
        free(policy);
        return result == ALRIOS_PROFILE_ERR_UNAVAILABLE ?
            ALRIOS_POLICY_ERR_CAPABILITY : ALRIOS_POLICY_ERR_ARGUMENT;
    }
    policy->magic = ALRIOS_SUPERVISOR_POLICY_MAGIC;
    policy->sealed = 1;
    *out_policy = policy;
    return ALRIOS_POLICY_OK;
}

int alrios_supervisor_policy_startup_with_trust(
    alrios_supervisor_policy_t **out_policy,
    alrios_profile_t profile,
    const char *trust_directory
) {
    alrios_supervisor_policy_t *policy = NULL;
    int result;

    result = alrios_supervisor_policy_startup(&policy, profile);
    if (result != ALRIOS_POLICY_OK) {
        return result;
    }
    result = load_trust_configuration(policy, trust_directory);
    if (result != ALRIOS_POLICY_OK) {
        alrios_supervisor_policy_shutdown(policy);
        return result;
    }
    *out_policy = policy;
    return ALRIOS_POLICY_OK;
}

void alrios_supervisor_policy_shutdown(alrios_supervisor_policy_t *policy) {
    if (policy) {
        EVP_PKEY_free(policy->ml_dsa_public_key);
        EVP_PKEY_free(policy->ed25519_public_key);
        alrios_trust_store_clear(&policy->trust_store);
        (void)memset(policy, 0, sizeof(*policy));
        free(policy);
    }
}

int alrios_supervisor_policy_set_profile(
    alrios_supervisor_policy_t *policy,
    alrios_profile_t profile
) {
    if (!policy || policy->magic != ALRIOS_SUPERVISOR_POLICY_MAGIC ||
        !is_valid_profile(profile)) {
        return ALRIOS_POLICY_ERR_ARGUMENT;
    }
    if (policy->sealed != 0) {
        return profile == policy->node_policy.profile ?
            ALRIOS_POLICY_ERR_IMMUTABLE : ALRIOS_POLICY_ERR_DOWNGRADE;
    }
    return ALRIOS_POLICY_ERR_STATE;
}

int alrios_supervisor_policy_get_profile(
    const alrios_supervisor_policy_t *policy,
    alrios_profile_t *out_profile
) {
    if (!policy || !out_profile ||
        policy->magic != ALRIOS_SUPERVISOR_POLICY_MAGIC ||
        policy->sealed == 0 || !canonical_policy_matches(&policy->node_policy)) {
        return ALRIOS_POLICY_ERR_STATE;
    }
    *out_profile = policy->node_policy.profile;
    return ALRIOS_POLICY_OK;
}

int alrios_supervisor_evaluate_policy(
    const alrios_node_policy_t *node_policy,
    const alrios_app_requirements_t *app_req,
    char *missing_caps_buf,
    size_t missing_caps_len
) {
    int result;
    int is_devmode;

    if (missing_caps_buf && missing_caps_len > 0U) {
        missing_caps_buf[0] = '\0';
    }
    if (!node_policy || validate_requirements(app_req) != ALRIOS_POLICY_OK) {
        return ALRIOS_POLICY_ERR_ARGUMENT;
    }
    if (!canonical_policy_matches(node_policy)) {
        return ALRIOS_POLICY_ERR_STATE;
    }

    result = check_capabilities(app_req, missing_caps_buf, missing_caps_len);
    if (result != ALRIOS_POLICY_OK) {
        return result;
    }

    is_devmode =
        (app_req->container_flags & ALRIOS_ARAPP_V2_FLAG_RING_DEVMODE) != 0U;
    if (node_policy->profile == PROFILE_SOVEREIGN_MAX) {
        result = require_hybrid_signature(app_req);
        if (result != ALRIOS_POLICY_OK) {
            return result;
        }
        if (is_devmode) {
            return ALRIOS_POLICY_ERR_REJECTED;
        }
        return ALRIOS_POLICY_OK;
    }

    if (node_policy->profile == PROFILE_ENTERPRISE_BAL) {
        if (app_req->package_signed != 0) {
            result = require_classical_signature(app_req);
            if (result != ALRIOS_POLICY_OK) {
                return result;
            }
            if (app_req->require_pqc != 0 ||
                (app_req->container_flags &
                 ALRIOS_ARAPP_V2_FLAG_PQC_HYBRID_SIG) != 0U) {
                return require_hybrid_signature(app_req);
            }
            return ALRIOS_POLICY_OK;
        }
        if (is_devmode) {
            return app_req->developer_mode_verified != 0 ?
                ALRIOS_POLICY_OK : ALRIOS_POLICY_ERR_REJECTED;
        }
        return ALRIOS_POLICY_ERR_SIGNATURE;
    }

    if (is_devmode && app_req->developer_mode_verified == 0) {
        return ALRIOS_POLICY_ERR_REJECTED;
    }
    if (app_req->package_signed != 0) {
        result = require_classical_signature(app_req);
        if (result != ALRIOS_POLICY_OK) {
            return result;
        }
        if (app_req->require_pqc != 0 ||
            (app_req->container_flags &
             ALRIOS_ARAPP_V2_FLAG_PQC_HYBRID_SIG) != 0U) {
            return require_hybrid_signature(app_req);
        }
    }
    return ALRIOS_POLICY_OK;
}

int alrios_supervisor_policy_evaluate_launch(
    const alrios_supervisor_policy_t *policy,
    const alrios_app_requirements_t *app_req,
    char *missing_caps_buf,
    size_t missing_caps_len
) {
    if (!policy || policy->magic != ALRIOS_SUPERVISOR_POLICY_MAGIC ||
        policy->sealed == 0) {
        return ALRIOS_POLICY_ERR_STATE;
    }
    return alrios_supervisor_evaluate_policy(
        &policy->node_policy, app_req, missing_caps_buf, missing_caps_len);
}

static int bytes_all_zero(const uint8_t *bytes, size_t length) {
    uint8_t combined = 0U;
    size_t index;

    for (index = 0U; index < length; ++index) {
        combined |= bytes[index];
    }
    return combined == 0U;
}

static void set_reason(char *reason_buf,
                       size_t reason_len,
                       const char *reason) {
    if (reason_buf && reason_len > 0U) {
        (void)snprintf(reason_buf, reason_len, "%s", reason);
    }
}

int alrios_supervisor_policy_evaluate_package(
    const alrios_supervisor_policy_t *policy,
    const char *package_path,
    uint64_t now,
    char *reason_buf,
    size_t reason_len
) {
    uint8_t *package_data = NULL;
    size_t package_length = 0U;
    alrios_arapp_v2_view_t view;
    alrios_app_requirements_t requirements;
    int result;

    if (reason_buf && reason_len > 0U) {
        reason_buf[0] = '\0';
    }
    if (!policy || policy->magic != ALRIOS_SUPERVISOR_POLICY_MAGIC ||
        policy->sealed == 0 || !package_path) {
        return ALRIOS_POLICY_ERR_STATE;
    }
    result = read_bounded_file(package_path, ALRIOS_POLICY_MAX_PACKAGE_SIZE,
                               &package_data, &package_length);
    if (result != ALRIOS_POLICY_OK) {
        set_reason(reason_buf, reason_len, "package read failed");
        return result;
    }

    if (package_length >= ALRIOS_POLICY_LEGACY_MAGIC_SIZE &&
        memcmp(package_data, ALRIOS_POLICY_LEGACY_MAGIC,
               ALRIOS_POLICY_LEGACY_MAGIC_SIZE) == 0 &&
        (package_length < ALRIOS_ARAPP_V2_MAGIC_SIZE ||
         memcmp(package_data, ALRIOS_ARAPP_V2_MAGIC,
                ALRIOS_ARAPP_V2_MAGIC_SIZE) != 0)) {
        (void)memset(&requirements, 0, sizeof(requirements));
        requirements.container_flags = ALRIOS_ARAPP_V2_FLAG_RING_DEVMODE;
        requirements.developer_mode_verified =
            policy->node_policy.profile != PROFILE_SOVEREIGN_MAX;
        free(package_data);
        result = alrios_supervisor_policy_evaluate_launch(
            policy, &requirements, reason_buf, reason_len);
        if (result != ALRIOS_POLICY_OK) {
            set_reason(reason_buf, reason_len,
                       "legacy unsigned package rejected by profile");
        }
        return result;
    }

    result = alrios_arapp_v2_validate(
        package_data, package_length, now,
        ALRIOS_ARAPP_V2_MAX_CLOCK_SKEW_SECONDS, &view);
    if (result != ALRIOS_ARAPP_V2_OK) {
        free(package_data);
        set_reason(reason_buf, reason_len, "invalid arapp v2 envelope");
        return ALRIOS_POLICY_ERR_FORMAT;
    }

    (void)memset(&requirements, 0, sizeof(requirements));
    requirements.container_flags = view.flags;
    requirements.require_pqc =
        (view.flags & ALRIOS_ARAPP_V2_FLAG_PQC_HYBRID_SIG) != 0U;
    requirements.ed25519_signature_present = !bytes_all_zero(
        view.ed25519_signature, ALRIOS_ARAPP_V2_ED25519_SIGNATURE_SIZE);
    requirements.ml_dsa_signature_present = !bytes_all_zero(
        view.ml_dsa_65_signature,
        ALRIOS_ARAPP_V2_ML_DSA_65_SIGNATURE_SIZE);
    requirements.package_signed = requirements.ed25519_signature_present;
    requirements.required_capabilities = ALRIOS_CRYPTO_CAP_ED25519;
    if (requirements.require_pqc != 0) {
        requirements.required_capabilities |= ALRIOS_CRYPTO_CAP_ML_DSA_65;
    }

    if (requirements.package_signed != 0 && policy->trust_ready != 0 &&
        policy->ed25519_public_key &&
        alrios_arapp_v2_verify_ed25519(
            package_data, package_length, policy->ed25519_public_key) ==
            ALRIOS_ARAPP_V2_SIG_OK) {
        requirements.ed25519_signature_valid = 1;
        requirements.certificate_chain_valid =
            alrios_pki_verify_chain(&policy->trust_store,
                                    &policy->leaf_certificate,
                                    &policy->intermediate_certificate,
                                    now) == ALRIOS_ISSUER_OK;
    }
    if (requirements.ml_dsa_signature_present != 0 &&
        policy->ml_dsa_public_key &&
        alrios_arapp_v2_verify_ml_dsa_65(
            package_data, package_length, policy->ml_dsa_public_key) ==
            ALRIOS_ARAPP_V2_SIG_OK) {
        requirements.ml_dsa_signature_valid = 1;
    }
    if ((view.flags & ALRIOS_ARAPP_V2_FLAG_RING_DEVMODE) != 0U) {
        requirements.developer_mode_verified =
            policy->node_policy.profile != PROFILE_SOVEREIGN_MAX &&
            requirements.package_signed == 0 &&
            requirements.ed25519_signature_present == 0 &&
            requirements.ml_dsa_signature_present == 0;
    }

    result = alrios_supervisor_policy_evaluate_launch(
        policy, &requirements, reason_buf, reason_len);
    if (result != ALRIOS_POLICY_OK &&
        (!reason_buf || reason_len == 0U || reason_buf[0] == '\0')) {
        set_reason(reason_buf, reason_len, "package rejected by node profile");
    }
    free(package_data);
    return result;
}
