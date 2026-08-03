cmake_minimum_required(VERSION 3.10)
project(ALRIOS C)

if(WIN32)
    add_compile_definitions(WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE _WINSOCK_DEPRECATED_NO_WARNINGS strcasecmp=_stricmp strncasecmp=_strnicmp strtok_r=strtok_s getcwd=_getcwd chdir=_chdir)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/arcore")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/arcore")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/arcore")

add_subdirectory(src/ALRIOS/arkernel)
add_subdirectory(src/ALRIOS/core)
add_subdirectory(src/tools)
add_subdirectory(src/apps)
if(NOT WIN32)
    add_subdirectory(src/installer)
endif()
