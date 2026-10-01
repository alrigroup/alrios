add_executable(armake
    ${CMAKE_SOURCE_DIR}/src/commands/armake/main.c
    ${CMAKE_SOURCE_DIR}/src/commands/armake/package_v2.c
)
target_include_directories(armake PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/core
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/arkernel/os/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/alrios
)
target_link_libraries(armake PRIVATE ararchive armanifest arhal_isolation)
set_target_properties(armake PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/arcore"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/arcore"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/arcore"
)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# alrios CLI with embedded arpm package manager
add_executable(alrios
    alrios.c
    arpm.c
)
target_include_directories(alrios PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/core
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/arkernel/include
)
target_link_libraries(alrios PRIVATE arkernel arlog ararchive armanifest OpenSSL::SSL OpenSSL::Crypto Threads::Threads ${CMAKE_DL_LIBS})

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(alrios PRIVATE "-Wl,-rpath,$ORIGIN/lib")
    target_link_options(armake PRIVATE "-Wl,-rpath,$ORIGIN/lib")
endif()
set_target_properties(alrios PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/arcore"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/arcore"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/arcore"
)

if(UNIX)
    add_custom_command(TARGET alrios POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink alrios "${CMAKE_SOURCE_DIR}/arcore/arpm"
        COMMENT "Generating arcore/arpm symlink to alrios"
    )
endif()

if(WIN32)
    target_link_libraries(alrios PRIVATE ws2_32)
endif()

# Offline Sovereign Certificate Authority CLI (PKI Tier 1/2)
add_executable(alrios-ca
    ${CMAKE_SOURCE_DIR}/src/commands/alrios-ca/main.c
)
target_include_directories(alrios-ca PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/alrios
)
target_link_libraries(alrios-ca PRIVATE arpki arcrypto OpenSSL::Crypto)
set_target_properties(alrios-ca PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/arcore"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/arcore"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/arcore"
)

# Strict C11 Compiler Wrapper & Banned API Scanner (arcc)
add_executable(arcc
    ${CMAKE_SOURCE_DIR}/src/commands/arcc/main.c
)
set_target_properties(arcc PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/arcore"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/arcore"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/arcore"
)
add_custom_command(TARGET arcc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/arcore/scripts/gates"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/scripts/gates/banned_apis.py"
        "${CMAKE_SOURCE_DIR}/arcore/scripts/gates/banned_apis.py"
)
add_executable(arsign
    ${CMAKE_SOURCE_DIR}/src/commands/arsign/main.c
    ${CMAKE_SOURCE_DIR}/src/commands/arsign/arapp_sign.c
)
target_include_directories(arsign PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/alrios
)
target_link_libraries(arsign PRIVATE arpackage arpki arcrypto OpenSSL::Crypto)
set_target_properties(arsign PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/arcore"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/arcore"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/arcore"
)
