cmake_minimum_required(VERSION 3.10)
project(ALRIOS C)

if(WIN32)
    add_compile_definitions(WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE _WINSOCK_DEPRECATED_NO_WARNINGS strcasecmp=_stricmp strncasecmp=_strnicmp strtok_r=strtok_s getcwd=_getcwd chdir=_chdir)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/arcore")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/arcore")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/arcore")

add_subdirectory(src/ALRIOS/arkernel)
if(EXISTS "${CMAKE_SOURCE_DIR}/src/ALRIOS/arauth")
    add_subdirectory(src/ALRIOS/arauth)
endif()
add_subdirectory(src/ALRIOS/core)
add_subdirectory(src/tools)
add_subdirectory(src/installer)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

if(EXISTS "${CMAKE_SOURCE_DIR}/src/apps/arwn/tools/arwn_build.c")
    add_executable(arwn_build
        src/apps/arwn/tools/arwn_build.c
        src/apps/arwn/core/arwn_core.c
        src/apps/arwn/core/arwn_server.c
        src/apps/arwn/core/arwn_http.c
        src/apps/arwn/core/arwn_gateway.c
        src/apps/arwn/core/arwn_builder.c
        src/apps/arwn/core/arwn_obfuscator.c
        src/apps/arwn/core/arwn_pack.c
        src/apps/arwn/core/arwn_config.c
    )
    target_include_directories(arwn_build PRIVATE
        src/apps/arwn
        src/apps/arwn/include
        src/apps/arwn/core
        src/ALRIOS/arkernel/include
        src/ALRIOS/arkernel/os/include
    )
    target_link_libraries(arwn_build PRIVATE arkernel OpenSSL::SSL OpenSSL::Crypto)
    if(WIN32)
        target_link_libraries(arwn_build PRIVATE ws2_32)
    endif()
    set_target_properties(arwn_build PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/arcore/.staging/arwn"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/arcore/.staging/arwn"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/arcore/.staging/arwn"
    )
    add_custom_command(TARGET arwn_build POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/src/apps/arwn/include/arwn.h"
            "${CMAKE_SOURCE_DIR}/arcore/.staging/arwn/arwn.h"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/arcore/.staging/arwn/arwn_build"
            "${CMAKE_SOURCE_DIR}/arcore/arwn"
        COMMENT "Copying arwn assets"
    )
endif()
