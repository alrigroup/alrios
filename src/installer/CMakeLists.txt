find_package(PkgConfig QUIET)
pkg_check_modules(GTK3 gtk+-3.0)

add_executable(arinstall
    arinstall.c
    ${CMAKE_SOURCE_DIR}/src/tools/zip.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/core/arapp_parser.c
)
target_include_directories(arinstall PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/tools
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/core
    ${GTK3_INCLUDE_DIRS}
)
if(GTK3_FOUND)
    target_compile_options(arinstall PRIVATE ${GTK3_CFLAGS_OTHER})
    target_link_libraries(arinstall PRIVATE ${GTK3_LIBRARIES})
    target_compile_definitions(arinstall PRIVATE HAS_GTK3=1)
else()
    message(STATUS "GTK3 not found — arinstall will be CLI-only (no GUI)")
endif()
if(WIN32)
    target_link_libraries(arinstall PRIVATE urlmon)
endif()
