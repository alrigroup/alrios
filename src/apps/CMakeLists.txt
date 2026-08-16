# ------------------------------------------------------------------
# ALRIOS Apps — build consolidado (modo novo)
# Todas as apps (web + infra nativa) são definidas por .arappmake.
# Compilação nativa fica aqui, no sistema; NÃO há CMakeLists.txt
# dentro de cada app.
# ------------------------------------------------------------------

set(ARCORE_DIR "${CMAKE_SOURCE_DIR}/arcore")
set(APPS_STAGING_DIR "${ARCORE_DIR}/.staging")

# ------------------------------------------------------------------
# ARWN — núcleo 100% C (lib + CLI + testes)
# ------------------------------------------------------------------
set(ARWN_STAGING "${ARCORE_DIR}/.staging/arwn")
set(ARWN_SRC "${CMAKE_CURRENT_SOURCE_DIR}/arwn")

# Hardening obrigatório em TODOS os targets da ARWN (Fase 0.1)
set(ARWN_HARDEN_FLAGS
    -O2
    -Wall -Wextra
    -D_FORTIFY_SOURCE=3
    -fstack-protector-strong
    -fstack-clash-protection
    -fno-strict-overflow
    -fwrapv
)
set(ARWN_HARDEN_LINK
    -Wl,-z,relro
    -Wl,-z,now
    -Wl,-z,noexecstack
)

# Embutir runtime/arwn-bridge.js no binário (gerar header hex, sem escaping)
file(READ "${ARWN_SRC}/runtime/arwn-bridge.js"
     ARWN_BRIDGE_HEX HEX)
string(REGEX REPLACE "([0-9a-fA-F][0-9a-fA-F])" "0x\\1,"
       ARWN_BRIDGE_BYTES "${ARWN_BRIDGE_HEX}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/arwn_bridge_embed.h"
     "static const unsigned char arwn_bridge_js[] = { ${ARWN_BRIDGE_BYTES} };\n"
     "static const size_t arwn_bridge_js_len = sizeof(arwn_bridge_js);\n")
add_library(arwn_lib STATIC
    ${ARWN_SRC}/core/arwn_config.c
    ${ARWN_SRC}/core/arwn_pack.c
    ${ARWN_SRC}/core/arwn_core.c
    ${ARWN_SRC}/core/arwn_builder.c
    ${ARWN_SRC}/core/arwn_obfuscator.c
    ${ARWN_SRC}/core/arwn_http.c
    ${ARWN_SRC}/core/arwn_server.c
    ${ARWN_SRC}/core/arwn_gateway.c
)
target_include_directories(arwn_lib PUBLIC
    ${ARWN_SRC}/include
    ${ARWN_SRC}/core
)
target_include_directories(arwn_lib PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/arkernel/include
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/arkernel/os/include
)
target_compile_options(arwn_lib PRIVATE ${ARWN_HARDEN_FLAGS})
target_link_options(arwn_lib PRIVATE ${ARWN_HARDEN_LINK})
target_link_libraries(arwn_lib PUBLIC arkernel)
set_property(TARGET arwn_lib PROPERTY POSITION_INDEPENDENT_CODE ON)

add_executable(arwn_build ${ARWN_SRC}/tools/arwn_build.c)
target_include_directories(arwn_build PRIVATE
    ${ARWN_SRC}/include
)
target_compile_options(arwn_build PRIVATE ${ARWN_HARDEN_FLAGS})
target_link_options(arwn_build PRIVATE ${ARWN_HARDEN_LINK})
target_link_libraries(arwn_build PRIVATE arwn_lib)
set_target_properties(arwn_build PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${ARWN_STAGING}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${ARWN_STAGING}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${ARWN_STAGING}"
)

add_executable(arwn_tests
    ${ARWN_SRC}/tests/test_core.c
)
target_include_directories(arwn_tests PRIVATE
    ${ARWN_SRC}/include
    ${ARWN_SRC}/core
)
target_compile_options(arwn_tests PRIVATE ${ARWN_HARDEN_FLAGS})
target_link_options(arwn_tests PRIVATE ${ARWN_HARDEN_LINK})
target_link_libraries(arwn_tests PRIVATE arwn_lib)
set_target_properties(arwn_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${ARWN_STAGING}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${ARWN_STAGING}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${ARWN_STAGING}"
)

option(ARWN_SANITIZE "Build ARWN tests with ASan/UBSan" OFF)
if(ARWN_SANITIZE)
    add_executable(arwn_tests_san ${ARWN_SRC}/tests/test_core.c)
    target_include_directories(arwn_tests_san PRIVATE
        ${ARWN_SRC}/include
        ${ARWN_SRC}/core
    )
    target_compile_options(arwn_tests_san PRIVATE
        ${ARWN_HARDEN_FLAGS} -fsanitize=address,undefined -fno-omit-frame-pointer -g
    )
    target_link_options(arwn_tests_san PRIVATE
        ${ARWN_HARDEN_LINK} -fsanitize=address,undefined
    )
    target_link_libraries(arwn_tests_san PRIVATE arwn_lib)
    set_target_properties(arwn_tests_san PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${ARWN_STAGING}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${ARWN_STAGING}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${ARWN_STAGING}"
    )
endif()

option(ARWN_FUZZ "Build ARWN fuzz targets (libFuzzer, clang)" OFF)
if(ARWN_FUZZ AND CMAKE_C_COMPILER_ID MATCHES "Clang")
    foreach(fuzz_target fuzz_config fuzz_pack fuzz_http)
        add_executable(arwn_${fuzz_target} ${ARWN_SRC}/tests/${fuzz_target}.c)
        target_include_directories(arwn_${fuzz_target} PRIVATE
            ${ARWN_SRC}/include
            ${ARWN_SRC}/core
        )
        target_compile_options(arwn_${fuzz_target} PRIVATE
            -g -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer
        )
        target_link_options(arwn_${fuzz_target} PRIVATE -fsanitize=fuzzer,address,undefined)
        target_link_libraries(arwn_${fuzz_target} PRIVATE arwn_lib)
        set_target_properties(arwn_${fuzz_target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${ARWN_STAGING}"
            RUNTIME_OUTPUT_DIRECTORY_DEBUG "${ARWN_STAGING}"
            RUNTIME_OUTPUT_DIRECTORY_RELEASE "${ARWN_STAGING}"
        )
    endforeach()
endif()

add_custom_target(arwn_pack ALL
    COMMAND ${CMAKE_COMMAND} -E copy
        ${ARWN_SRC}/arwn.arappmake
        ${ARWN_STAGING}/arwn.arappmake
    COMMAND ${CMAKE_COMMAND} -E copy
        ${ARWN_SRC}/include/arwn.h
        ${ARWN_STAGING}/arwn.h
    COMMAND ${ARCORE_DIR}/armake build
        ${ARWN_STAGING}
        ${ARCORE_DIR}/apps/arwn.arapp
        || ${CMAKE_COMMAND} -E echo "armake check: pack final na Fase 5"
    COMMENT "Packaging arwn.arapp"
)
add_dependencies(arwn_pack arwn_build)

# ------------------------------------------------------------------
# ARWS — gateway MODULE (libarws.arlib)
# ------------------------------------------------------------------
set(ARWS_STAGING "${ARCORE_DIR}/.staging/arws")
set(ARWS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/arws")

add_library(arws MODULE
    ${ARWS_SRC}/arws_entry.c
    ${ARWS_SRC}/arws_gateway.c
    ${ARWS_SRC}/arws_utils.c
    ${ARWS_SRC}/arws_config.c
    ${ARWS_SRC}/arws_ratelimit.c
    ${ARWS_SRC}/arws_session.c
    ${ARWS_SRC}/arws_cache.c
    ${ARWS_SRC}/arws_proxy.c
    ${ARWS_SRC}/arws_stream_proxy.c
    ${ARWS_SRC}/modes.c
    ${ARWS_SRC}/router_ex.c
    ${ARWS_SRC}/registry.c
    ${ARWS_SRC}/dispatcher.c
    ${ARWS_SRC}/server.c
    ${ARWS_SRC}/cJSON.c
)
target_include_directories(arws PRIVATE
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/core
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/arkernel/include
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/arkernel/ipc
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/arkernel/os/include
    ${ARWS_SRC}
    ${ARWS_SRC}/ipc
    ${OPENSSL_INCLUDE_DIR}
)
target_link_libraries(arws PRIVATE arkernel)
set_target_properties(arws PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${ARWS_STAGING}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${ARWS_STAGING}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${ARWS_STAGING}"
    SUFFIX ".arlib"
    PREFIX "lib"
)
if(MSVC)
    set_source_files_properties(${ARWS_SRC}/home_handler.c PROPERTIES COMPILE_FLAGS "/TP")
endif()

add_custom_target(arws_pack ALL
    COMMAND ${CMAKE_COMMAND} -E copy
        ${ARWS_SRC}/arws.arappmake
        ${ARWS_STAGING}/arws.arappmake
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${ARWS_SRC}/web
        ${ARWS_STAGING}/web
    COMMAND ${ARCORE_DIR}/armake build
        ${ARWS_STAGING}
        ${ARCORE_DIR}/apps/arws.arapp
    COMMENT "Packaging arws.arapp"
)
add_dependencies(arws_pack arws armake)

# ------------------------------------------------------------------
# CDN — serviço nativo (compilado via armake build.steps)
# ------------------------------------------------------------------
set(CDN_STAGING "${ARCORE_DIR}/.staging/cdn")
add_custom_target(cdn_pack ALL
    COMMAND ${ARCORE_DIR}/armake build
        ${CMAKE_CURRENT_SOURCE_DIR}/cdn
        ${ARCORE_DIR}/apps/cdn.arapp
        --staging ${CDN_STAGING}
    DEPENDS armake
    COMMENT "Building + packaging cdn via armake"
)

# ------------------------------------------------------------------
# Web apps ARWN-native (entry = cópia de arwn_build)
# ------------------------------------------------------------------
set(ARWN_BUILD_BIN "${ARWN_STAGING}/arwn_build")

set(HOME_WEB_STAGING "${ARCORE_DIR}/.staging/home.web")
add_custom_target(home_web_pack ALL
    COMMAND ${ARCORE_DIR}/armake build
        ${CMAKE_CURRENT_SOURCE_DIR}/home.web
        ${ARCORE_DIR}/apps/home.web.arapp
        --staging ${HOME_WEB_STAGING}
        --arwn-build ${ARWN_BUILD_BIN}
    DEPENDS armake arwn_build
    COMMENT "Building + packaging home.web via armake"
)

set(DETROIT_WEB_STAGING "${ARCORE_DIR}/.staging/detroit.web")
add_custom_target(detroit_web_pack ALL
    COMMAND ${ARCORE_DIR}/armake build
        ${CMAKE_CURRENT_SOURCE_DIR}/detroit.web
        ${ARCORE_DIR}/apps/detroit.web.arapp
        --staging ${DETROIT_WEB_STAGING}
        --arwn-build ${ARWN_BUILD_BIN}
    DEPENDS armake arwn_build
    COMMENT "Building + packaging detroit.web via armake"
)