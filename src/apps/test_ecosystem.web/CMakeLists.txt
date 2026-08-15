set(TEST_ECOSYSTEM_WEB_STAGING "${CMAKE_SOURCE_DIR}/arcore/.staging/test_ecosystem.web")

# Compilação e empacotamento via ARWN
add_custom_target(test_ecosystem_web_arweb
    COMMAND ${CMAKE_SOURCE_DIR}/arcore/.staging/arwn/arwn_build build ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Building ARWN container for test_ecosystem.web"
)

add_executable(test_ecosystem_web
    test_server.c
)
target_include_directories(test_ecosystem_web PRIVATE
    ${CMAKE_SOURCE_DIR}/src/apps/arwn/include
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/arkernel/include
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/arkernel/os/include
)
target_link_libraries(test_ecosystem_web PRIVATE arwn_lib arkernel)

set_target_properties(test_ecosystem_web PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TEST_ECOSYSTEM_WEB_STAGING}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${TEST_ECOSYSTEM_WEB_STAGING}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${TEST_ECOSYSTEM_WEB_STAGING}"
)

add_custom_target(test_ecosystem_web_pack ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/arcore/apps
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:test_ecosystem_web> ${TEST_ECOSYSTEM_WEB_STAGING}/test_ecosystem_web
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/test_ecosystem.web.arappmake ${TEST_ECOSYSTEM_WEB_STAGING}/test_ecosystem.web.arappmake
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/config.arwn ${TEST_ECOSYSTEM_WEB_STAGING}/config.arwn
    COMMAND ${CMAKE_COMMAND} -E make_directory ${TEST_ECOSYSTEM_WEB_STAGING}/build
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/build/main.arweb ${TEST_ECOSYSTEM_WEB_STAGING}/build/main.arweb
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/build/c_engine.arweb ${TEST_ECOSYSTEM_WEB_STAGING}/build/c_engine.arweb
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/build/cpp_engine.arweb ${TEST_ECOSYSTEM_WEB_STAGING}/build/cpp_engine.arweb
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/build/go_engine.arweb ${TEST_ECOSYSTEM_WEB_STAGING}/build/go_engine.arweb
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/build/rust_engine.arweb ${TEST_ECOSYSTEM_WEB_STAGING}/build/rust_engine.arweb
    COMMAND ${CMAKE_COMMAND} -E make_directory ${TEST_ECOSYSTEM_WEB_STAGING}/web
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/web/main.arhtml ${TEST_ECOSYSTEM_WEB_STAGING}/web/main.arhtml
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/web/main.css ${TEST_ECOSYSTEM_WEB_STAGING}/web/main.css
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/web/main.js ${TEST_ECOSYSTEM_WEB_STAGING}/web/main.js
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/units ${TEST_ECOSYSTEM_WEB_STAGING}/units
    COMMAND ${CMAKE_SOURCE_DIR}/arcore/armake build ${TEST_ECOSYSTEM_WEB_STAGING} ${CMAKE_SOURCE_DIR}/arcore/apps/test_ecosystem.web.arapp
    DEPENDS test_ecosystem_web test_ecosystem_web_arweb
    COMMENT "Packaging test_ecosystem.web.arapp"
)
add_dependencies(test_ecosystem_web_pack test_ecosystem_web test_ecosystem_web_arweb armake)
