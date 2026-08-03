set(ARWS_STAGING "${CMAKE_SOURCE_DIR}/arcore/.staging/arws")

add_library(arws MODULE
    arws_entry.c
    arws_gateway.c
    arws_utils.c
    arws_config.c
    arws_ratelimit.c
    arws_session.c
    arws_cache.c
    arws_proxy.c
    arws_stream_proxy.c
    modes.c
    router_ex.c
    registry.c
    dispatcher.c
    server.c
    cJSON.c
)
target_include_directories(arws PRIVATE
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/core
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/arkernel/include
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/arkernel/ipc
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/arkernel/os/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/ipc
    ${OPENSSL_INCLUDE_DIR}
)
target_link_libraries(arws PRIVATE arkernel)
set_target_properties(arws PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${ARWS_STAGING}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${ARWS_STAGING}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${ARWS_STAGING}"
    SUFFIX ".arlib"
    PREFIX "lib"
)
if(MSVC)
    set_source_files_properties(home_handler.c PROPERTIES COMPILE_FLAGS "/TP")
endif()

add_custom_target(arws_pack ALL
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/arws.arappmake
        ${ARWS_STAGING}/arws.arappmake
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/web
        ${ARWS_STAGING}/web
    COMMAND ${CMAKE_SOURCE_DIR}/arcore/armake build
        ${ARWS_STAGING}
        ${CMAKE_SOURCE_DIR}/arcore/apps/arws.arapp
    COMMENT "Packaging arws.arapp"
)
add_dependencies(arws_pack arws armake)
