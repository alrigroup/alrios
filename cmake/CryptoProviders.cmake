# ====================================================================
# Copyright (c) 2026 ALRIGROUP and its affiliates.
# Engineered and maintained by ALRI Development.
#
# This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
# found in the LICENSE file in the root directory of this source tree
# and at: https://github.com/alrigroup/licenses
# ====================================================================
#
# CryptoProviders.cmake - Cryptographic provider discovery and capability gates
#
# Discovers cryptographic engine capabilities for Post-Quantum Cryptography
# (NIST FIPS 204 ML-DSA-65, NIST FIPS 203 ML-KEM-768). When supported by OpenSSL,
# the capability macros ALRIOS_HAVE_ML_DSA_65 and ALRIOS_HAVE_ML_KEM_768 are defined
# globally. If unavailable, crypto operations fail closed.
# ====================================================================

include(CheckCSourceRuns)
include(CheckCSourceCompiles)
include(CMakePushCheckState)

option(ALRIOS_ENABLE_ML_DSA_65 "Enable FIPS 204 ML-DSA-65 post-quantum provider" ON)
option(ALRIOS_ENABLE_ML_KEM_768 "Enable FIPS 203 ML-KEM-768 post-quantum provider" ON)

if(ALRIOS_ENABLE_ML_DSA_65)
    cmake_push_check_state()
    set(CMAKE_REQUIRED_INCLUDES ${OPENSSL_INCLUDE_DIR})
    set(CMAKE_REQUIRED_LIBRARIES OpenSSL::Crypto)

    if(CMAKE_CROSSCOMPILING)
        check_c_source_compiles("
        #include <openssl/evp.h>
        int main(void) {
            EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, \"ML-DSA-65\", NULL);
            if (!ctx) return 1;
            EVP_PKEY_CTX_free(ctx);
            return 0;
        }
        " ALRIOS_OPENSSL_SUPPORTS_ML_DSA_65)
    else()
        check_c_source_runs("
        #include <openssl/evp.h>
        int main(void) {
            EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, \"ML-DSA-65\", NULL);
            if (!ctx) return 1;
            EVP_PKEY_CTX_free(ctx);
            return 0;
        }
        " ALRIOS_OPENSSL_SUPPORTS_ML_DSA_65)
    endif()

    cmake_pop_check_state()

    if(ALRIOS_OPENSSL_SUPPORTS_ML_DSA_65)
        message(STATUS "ALRIOS Crypto Provider: FIPS 204 ML-DSA-65 detected and verified.")
        add_compile_definitions(ALRIOS_HAVE_ML_DSA_65=1)
    else()
        message(WARNING "ALRIOS Crypto Provider: ML-DSA-65 not supported by OpenSSL; failing closed.")
    endif()
else()
    message(STATUS "ALRIOS Crypto Provider: ML-DSA-65 disabled by configuration.")
endif()

if(ALRIOS_ENABLE_ML_KEM_768)
    cmake_push_check_state()
    set(CMAKE_REQUIRED_INCLUDES ${OPENSSL_INCLUDE_DIR})
    set(CMAKE_REQUIRED_LIBRARIES OpenSSL::Crypto)

    if(CMAKE_CROSSCOMPILING)
        check_c_source_compiles("
        #include <openssl/evp.h>
        int main(void) {
            EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, \"ML-KEM-768\", NULL);
            if (!ctx) return 1;
            EVP_PKEY_CTX_free(ctx);
            return 0;
        }
        " ALRIOS_OPENSSL_SUPPORTS_ML_KEM_768)
    else()
        check_c_source_runs("
        #include <openssl/evp.h>
        int main(void) {
            EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, \"ML-KEM-768\", NULL);
            if (!ctx) return 1;
            EVP_PKEY_CTX_free(ctx);
            return 0;
        }
        " ALRIOS_OPENSSL_SUPPORTS_ML_KEM_768)
    endif()

    cmake_pop_check_state()

    if(ALRIOS_OPENSSL_SUPPORTS_ML_KEM_768)
        message(STATUS "ALRIOS Crypto Provider: FIPS 203 ML-KEM-768 detected and verified.")
        add_compile_definitions(ALRIOS_HAVE_ML_KEM_768=1)
    else()
        message(WARNING "ALRIOS Crypto Provider: ML-KEM-768 not supported by OpenSSL; failing closed.")
    endif()
else()
    message(STATUS "ALRIOS Crypto Provider: ML-KEM-768 disabled by configuration.")
endif()
