set(HOME_WEB_STAGING "${CMAKE_SOURCE_DIR}/arcore/.staging/home.web")

find_program(HOME_WEB_NODE NAMES node nodejs)

if(HOME_WEB_NODE)
    add_custom_target(home_web_spa
        COMMAND ${HOME_WEB_NODE} ${CMAKE_CURRENT_SOURCE_DIR}/web/rebuild.mjs
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/web
        DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/web/package.json
            ${CMAKE_CURRENT_SOURCE_DIR}/web/vite.config.js
            ${CMAKE_CURRENT_SOURCE_DIR}/web/index.html
            ${CMAKE_CURRENT_SOURCE_DIR}/web/rebuild.mjs
            ${CMAKE_CURRENT_SOURCE_DIR}/web/src/main.jsx
            ${CMAKE_CURRENT_SOURCE_DIR}/web/src/App.jsx
            ${CMAKE_CURRENT_SOURCE_DIR}/web/src/i18n.js
            ${CMAKE_CURRENT_SOURCE_DIR}/web/src/styles.css
        COMMENT "Building React SPA (dist/index.html)"
    )
endif()

add_executable(home_web
    home_server.c
    os/main.c
)
target_include_directories(home_web PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/os/include
)
target_link_libraries(home_web PRIVATE arkernel)

set_target_properties(home_web PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${HOME_WEB_STAGING}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${HOME_WEB_STAGING}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${HOME_WEB_STAGING}"
)

if(WIN32)
    set(HOME_WEB_BUILD_BIN "home_web.exe")
    set(HOME_WEB_PACK_BIN "home.web.exe")
else()
    set(HOME_WEB_BUILD_BIN "home_web")
    set(HOME_WEB_PACK_BIN "home_web")
endif()

set(HOME_WEB_RENAME_COMMANDS "")
if(WIN32)
    list(APPEND HOME_WEB_RENAME_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy
            ${HOME_WEB_STAGING}/${HOME_WEB_BUILD_BIN}
            ${HOME_WEB_STAGING}/${HOME_WEB_PACK_BIN})
endif()

add_custom_target(home_web_pack ALL
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/home.web.arappmake
        ${HOME_WEB_STAGING}/home.web.arappmake
    ${HOME_WEB_RENAME_COMMANDS}
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/web/dist/index.html
        ${HOME_WEB_STAGING}/index.arhtml
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/web/dist/assets.index
        ${HOME_WEB_STAGING}/assets.index
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/web/dist/assets
        ${HOME_WEB_STAGING}/assets
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/web/public/robots.txt
        ${HOME_WEB_STAGING}/robots.txt
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/web/public/sitemap.xml
        ${HOME_WEB_STAGING}/sitemap.xml
    COMMAND ${CMAKE_SOURCE_DIR}/arcore/armake build
        ${HOME_WEB_STAGING}
        ${CMAKE_SOURCE_DIR}/arcore/apps/home.web.arapp
    COMMENT "Packaging home.web.arapp"
)
if(HOME_WEB_NODE)
    add_dependencies(home_web_pack home_web_spa home_web armake)
else()
    add_dependencies(home_web_pack home_web armake)
endif()
