find_package(PkgConfig QUIET)
if(UNIX)
    pkg_check_modules(GTK3 gtk+-3.0)
endif()

set(INSTALLER_SOURCES
    arinstall.c
    installer_core.c
    cli_ui.c
    ${CMAKE_SOURCE_DIR}/src/tools/zip.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/core/arapp_parser.c
)

if(WIN32)
    list(APPEND INSTALLER_SOURCES win32_gui.c)
endif()

if(GTK3_FOUND)
    list(APPEND INSTALLER_SOURCES gtk3_gui.c)
endif()

add_executable(arinstall ${INSTALLER_SOURCES})

target_include_directories(arinstall PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/tools
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/core
)

if(GTK3_FOUND)
    target_include_directories(arinstall PRIVATE ${GTK3_INCLUDE_DIRS})
    target_compile_options(arinstall PRIVATE ${GTK3_CFLAGS_OTHER})
    target_link_libraries(arinstall PRIVATE ${GTK3_LIBRARIES})
    target_compile_definitions(arinstall PRIVATE HAS_GTK3=1)
endif()

if(WIN32)
    target_link_libraries(arinstall PRIVATE urlmon ws2_32 comctl32 shell32 user32 gdi32 ole32 advapi32)
    target_compile_definitions(arinstall PRIVATE HAS_WIN32_GUI=1)
endif()

set_target_properties(arinstall PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/arcore"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/arcore"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/arcore"
)
