add_library(arvault_legacy STATIC
    vault_loader.c
)
target_include_directories(arvault_legacy PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/alrios
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(arcore
    main.c
    loader.c
    ctl.c
    hotreload.c
    ar_path.c
)
target_include_directories(arcore PRIVATE
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/arkernel/include
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/arkernel/os/include
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/core
    ${CMAKE_SOURCE_DIR}/src/tools
)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
target_link_libraries(arcore PRIVATE arkernel arlog ararchive armanifest arvault_legacy OpenSSL::SSL OpenSSL::Crypto Threads::Threads ${CMAKE_DL_LIBS})
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(arcore PRIVATE -rdynamic "-Wl,-rpath,$ORIGIN/lib")
endif()
set_target_properties(arcore PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/arcore"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/arcore"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/arcore"
    ENABLE_EXPORTS TRUE
)

if(WIN32)
    target_link_libraries(arcore PRIVATE ws2_32)
endif()
