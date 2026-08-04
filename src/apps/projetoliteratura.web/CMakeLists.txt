set(PROJETOLITERATURA_WEB_STAGING "${CMAKE_SOURCE_DIR}/arcore/.staging/projetoliteratura.web")

find_program(PROJETOLITERATURA_WEB_NODE NAMES node nodejs)

if(PROJETOLITERATURA_WEB_NODE)
    add_custom_target(projetoliteratura_web_spa
        COMMAND ${PROJETOLITERATURA_WEB_NODE} ${CMAKE_CURRENT_SOURCE_DIR}/web/rebuild.mjs
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/web
        DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/web/package.json
            ${CMAKE_CURRENT_SOURCE_DIR}/web/vite.config.js
            ${CMAKE_CURRENT_SOURCE_DIR}/web/rebuild.mjs
            ${CMAKE_CURRENT_SOURCE_DIR}/web/index.html
            ${CMAKE_CURRENT_SOURCE_DIR}/web/src/main.jsx
            ${CMAKE_CURRENT_SOURCE_DIR}/web/src/projetoliteratura.css
            ${CMAKE_CURRENT_SOURCE_DIR}/web/src/ProjetoLiteratura.jsx
        COMMENT "Building ProjetoLiteratura React SPA (web/dist)"
    )
endif()

add_executable(projetoliteratura_web
    projetoliteratura_server.c
    os/main.c
)
target_include_directories(projetoliteratura_web PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/os/include
)
target_link_libraries(projetoliteratura_web PRIVATE arkernel)

set_target_properties(projetoliteratura_web PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJETOLITERATURA_WEB_STAGING}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${PROJETOLITERATURA_WEB_STAGING}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${PROJETOLITERATURA_WEB_STAGING}"
)

if(WIN32)
    set(PROJETOLITERATURA_WEB_BUILD_BIN "projetoliteratura_web.exe")
    set(PROJETOLITERATURA_WEB_PACK_BIN "projetoliteratura.web.exe")
else()
    set(PROJETOLITERATURA_WEB_BUILD_BIN "projetoliteratura_web")
    set(PROJETOLITERATURA_WEB_PACK_BIN "projetoliteratura_web")
endif()

set(PROJETOLITERATURA_WEB_RENAME_COMMANDS "")
if(WIN32)
    list(APPEND PROJETOLITERATURA_WEB_RENAME_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy
            ${PROJETOLITERATURA_WEB_STAGING}/${PROJETOLITERATURA_WEB_BUILD_BIN}
            ${PROJETOLITERATURA_WEB_STAGING}/${PROJETOLITERATURA_WEB_PACK_BIN})
endif()

add_custom_target(projetoliteratura_web_pack ALL
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/projetoliteratura.web.arappmake
        ${PROJETOLITERATURA_WEB_STAGING}/projetoliteratura.web.arappmake
    ${PROJETOLITERATURA_WEB_RENAME_COMMANDS}
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/web/dist/index.html
        ${PROJETOLITERATURA_WEB_STAGING}/projetoliteratura.arhtml
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/web/dist/assets.index
        ${PROJETOLITERATURA_WEB_STAGING}/assets.index
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/web/dist/assets
        ${PROJETOLITERATURA_WEB_STAGING}/assets
    COMMAND ${CMAKE_SOURCE_DIR}/arcore/armake build
        ${PROJETOLITERATURA_WEB_STAGING}
        ${CMAKE_SOURCE_DIR}/arcore/apps/projetoliteratura.web.arapp
    COMMENT "Packaging projetoliteratura.web.arapp"
)
add_dependencies(projetoliteratura_web_pack projetoliteratura_web_spa projetoliteratura_web armake)
