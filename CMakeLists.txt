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

add_subdirectory(src/ALRIOS/arkernel)
add_subdirectory(src/ALRIOS/core)
add_subdirectory(src/tools)
add_subdirectory(src/installer)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
