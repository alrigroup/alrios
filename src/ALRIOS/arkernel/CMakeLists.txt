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
# ALRIOS Native SDK (AR-SDK) and Sovereign Security Subsystem (libarsdk)
# ====================================================================
add_library(arsdk STATIC
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/sdk/ar_sdk.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/profiles/profiles.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/kernel/memloader.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/hooks/hooks_executor.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/deploy/fd_pass.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/deploy/atomic_swap.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/deploy/arapp_index.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/crypto/utils.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/crypto/sha512.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/crypto/ed25519.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/crypto/ml_dsa_65.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/crypto/aes_gcm.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/ipc/ipc_channel.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/ipc/epoll_multiplexer.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/ipc/http_serializer.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/sandbox/seccomp_filter.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/sandbox/cgroups_init.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/sandbox/ns_unshare.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/pki/manifest_canon.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/pki/preimage.c
)
set_property(TARGET arsdk PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(arsdk PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/alrios
    include
    os/include
)
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
