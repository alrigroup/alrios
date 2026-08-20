add_executable(arcore
    main.c
    loader.c
    ctl.c
    arapp_parser.c
    ar_path.c
    log.c
    ${CMAKE_SOURCE_DIR}/src/tools/zip.c
)
target_include_directories(arcore PRIVATE
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/arkernel/include
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/arkernel/os/include
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/core
    ${CMAKE_SOURCE_DIR}/src/tools
)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
target_link_libraries(arcore PRIVATE arkernel OpenSSL::SSL OpenSSL::Crypto Threads::Threads ${CMAKE_DL_LIBS})
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(arcore PRIVATE -rdynamic)
endif()
if(WIN32)
    set_target_properties(arcore PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/arcore"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/arcore"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/arcore"
        ENABLE_EXPORTS TRUE
    )
endif()
