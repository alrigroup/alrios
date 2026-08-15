add_executable(armake
    armake.c
    zip.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/core/arapp_parser.c
)
target_include_directories(armake PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src/ALRIOS/core)
if(WIN32)
    set_target_properties(armake PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/arcore"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/arcore"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/arcore"
    )
endif()

add_executable(alrios
    alrios.c
)
target_link_libraries(alrios PRIVATE arkernel)
if(WIN32)
    set_target_properties(alrios PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/arcore"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/arcore"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/arcore"
    )
endif()