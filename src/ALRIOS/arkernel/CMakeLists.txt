add_library(arlog STATIC
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/core/log.c
)
set_property(TARGET arlog PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(arlog PUBLIC
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/core
)

add_library(ararchive STATIC
    ${CMAKE_SOURCE_DIR}/src/tools/zip.c
)
set_property(TARGET ararchive PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(ararchive PUBLIC
    ${CMAKE_SOURCE_DIR}/src/tools
)

add_library(armanifest STATIC
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/core/arapp_parser.c
)
set_property(TARGET armanifest PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(armanifest PUBLIC
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/core
)

add_library(arhal_isolation STATIC
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/arkernel/os/isolation.c
)
set_property(TARGET arhal_isolation PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(arhal_isolation PUBLIC
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/arkernel/os/include
)

add_library(arkernel
    os/main.c
    svc/svc_manager.c
    svc/svc_supervisor.c
    svc/svc_health.c
    svc/arkernel_main.c
    ipc/ar_ipc.c
)
set_property(TARGET arkernel PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(arkernel PUBLIC
    include
    ipc
    os/include
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/core
)
target_link_libraries(arkernel PUBLIC arlog)
find_package(OpenSSL REQUIRED)
target_include_directories(arkernel PUBLIC ${OPENSSL_INCLUDE_DIR})
set_target_properties(arkernel PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/arcore/lib"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/arcore/lib"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/arcore/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/arcore/lib"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/arcore/lib"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/arcore/lib"
)

if(WIN32)
    target_link_libraries(arkernel PUBLIC ws2_32)
endif()

add_custom_command(TARGET arkernel POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/arcore/include"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/os/include/aros_hal.h"
        "${CMAKE_SOURCE_DIR}/arcore/include/aros_hal.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/include/ar_ipc.h"
        "${CMAKE_SOURCE_DIR}/arcore/include/ar_ipc.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/include/ar_kernel.h"
        "${CMAKE_SOURCE_DIR}/arcore/include/ar_kernel.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/include/ar_svc.h"
        "${CMAKE_SOURCE_DIR}/arcore/include/ar_svc.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/src/ALRIOS/core/log.h"
        "${CMAKE_SOURCE_DIR}/arcore/include/log.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/src/ALRIOS/core/arapp_parser.h"
        "${CMAKE_SOURCE_DIR}/arcore/include/arapp_parser.h"
    COMMENT "Installing ALRIOS kernel SDK headers into arcore/include"
)

# ====================================================================
# Modular Subsystems & Libraries
# ====================================================================

# 1. arsdk_core (Strict Portable C11 Application SDK)
add_library(arsdk_core STATIC
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/sdk/ar_sdk.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/vault/vault.c
)
set_property(TARGET arsdk_core PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(arsdk_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/alrios
)

add_library(arvault STATIC
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/vault/vault.c
    ${CMAKE_SOURCE_DIR}/src/libraries/vault/store.c
    ${CMAKE_SOURCE_DIR}/src/libraries/vault/inject.c
)
set_property(TARGET arvault PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(arvault PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/alrios
    ${OPENSSL_INCLUDE_DIR}
)
target_link_libraries(arvault PUBLIC arcrypto OpenSSL::Crypto)

# 2. arcrypto (Cryptographic Operations & Hardware Verification)
add_library(arcrypto STATIC
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/crypto/utils.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/crypto/sha512.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/crypto/ed25519.c
    ${CMAKE_SOURCE_DIR}/src/libraries/crypto/ml_dsa_65.c
    ${CMAKE_SOURCE_DIR}/src/libraries/crypto/ml_kem_768.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/crypto/aes_gcm.c
)
set_property(TARGET arcrypto PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(arcrypto PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/alrios
    ${OPENSSL_INCLUDE_DIR}
)
target_link_libraries(arcrypto PUBLIC OpenSSL::Crypto)

# 3. arpolicy (Execution Profiles & Compliance Gates)
add_library(arpolicy STATIC
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/profiles/profiles.c
    ${CMAKE_SOURCE_DIR}/src/daemons/supervisor/policy.c
    ${CMAKE_SOURCE_DIR}/src/daemons/supervisor/devmode.c
)
set_property(TARGET arpolicy PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(arpolicy PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/alrios
)
target_link_libraries(arpolicy PUBLIC arcrypto)

# 4. aripc_v2 (Sovereign Binary Framing & Multiplexing)
add_library(aripc_v2 STATIC
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/ipc/ipc_channel.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/ipc/epoll_multiplexer.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/ipc/http_serializer.c
    ${CMAKE_SOURCE_DIR}/src/libraries/ipc/guest_channel.c
    ${CMAKE_SOURCE_DIR}/src/daemons/supervisor/guest_router.c
)
set_property(TARGET aripc_v2 PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(aripc_v2 PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/alrios
)

# 5. arpki (Manifest Canonicalization & Preimage Generation)
add_library(arpki STATIC
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/pki/manifest_canon.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/pki/preimage.c
    ${CMAKE_SOURCE_DIR}/src/libraries/pki/certificate.c
    ${CMAKE_SOURCE_DIR}/src/libraries/pki/issuer.c
    ${CMAKE_SOURCE_DIR}/src/libraries/pki/notary.c
    ${CMAKE_SOURCE_DIR}/src/libraries/pki/notary_isolation.c
)
set_property(TARGET arpki PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(arpki PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/alrios
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/arkernel/os/include
)
target_link_libraries(arpki PUBLIC arcrypto arhal_isolation)

# 6. ardeploy (Zero-Downtime Deployment, SCM_RIGHTS, Symlinks & Deltas)
add_library(ardeploy STATIC
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/deploy/fd_pass.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/deploy/atomic_swap.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/deploy/arapp_index.c
    ${CMAKE_SOURCE_DIR}/src/libraries/deploy/drain.c
    ${CMAKE_SOURCE_DIR}/src/daemons/supervisor/socket_handover.c
)
set_property(TARGET ardeploy PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(ardeploy PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/alrios
)

# 7. arhooks (Deterministic Lifecycle Event Execution)
add_library(arhooks STATIC
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/hooks/hooks_executor.c
    ${CMAKE_SOURCE_DIR}/src/commands/armake/build_pipeline.c
    ${CMAKE_SOURCE_DIR}/src/daemons/supervisor/deploy_hooks.c
)
set_property(TARGET arhooks PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(arhooks PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/alrios
)

# 8. arsandbox_linux (Seccomp-BPF, Cgroups v2 & Namespace Quarantine)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_library(arsandbox_linux STATIC
        ${CMAKE_SOURCE_DIR}/src/ALRIOS/sandbox/seccomp_filter.c
        ${CMAKE_SOURCE_DIR}/src/ALRIOS/sandbox/cgroups_init.c
        ${CMAKE_SOURCE_DIR}/src/ALRIOS/sandbox/ns_unshare.c
        ${CMAKE_SOURCE_DIR}/src/ALRIOS/kernel/memloader.c
        ${CMAKE_SOURCE_DIR}/src/libraries/sandbox/launcher_linux.c
    )
    set_property(TARGET arsandbox_linux PROPERTY POSITION_INDEPENDENT_CODE ON)
    target_include_directories(arsandbox_linux PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/alrios
    )
endif()

# 9. arpackage (Canonical .arapp v2 Reader and Envelope Structural Validator)
add_library(arpackage STATIC
    ${CMAKE_SOURCE_DIR}/src/libraries/package/arapp_v2_reader.c
    ${CMAKE_SOURCE_DIR}/src/libraries/package/arapp_v2_signatures.c
)
set_property(TARGET arpackage PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(arpackage PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/alrios
)
target_link_libraries(arpackage PUBLIC arpki arcrypto OpenSSL::Crypto)

# 10. arloader (Verified Decrypt-to-Memfd Execution Pipeline)
add_library(arloader STATIC
    ${CMAKE_SOURCE_DIR}/src/libraries/loader/sovereign_loader.c
)
set_property(TARGET arloader PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(arloader PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/alrios
)
target_link_libraries(arloader PUBLIC arpackage arpki arcrypto OpenSSL::Crypto)

# 11. arstorage (Immutable Code and External Persistent Data Mounts)
add_library(arstorage STATIC
    ${CMAKE_SOURCE_DIR}/src/libraries/storage/bindings.c
    ${CMAKE_SOURCE_DIR}/src/daemons/supervisor/mounts.c
)
set_property(TARGET arstorage PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(arstorage PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/alrios
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/arkernel/os/include
)
target_link_libraries(arstorage PUBLIC arkernel arcrypto)

# 12. araudit (Append-Only Cryptographic Audit Chain & Forwarder)
add_library(araudit STATIC
    ${CMAKE_SOURCE_DIR}/src/libraries/audit/hash_chain.c
    ${CMAKE_SOURCE_DIR}/src/libraries/audit/forwarder.c
)
set_property(TARGET araudit PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(araudit PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/alrios
)
target_link_libraries(araudit PUBLIC arcrypto OpenSSL::Crypto)

add_executable(ar_auditd ${CMAKE_SOURCE_DIR}/src/daemons/ar_auditd/main.c)
target_link_libraries(ar_auditd PRIVATE araudit arcrypto OpenSSL::Crypto)
set_target_properties(ar_auditd PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/arcore"
)

# 13. arresources (PSI Memory Governance & Cooperative Shedding)
add_library(arresources STATIC
    ${CMAKE_SOURCE_DIR}/src/libraries/resources/psi.c
)
set_property(TARGET arresources PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(arresources PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/alrios
)

add_executable(ar_resourced ${CMAKE_SOURCE_DIR}/src/daemons/ar_resourced/main.c)
target_link_libraries(ar_resourced PRIVATE arresources)
set_target_properties(ar_resourced PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/arcore"
)

# ====================================================================
# Umbrella Target: libarsdk (Maintains 100% Backward Compatibility)
# ====================================================================
add_library(arsdk STATIC
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/sdk/ar_sdk.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/vault/vault.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/profiles/profiles.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/kernel/memloader.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/hooks/hooks_executor.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/deploy/fd_pass.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/deploy/atomic_swap.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/deploy/arapp_index.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/crypto/utils.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/crypto/sha512.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/crypto/ed25519.c
    ${CMAKE_SOURCE_DIR}/src/libraries/crypto/ml_dsa_65.c
    ${CMAKE_SOURCE_DIR}/src/libraries/crypto/ml_kem_768.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/crypto/aes_gcm.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/ipc/ipc_channel.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/ipc/epoll_multiplexer.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/ipc/http_serializer.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/sandbox/seccomp_filter.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/sandbox/cgroups_init.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/sandbox/ns_unshare.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/pki/manifest_canon.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/pki/preimage.c
    ${CMAKE_SOURCE_DIR}/src/libraries/pki/certificate.c
    ${CMAKE_SOURCE_DIR}/src/libraries/pki/issuer.c
    ${CMAKE_SOURCE_DIR}/src/libraries/package/arapp_v2_reader.c
    ${CMAKE_SOURCE_DIR}/src/libraries/package/arapp_v2_signatures.c
    ${CMAKE_SOURCE_DIR}/src/libraries/loader/sovereign_loader.c
    ${CMAKE_SOURCE_DIR}/src/libraries/storage/bindings.c
    ${CMAKE_SOURCE_DIR}/src/daemons/supervisor/mounts.c
    ${CMAKE_SOURCE_DIR}/src/libraries/audit/hash_chain.c
    ${CMAKE_SOURCE_DIR}/src/daemons/supervisor/devmode.c
    ${CMAKE_SOURCE_DIR}/src/libraries/sandbox/launcher_linux.c
)
set_property(TARGET arsdk PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(arsdk PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/alrios
    include
    os/include
)
target_link_libraries(arsdk PUBLIC OpenSSL::Crypto)
set_target_properties(arsdk PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/arcore/lib"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/arcore/lib"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/arcore/lib"
)

add_custom_command(TARGET arsdk POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/arcore/include/alrios"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/include/alrios"
        "${CMAKE_SOURCE_DIR}/arcore/include/alrios"
    COMMENT "Installing ALRIOS Native SDK (ar.h, profiles.h, etc.) into arcore/include/alrios"
)

add_executable(supervisor_policy
    ${CMAKE_SOURCE_DIR}/src/daemons/supervisor/main.c
)
target_link_libraries(supervisor_policy PRIVATE arpolicy)
set_target_properties(supervisor_policy PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/arcore"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/arcore"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/arcore"
)
