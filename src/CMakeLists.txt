set(ALRIOS_PUBLIC_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
set(ALRIOS_PLATFORM_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/platform/kernel/include")
set(ALRIOS_HAL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/platform/hal/include")
set(ALRIOS_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/platform/core")
set(ALRIOS_ARCHIVE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libraries/archive")

function(alrios_pic target)
    set_property(TARGET ${target} PROPERTY POSITION_INDEPENDENT_CODE ON)
endfunction()

add_library(arlog STATIC libraries/log/log.c)
alrios_pic(arlog)
target_include_directories(arlog PUBLIC libraries/log)

add_library(ararchive STATIC libraries/archive/zip.c)
alrios_pic(ararchive)
target_include_directories(ararchive PUBLIC libraries/archive)

add_library(armanifest STATIC libraries/manifest/arapp_parser.c)
alrios_pic(armanifest)
target_include_directories(armanifest PUBLIC libraries/manifest)

add_library(arhal_isolation STATIC platform/hal/isolation.c)
alrios_pic(arhal_isolation)
target_include_directories(arhal_isolation PUBLIC "${ALRIOS_HAL_INCLUDE_DIR}")

set(ALRIOS_HAL_SOURCES platform/hal/main.c)
if(WIN32)
    list(APPEND ALRIOS_HAL_SOURCES
        platform/hal/windows/file.c
        platform/hal/windows/process.c
        platform/hal/windows/module.c
        platform/hal/windows/thread.c
        platform/hal/windows/time.c
        platform/hal/windows/socket.c
        platform/hal/windows/fs.c
        platform/hal/windows/memory.c
        platform/hal/windows/ssl.c
        platform/hal/windows/poll.c
    )
else()
    list(APPEND ALRIOS_HAL_SOURCES
        platform/hal/linux/file.c
        platform/hal/linux/process.c
        platform/hal/linux/module.c
        platform/hal/linux/thread.c
        platform/hal/linux/time.c
        platform/hal/linux/socket.c
        platform/hal/linux/fs.c
        platform/hal/linux/memory.c
        platform/hal/linux/ssl.c
        platform/hal/linux/poll.c
    )
endif()

add_library(arhal STATIC ${ALRIOS_HAL_SOURCES})
alrios_pic(arhal)
target_include_directories(arhal
    PUBLIC "${ALRIOS_HAL_INCLUDE_DIR}"
    PRIVATE platform/hal
)
target_link_libraries(arhal PUBLIC OpenSSL::SSL OpenSSL::Crypto Threads::Threads ${CMAKE_DL_LIBS})
if(WIN32)
    target_link_libraries(arhal PUBLIC ws2_32)
endif()

add_library(arkernel STATIC
    platform/kernel/svc/svc_manager.c
    platform/kernel/svc/svc_supervisor.c
    platform/kernel/svc/svc_health.c
    platform/kernel/svc/arkernel_main.c
    platform/kernel/ipc/ar_ipc.c
)
alrios_pic(arkernel)
target_include_directories(arkernel PUBLIC
    "${ALRIOS_PLATFORM_INCLUDE_DIR}"
    "${ALRIOS_HAL_INCLUDE_DIR}"
    libraries/log
)
target_link_libraries(arkernel PUBLIC arhal arlog)
set_target_properties(arkernel PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/arcore/lib"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/arcore/lib"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/arcore/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/arcore/lib"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/arcore/lib"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/arcore/lib"
)
add_custom_command(TARGET arkernel POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/arcore/include"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ALRIOS_HAL_INCLUDE_DIR}/aros_hal.h" "${CMAKE_SOURCE_DIR}/arcore/include/aros_hal.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ALRIOS_PLATFORM_INCLUDE_DIR}/ar_ipc.h" "${CMAKE_SOURCE_DIR}/arcore/include/ar_ipc.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ALRIOS_PLATFORM_INCLUDE_DIR}/ar_kernel.h" "${CMAKE_SOURCE_DIR}/arcore/include/ar_kernel.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ALRIOS_PLATFORM_INCLUDE_DIR}/ar_svc.h" "${CMAKE_SOURCE_DIR}/arcore/include/ar_svc.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/libraries/log/log.h" "${CMAKE_SOURCE_DIR}/arcore/include/log.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/libraries/manifest/arapp_parser.h" "${CMAKE_SOURCE_DIR}/arcore/include/arapp_parser.h"
    COMMENT "Installing ALRIOS kernel SDK headers into arcore/include"
)

add_library(arvault STATIC libraries/vault/vault.c)
alrios_pic(arvault)
target_include_directories(arvault PUBLIC "${ALRIOS_PUBLIC_INCLUDE_DIR}")

add_library(arsdk_core STATIC libraries/sdk/ar_sdk.c)
alrios_pic(arsdk_core)
target_include_directories(arsdk_core PUBLIC "${ALRIOS_PUBLIC_INCLUDE_DIR}")
target_link_libraries(arsdk_core PUBLIC arvault)

add_library(arcrypto STATIC
    libraries/crypto/utils.c
    libraries/crypto/sha512.c
    libraries/crypto/ed25519.c
    libraries/crypto/ml_dsa_65.c
    libraries/crypto/ml_kem_768.c
    libraries/crypto/aes_gcm.c
)
alrios_pic(arcrypto)
target_include_directories(arcrypto PUBLIC "${ALRIOS_PUBLIC_INCLUDE_DIR}")
target_link_libraries(arcrypto PUBLIC OpenSSL::Crypto)

add_library(arpolicy STATIC libraries/policy/profiles.c)
alrios_pic(arpolicy)
target_include_directories(arpolicy PUBLIC "${ALRIOS_PUBLIC_INCLUDE_DIR}")
target_link_libraries(arpolicy PUBLIC arcrypto)

set(ALRIOS_IPC_V2_SOURCES libraries/ipc/http_serializer.c)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND ALRIOS_IPC_V2_SOURCES
        libraries/ipc/ipc_channel.c
        libraries/ipc/epoll_multiplexer.c
    )
else()
    list(APPEND ALRIOS_IPC_V2_SOURCES
        libraries/ipc/ipc_channel_unsupported.c
        libraries/ipc/epoll_multiplexer_unsupported.c
    )
endif()
add_library(aripc_v2 STATIC ${ALRIOS_IPC_V2_SOURCES})
alrios_pic(aripc_v2)
target_include_directories(aripc_v2 PUBLIC "${ALRIOS_PUBLIC_INCLUDE_DIR}")

add_library(arpki STATIC
    libraries/pki/manifest_canon.c
    libraries/pki/preimage.c
    libraries/pki/certificate.c
    libraries/pki/issuer.c
    libraries/pki/notary.c
    libraries/pki/notary_isolation.c
)
alrios_pic(arpki)
target_include_directories(arpki PUBLIC "${ALRIOS_PUBLIC_INCLUDE_DIR}" "${ALRIOS_HAL_INCLUDE_DIR}")
target_link_libraries(arpki PUBLIC arcrypto arhal_isolation)

add_library(arpackage STATIC libraries/package/arapp_v2_reader.c libraries/package/arapp_v2_signatures.c)
alrios_pic(arpackage)
target_include_directories(arpackage PUBLIC "${ALRIOS_PUBLIC_INCLUDE_DIR}")
target_link_libraries(arpackage PUBLIC arpki)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(ALRIOS_MEMLOADER_SOURCE libraries/loader/memloader.c)
    set(ALRIOS_LOADER_SOURCE libraries/loader/sovereign_loader.c)
else()
    set(ALRIOS_MEMLOADER_SOURCE libraries/loader/memloader_unsupported.c)
    set(ALRIOS_LOADER_SOURCE libraries/loader/sovereign_loader_unsupported.c)
endif()
add_library(armemloader STATIC ${ALRIOS_MEMLOADER_SOURCE})
alrios_pic(armemloader)
target_include_directories(armemloader PUBLIC "${ALRIOS_PUBLIC_INCLUDE_DIR}")

add_library(arloader STATIC ${ALRIOS_LOADER_SOURCE})
alrios_pic(arloader)
target_include_directories(arloader PUBLIC "${ALRIOS_PUBLIC_INCLUDE_DIR}" "${ALRIOS_HAL_INCLUDE_DIR}")
target_link_libraries(arloader PUBLIC arpackage arcrypto arhal OpenSSL::Crypto)

add_library(arsupervisor_policy STATIC libraries/supervisor/policy.c)
alrios_pic(arsupervisor_policy)
target_include_directories(arsupervisor_policy PUBLIC "${ALRIOS_PUBLIC_INCLUDE_DIR}")
target_link_libraries(arsupervisor_policy PUBLIC arpolicy arpackage arpki arcrypto)

add_library(arvault_store STATIC libraries/vault/store.c)
alrios_pic(arvault_store)
target_include_directories(arvault_store PUBLIC "${ALRIOS_PUBLIC_INCLUDE_DIR}")
target_link_libraries(arvault_store PUBLIC arcrypto OpenSSL::Crypto)

add_library(araudit STATIC libraries/audit/hash_chain.c libraries/audit/forwarder.c)
alrios_pic(araudit)
target_include_directories(araudit PUBLIC "${ALRIOS_PUBLIC_INCLUDE_DIR}" "${ALRIOS_HAL_INCLUDE_DIR}")
target_link_libraries(araudit PUBLIC OpenSSL::Crypto OpenSSL::SSL arkernel arcrypto)

add_library(areventbus STATIC libraries/events/broker.c)
alrios_pic(areventbus)
target_include_directories(areventbus PUBLIC "${ALRIOS_PUBLIC_INCLUDE_DIR}")

add_library(arresources STATIC libraries/resources/psi.c)
alrios_pic(arresources)
target_include_directories(arresources PUBLIC "${ALRIOS_PUBLIC_INCLUDE_DIR}")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(ALRIOS_SANDBOX_MOUNTS_SOURCE libraries/sandbox/mounts.c)
else()
    set(ALRIOS_SANDBOX_MOUNTS_SOURCE libraries/sandbox/mounts_unsupported.c)
endif()
add_library(arsandbox_mounts STATIC ${ALRIOS_SANDBOX_MOUNTS_SOURCE})
alrios_pic(arsandbox_mounts)
target_include_directories(arsandbox_mounts PUBLIC "${ALRIOS_PUBLIC_INCLUDE_DIR}")

add_library(arindex STATIC libraries/package/index.c libraries/package/index_writer.c)
alrios_pic(arindex)
target_include_directories(arindex PUBLIC "${ALRIOS_PUBLIC_INCLUDE_DIR}")
target_link_libraries(arindex PUBLIC OpenSSL::Crypto)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(ALRIOS_FD_PASS_SOURCE libraries/deploy/fd_pass.c)
    set(ALRIOS_HANDOVER_SOURCE libraries/supervisor/socket_handover.c)
    set(ALRIOS_DRAIN_SOURCE libraries/deploy/drain.c)
    set(ALRIOS_ATOMIC_SWAP_SOURCE libraries/deploy/atomic_swap.c)
    set(ALRIOS_HOOK_SOURCES libraries/hooks/hooks_executor.c libraries/hooks/lifecycle.c)
else()
    set(ALRIOS_FD_PASS_SOURCE libraries/deploy/fd_pass_unsupported.c)
    set(ALRIOS_HANDOVER_SOURCE libraries/supervisor/socket_handover_unsupported.c)
    set(ALRIOS_DRAIN_SOURCE libraries/deploy/drain_unsupported.c)
    set(ALRIOS_ATOMIC_SWAP_SOURCE libraries/deploy/atomic_swap_unsupported.c)
    set(ALRIOS_HOOK_SOURCES libraries/hooks/hooks_unsupported.c)
endif()

add_library(arfds STATIC ${ALRIOS_FD_PASS_SOURCE})
alrios_pic(arfds)
target_include_directories(arfds PUBLIC "${ALRIOS_PUBLIC_INCLUDE_DIR}")

add_library(arsocket_handover STATIC ${ALRIOS_HANDOVER_SOURCE})
alrios_pic(arsocket_handover)
target_include_directories(arsocket_handover PUBLIC "${ALRIOS_PUBLIC_INCLUDE_DIR}")

add_library(ardeploy STATIC
    ${ALRIOS_ATOMIC_SWAP_SOURCE}
    libraries/deploy/arapp_index.c
    libraries/deploy/layering.c
    ${ALRIOS_DRAIN_SOURCE}
)
alrios_pic(ardeploy)
target_include_directories(ardeploy PUBLIC "${ALRIOS_PUBLIC_INCLUDE_DIR}" "${ALRIOS_HAL_INCLUDE_DIR}")
target_link_libraries(ardeploy PUBLIC arindex arfds arsocket_handover arhal OpenSSL::Crypto)

add_library(arhooks STATIC ${ALRIOS_HOOK_SOURCES})
alrios_pic(arhooks)
target_include_directories(arhooks PUBLIC "${ALRIOS_PUBLIC_INCLUDE_DIR}" "${ALRIOS_HAL_INCLUDE_DIR}")
target_link_libraries(arhooks PUBLIC arkernel ardeploy arhal)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_library(arsandbox_linux STATIC
        libraries/sandbox/seccomp_filter.c
        libraries/sandbox/cgroups_init.c
        libraries/sandbox/ns_unshare.c
    )
    alrios_pic(arsandbox_linux)
    target_include_directories(arsandbox_linux PUBLIC "${ALRIOS_PUBLIC_INCLUDE_DIR}" "${ALRIOS_HAL_INCLUDE_DIR}")
    target_link_libraries(arsandbox_linux PUBLIC arsandbox_mounts armemloader arhal)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(ALRIOS_DEVMODE_JAIL_SOURCE libraries/sandbox/devmode_jail.c)
else()
    set(ALRIOS_DEVMODE_JAIL_SOURCE libraries/sandbox/devmode_jail_unsupported.c)
endif()
add_library(ardevmode_jail STATIC ${ALRIOS_DEVMODE_JAIL_SOURCE})
alrios_pic(ardevmode_jail)
target_include_directories(ardevmode_jail PUBLIC "${ALRIOS_PUBLIC_INCLUDE_DIR}")
target_link_libraries(ardevmode_jail PUBLIC arpolicy)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(ardevmode_jail PUBLIC arsandbox_linux)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(ALRIOS_GUEST_CHANNEL_SOURCE libraries/ipc/guest_channel.c)
    set(ALRIOS_GUEST_ROUTER_SOURCE libraries/supervisor/guest_router.c)
else()
    set(ALRIOS_GUEST_CHANNEL_SOURCE libraries/ipc/guest_channel_unsupported.c)
    set(ALRIOS_GUEST_ROUTER_SOURCE libraries/supervisor/guest_router_unsupported.c)
endif()
add_library(arguest_channel STATIC ${ALRIOS_GUEST_CHANNEL_SOURCE})
alrios_pic(arguest_channel)
target_include_directories(arguest_channel PUBLIC "${ALRIOS_PUBLIC_INCLUDE_DIR}")

add_library(arguest_router STATIC ${ALRIOS_GUEST_ROUTER_SOURCE})
alrios_pic(arguest_router)
target_include_directories(arguest_router PUBLIC "${ALRIOS_PUBLIC_INCLUDE_DIR}")
target_link_libraries(arguest_router PUBLIC arguest_channel)

add_library(arsecurity_epoch STATIC libraries/policy/security_epoch.c)
alrios_pic(arsecurity_epoch)
target_include_directories(arsecurity_epoch PUBLIC "${ALRIOS_PUBLIC_INCLUDE_DIR}")
target_link_libraries(arsecurity_epoch PUBLIC arcrypto OpenSSL::Crypto)

add_library(arsdk_app STATIC libraries/sdk/app.c)
alrios_pic(arsdk_app)
target_include_directories(arsdk_app PUBLIC "${ALRIOS_PUBLIC_INCLUDE_DIR}")
target_link_libraries(arsdk_app PUBLIC arcrypto)

add_library(arsensitive_mem STATIC libraries/memory/sensitive.c libraries/memory/crash_report.c)
alrios_pic(arsensitive_mem)
target_include_directories(arsensitive_mem PUBLIC "${ALRIOS_PUBLIC_INCLUDE_DIR}" "${ALRIOS_HAL_INCLUDE_DIR}")
target_link_libraries(arsensitive_mem PUBLIC arcrypto arkernel)

add_library(arvault_legacy STATIC platform/core/vault_loader.c)
alrios_pic(arvault_legacy)
target_include_directories(arvault_legacy PUBLIC "${ALRIOS_PUBLIC_INCLUDE_DIR}" "${ALRIOS_CORE_DIR}")
target_link_libraries(arvault_legacy PUBLIC arvault)

add_library(arsupervisor STATIC daemons/supervisor/context.c daemons/supervisor/loader.c platform/core/ar_path.c)
alrios_pic(arsupervisor)
target_include_directories(arsupervisor PUBLIC
    "${ALRIOS_PUBLIC_INCLUDE_DIR}"
    "${ALRIOS_CORE_DIR}"
    "${ALRIOS_PLATFORM_INCLUDE_DIR}"
    "${ALRIOS_HAL_INCLUDE_DIR}"
    "${ALRIOS_ARCHIVE_DIR}"
)
target_link_libraries(arsupervisor PUBLIC
    arkernel arlog ararchive armanifest arsupervisor_policy arguest_router ardeploy
    OpenSSL::SSL OpenSSL::Crypto Threads::Threads ${CMAKE_DL_LIBS}
)

add_library(arcore_runtime STATIC platform/core/loader.c)
alrios_pic(arcore_runtime)
target_include_directories(arcore_runtime PUBLIC
    "${ALRIOS_CORE_DIR}"
    "${ALRIOS_PLATFORM_INCLUDE_DIR}"
    "${ALRIOS_HAL_INCLUDE_DIR}"
    "${ALRIOS_ARCHIVE_DIR}"
)
target_link_libraries(arcore_runtime PUBLIC
    arsupervisor
    arkernel arlog ararchive armanifest arvault_legacy arsupervisor_policy arpackage arpki
    OpenSSL::SSL OpenSSL::Crypto Threads::Threads ${CMAKE_DL_LIBS}
)

set(ALRIOS_SDK_MODULE_TARGETS
    arsdk_core arvault arpolicy aripc_v2 arhooks ardeploy arfds arsocket_handover
    arcrypto arpki arsandbox_mounts ardevmode_jail arsdk_app arloader armemloader
    arguest_channel arguest_router arpackage arindex arhal arkernel arlog arhal_isolation
)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND ALRIOS_SDK_MODULE_TARGETS arsandbox_linux)
endif()

set(ALRIOS_SDK_ARCHIVE_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/merge_arsdk_archives.cmake")
file(WRITE "${ALRIOS_SDK_ARCHIVE_SCRIPT}" [=[
if(NOT DEFINED OUTPUT OR NOT DEFINED AR OR NOT DEFINED RANLIB OR NOT DEFINED ARCHIVES)
    message(FATAL_ERROR "Missing SDK archive merge arguments")
endif()
string(REPLACE "|" ";" ARCHIVES "${ARCHIVES}")
file(REMOVE "${OUTPUT}")
set(objects)
set(index 0)
foreach(archive IN LISTS ARCHIVES)
    math(EXPR index "${index} + 1")
    set(extract_dir "${OUTPUT}.objects/${index}")
    file(REMOVE_RECURSE "${extract_dir}")
    file(MAKE_DIRECTORY "${extract_dir}")
    execute_process(COMMAND "${AR}" -x "${archive}"
                    WORKING_DIRECTORY "${extract_dir}"
                    RESULT_VARIABLE extract_result)
    if(NOT extract_result EQUAL 0)
        message(FATAL_ERROR "Failed to extract ${archive}")
    endif()
    file(GLOB extracted "${extract_dir}/*")
    foreach(object IN LISTS extracted)
        get_filename_component(object_name "${object}" NAME)
        file(RENAME "${object}" "${extract_dir}/${index}_${object_name}")
        list(APPEND objects "${extract_dir}/${index}_${object_name}")
    endforeach()
endforeach()
execute_process(COMMAND "${AR}" qc "${OUTPUT}" ${objects} RESULT_VARIABLE archive_result)
if(NOT archive_result EQUAL 0)
    message(FATAL_ERROR "Failed to create ${OUTPUT}")
endif()
if(EXISTS "${RANLIB}")
    execute_process(COMMAND "${RANLIB}" "${OUTPUT}" RESULT_VARIABLE ranlib_result)
    if(NOT ranlib_result EQUAL 0)
        message(FATAL_ERROR "Failed to index ${OUTPUT}")
    endif()
endif()
file(REMOVE_RECURSE "${OUTPUT}.objects")
]=])

set(ALRIOS_SDK_ARCHIVES)
foreach(module_target IN LISTS ALRIOS_SDK_MODULE_TARGETS)
    list(APPEND ALRIOS_SDK_ARCHIVES "$<TARGET_FILE:${module_target}>")
endforeach()
string(JOIN "|" ALRIOS_SDK_ARCHIVES_ARG ${ALRIOS_SDK_ARCHIVES})
set(ALRIOS_SDK_ARCHIVE "${CMAKE_SOURCE_DIR}/arcore/lib/${CMAKE_STATIC_LIBRARY_PREFIX}arsdk${CMAKE_STATIC_LIBRARY_SUFFIX}")
add_custom_command(
    OUTPUT "${ALRIOS_SDK_ARCHIVE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/arcore/lib"
    COMMAND ${CMAKE_COMMAND}
        "-DOUTPUT=${ALRIOS_SDK_ARCHIVE}"
        "-DAR=${CMAKE_AR}"
        "-DRANLIB=${CMAKE_RANLIB}"
        "-DARCHIVES=${ALRIOS_SDK_ARCHIVES_ARG}"
        -P "${ALRIOS_SDK_ARCHIVE_SCRIPT}"
    DEPENDS ${ALRIOS_SDK_MODULE_TARGETS}
    COMMAND_EXPAND_LISTS
    VERBATIM
)
add_custom_target(arsdk DEPENDS "${ALRIOS_SDK_ARCHIVE}")
add_library(arsdk_modules INTERFACE)
target_include_directories(arsdk_modules INTERFACE "${ALRIOS_PUBLIC_INCLUDE_DIR}" "${ALRIOS_PLATFORM_INCLUDE_DIR}" "${ALRIOS_HAL_INCLUDE_DIR}")
target_link_libraries(arsdk_modules INTERFACE ${ALRIOS_SDK_MODULE_TARGETS})
add_custom_target(arsdk_headers ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/arcore/include/alrios"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/include/alrios" "${CMAKE_SOURCE_DIR}/arcore/include/alrios"
    COMMENT "Installing ALRIOS Native SDK headers into arcore/include/alrios"
)

add_library(arcore_control STATIC platform/core/ctl.c platform/core/hotreload.c)
alrios_pic(arcore_control)
target_include_directories(arcore_control PUBLIC "${ALRIOS_CORE_DIR}")
target_link_libraries(arcore_control PUBLIC arcore_runtime ardeploy)

add_executable(arcore platform/core/main.c)
target_include_directories(arcore PRIVATE "${ALRIOS_CORE_DIR}")
target_link_libraries(arcore PRIVATE arcore_control)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(arcore PRIVATE -rdynamic "-Wl,-rpath,$ORIGIN/lib")
endif()
set_target_properties(arcore PROPERTIES ENABLE_EXPORTS TRUE)

add_library(armake_package_v2 STATIC libraries/package/package_v2.c)
target_include_directories(armake_package_v2 PUBLIC "${ALRIOS_PUBLIC_INCLUDE_DIR}")
target_link_libraries(armake_package_v2 PUBLIC arpki)

add_executable(armake commands/armake/main.c commands/armake/armake.c)
target_include_directories(armake PRIVATE commands/armake)
target_link_libraries(armake PRIVATE arkernel ararchive armanifest arhal_isolation armake_package_v2 arindex)

add_executable(alrios commands/alrios/main.c commands/alrios/arpm.c)
target_include_directories(alrios PRIVATE commands/alrios)
target_link_libraries(alrios PRIVATE arkernel ararchive armanifest ardeploy arindex OpenSSL::SSL OpenSSL::Crypto Threads::Threads ${CMAKE_DL_LIBS})
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(alrios PRIVATE "-Wl,-rpath,$ORIGIN/lib")
    target_link_options(armake PRIVATE "-Wl,-rpath,$ORIGIN/lib")
endif()
if(UNIX)
    add_custom_command(TARGET alrios POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink alrios "${CMAKE_SOURCE_DIR}/arcore/arpm"
        COMMENT "Generating arcore/arpm symlink to alrios"
    )
endif()

add_executable(alrios-ca commands/alrios-ca/main.c)
target_link_libraries(alrios-ca PRIVATE arpki arcrypto OpenSSL::Crypto)

add_executable(arsign commands/arsign/main.c commands/arsign/arapp_sign.c)
target_include_directories(arsign PRIVATE commands/arsign)
target_link_libraries(arsign PRIVATE arpki arcrypto arpackage OpenSSL::Crypto)

add_executable(arinstall commands/arinstall/main.c commands/arinstall/installer_core.c commands/arinstall/cli_ui.c)
target_include_directories(arinstall PRIVATE commands/arinstall)
target_link_libraries(arinstall PRIVATE ararchive armanifest arhal)
find_package(PkgConfig QUIET)
if(UNIX AND PkgConfig_FOUND)
    pkg_check_modules(GTK3 gtk+-3.0)
endif()
if(GTK3_FOUND)
    target_sources(arinstall PRIVATE commands/arinstall/gtk3_gui.c)
    target_include_directories(arinstall PRIVATE ${GTK3_INCLUDE_DIRS})
    target_compile_options(arinstall PRIVATE ${GTK3_CFLAGS_OTHER})
    target_link_libraries(arinstall PRIVATE ${GTK3_LIBRARIES})
    target_compile_definitions(arinstall PRIVATE HAS_GTK3=1)
endif()
if(WIN32)
    target_sources(arinstall PRIVATE commands/arinstall/win32_gui.c)
    target_link_libraries(arinstall PRIVATE urlmon ws2_32 comctl32 shell32 user32 gdi32 ole32 advapi32)
    target_compile_definitions(arinstall PRIVATE HAS_WIN32_GUI=1)
endif()

foreach(command_target arcore armake alrios alrios-ca arsign arinstall)
    set_target_properties(${command_target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/arcore"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/arcore"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/arcore"
    )
endforeach()

add_executable(ar_auditd daemons/ar_auditd/main.c)
target_link_libraries(ar_auditd PRIVATE araudit arkernel arcrypto OpenSSL::Crypto OpenSSL::SSL)
add_executable(ar_chronod daemons/ar_chronod/main.c)
target_link_libraries(ar_chronod PRIVATE arsecurity_epoch arkernel arcrypto OpenSSL::Crypto)
add_executable(ar_cored daemons/ar_cored/main.c)
target_link_libraries(ar_cored PRIVATE arsensitive_mem arkernel arcrypto OpenSSL::Crypto OpenSSL::SSL)
add_executable(ar_eventd daemons/ar_eventd/main.c)
target_link_libraries(ar_eventd PRIVATE areventbus arkernel arcrypto)
add_executable(ar_resourced daemons/ar_resourced/main.c)
target_link_libraries(ar_resourced PRIVATE arresources arkernel)

add_executable(arcc commands/arcc/main.c)
target_link_libraries(arcc PRIVATE arkernel arlog OpenSSL::SSL OpenSSL::Crypto Threads::Threads)
set_target_properties(arcc PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/arcore"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/arcore"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/arcore"
)
