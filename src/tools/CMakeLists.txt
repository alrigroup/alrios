add_executable(armake
    armake.c
    zip.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/core/arapp_parser.c
)
target_include_directories(armake PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src/ALRIOS/core)
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
    zip.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/core/arapp_parser.c
)
target_include_directories(alrios PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/core
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/arkernel/include
)
target_link_libraries(alrios PRIVATE arkernel OpenSSL::SSL OpenSSL::Crypto Threads::Threads ${CMAKE_DL_LIBS})

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
