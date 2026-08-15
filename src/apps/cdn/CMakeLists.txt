set(CDN_STAGING "${CMAKE_SOURCE_DIR}/arcore/.staging/cdn")

add_executable(cdn_web
    cdn_server.c
    os/main.c
)
target_include_directories(cdn_web PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/os/include
)
target_link_libraries(cdn_web PRIVATE arkernel)

set_target_properties(cdn_web PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CDN_STAGING}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CDN_STAGING}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CDN_STAGING}"
)

if(WIN32)
    set(CDN_BUILD_BIN "cdn_web.exe")
    set(CDN_PACK_BIN "cdn.web.exe")
else()
    set(CDN_BUILD_BIN "cdn_web")
    set(CDN_PACK_BIN "cdn_web")
endif()

set(CDN_RENAME_COMMANDS "")
if(WIN32)
    list(APPEND CDN_RENAME_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CDN_STAGING}/${CDN_BUILD_BIN}
            ${CDN_STAGING}/${CDN_PACK_BIN})
endif()

add_custom_target(cdn_pack ALL
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/cdn.arappmake
        ${CDN_STAGING}/cdn.arappmake
    ${CDN_RENAME_COMMANDS}
    COMMAND ${CMAKE_SOURCE_DIR}/arcore/armake build
        ${CDN_STAGING}
        ${CMAKE_SOURCE_DIR}/arcore/apps/cdn.arapp
    COMMENT "Packaging cdn.arapp"
)
add_dependencies(cdn_pack cdn_web armake)
