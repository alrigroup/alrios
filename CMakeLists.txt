cmake_minimum_required(VERSION 3.10)
project(ALRIOS C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /WX)
    add_compile_definitions(WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE _WINSOCK_DEPRECATED_NO_WARNINGS strcasecmp=_stricmp strncasecmp=_strnicmp strtok_r=strtok_s getcwd=_getcwd chdir=_chdir)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes)
    add_compile_definitions(_POSIX_C_SOURCE=200809L _GNU_SOURCE)
    if(WIN32)
        add_compile_definitions(WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE _WINSOCK_DEPRECATED_NO_WARNINGS strcasecmp=_stricmp strncasecmp=_strnicmp strtok_r=strtok_s getcwd=_getcwd chdir=_chdir)
    endif()
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/arcore")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/arcore")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/arcore")

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

include(cmake/CryptoProviders.cmake)

enable_testing()

add_subdirectory(src/ALRIOS/arkernel)
add_subdirectory(src/ALRIOS/core)
add_subdirectory(src/tools)
add_subdirectory(src/installer)

add_executable(ar_auditd
    src/daemons/ar_auditd/main.c
)
target_include_directories(ar_auditd PRIVATE
    include
    include/alrios
    src/ALRIOS/arkernel/os/include
)
target_link_libraries(ar_auditd PRIVATE araudit arkernel arcrypto OpenSSL::Crypto OpenSSL::SSL)

add_executable(ar_chronod
    src/daemons/ar_chronod/main.c
)
target_include_directories(ar_chronod PRIVATE
    include
    include/alrios
    src/ALRIOS/arkernel/os/include
)
target_link_libraries(ar_chronod PRIVATE arsecurity_epoch arkernel arcrypto OpenSSL::Crypto)

add_executable(ar_cored
    src/daemons/ar_cored/main.c
)
target_include_directories(ar_cored PRIVATE
    include
    include/alrios
    src/ALRIOS/arkernel/os/include
)
target_link_libraries(ar_cored PRIVATE arsensitive_mem arkernel arcrypto OpenSSL::Crypto OpenSSL::SSL)

add_executable(ar_eventd
    src/daemons/ar_eventd/main.c
)
target_include_directories(ar_eventd PRIVATE
    include
    include/alrios
    src/ALRIOS/arkernel/os/include
)
target_link_libraries(ar_eventd PRIVATE areventbus arkernel arcrypto)

add_executable(ar_resourced
    src/daemons/ar_resourced/main.c
)
target_include_directories(ar_resourced PRIVATE
    include
    include/alrios
    src/ALRIOS/arkernel/os/include
)
target_link_libraries(ar_resourced PRIVATE arresources arkernel)

add_executable(arcc
    src/commands/arcc/main.c
)
target_include_directories(arcc PRIVATE
    include
    include/alrios
    src/ALRIOS/arkernel/os/include
)
target_link_libraries(arcc PRIVATE arkernel arlog OpenSSL::SSL OpenSSL::Crypto Threads::Threads)
set_target_properties(arcc PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/arcore"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/arcore"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/arcore"
)

add_subdirectory(tests)
