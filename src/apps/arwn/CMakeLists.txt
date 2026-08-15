set(ARWN_STAGING "${CMAKE_SOURCE_DIR}/arcore/.staging/arwn")

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

# ------------------------------------------------------------------
# libarwn — núcleo 100% C
# ------------------------------------------------------------------
# Embutir runtime/arwn-bridge.js no binário (gerar header hex, sem escaping)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/runtime/arwn-bridge.js"
     ARWN_BRIDGE_HEX HEX)
string(REGEX REPLACE "([0-9a-fA-F][0-9a-fA-F])" "0x\\1,"
       ARWN_BRIDGE_BYTES "${ARWN_BRIDGE_HEX}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/arwn_bridge_embed.h"
     "static const unsigned char arwn_bridge_js[] = { ${ARWN_BRIDGE_BYTES} };\n"
     "static const size_t arwn_bridge_js_len = sizeof(arwn_bridge_js);\n")
add_library(arwn_lib STATIC
    core/arwn_config.c
    core/arwn_pack.c
    core/arwn_core.c
    core/arwn_builder.c
    core/arwn_obfuscator.c
    core/arwn_http.c
    core/arwn_server.c
    core/arwn_gateway.c
)
target_include_directories(arwn_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/core
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

# ------------------------------------------------------------------
# CLI
# ------------------------------------------------------------------
add_executable(arwn_build tools/arwn_build.c)
target_include_directories(arwn_build PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_options(arwn_build PRIVATE ${ARWN_HARDEN_FLAGS})
target_link_options(arwn_build PRIVATE ${ARWN_HARDEN_LINK})
target_link_libraries(arwn_build PRIVATE arwn_lib)
set_target_properties(arwn_build PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${ARWN_STAGING}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${ARWN_STAGING}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${ARWN_STAGING}"
)

# ------------------------------------------------------------------
# Testes de unidade (Fase 0.5 gates)
# ------------------------------------------------------------------
add_executable(arwn_tests
    tests/test_core.c
)
target_include_directories(arwn_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/core
)
target_compile_options(arwn_tests PRIVATE ${ARWN_HARDEN_FLAGS})
target_link_options(arwn_tests PRIVATE ${ARWN_HARDEN_LINK})
target_link_libraries(arwn_tests PRIVATE arwn_lib)
set_target_properties(arwn_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${ARWN_STAGING}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${ARWN_STAGING}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${ARWN_STAGING}"
)

# ------------------------------------------------------------------
# Target sanitizer (ASan/UBSan) — build separado para CI
# ------------------------------------------------------------------
option(ARWN_SANITIZE "Build ARWN tests with ASan/UBSan" OFF)
if(ARWN_SANITIZE)
    add_executable(arwn_tests_san tests/test_core.c)
    target_include_directories(arwn_tests_san PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/core
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

# ------------------------------------------------------------------
# Fuzz targets (libFuzzer) — apenas com clang
# ------------------------------------------------------------------
option(ARWN_FUZZ "Build ARWN fuzz targets (libFuzzer, clang)" OFF)
if(ARWN_FUZZ AND CMAKE_C_COMPILER_ID MATCHES "Clang")
    foreach(fuzz_target fuzz_config fuzz_pack fuzz_http)
        add_executable(arwn_${fuzz_target} tests/${fuzz_target}.c)
        target_include_directories(arwn_${fuzz_target} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/core
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

# ------------------------------------------------------------------
# Packaging .arapp (Fase 5/armake)
# ------------------------------------------------------------------
add_custom_target(arwn_pack ALL
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/arwn.arappmake
        ${ARWN_STAGING}/arwn.arappmake
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/include/arwn.h
        ${ARWN_STAGING}/arwn.h
    COMMAND ${CMAKE_SOURCE_DIR}/arcore/armake build
        ${ARWN_STAGING}
        ${CMAKE_SOURCE_DIR}/arcore/apps/arwn.arapp
        || ${CMAKE_COMMAND} -E echo "armake check: pack final na Fase 5"
    COMMENT "Packaging arwn.arapp"
)
add_dependencies(arwn_pack arwn_build)