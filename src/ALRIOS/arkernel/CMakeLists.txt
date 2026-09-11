add_library(arkernel
    os/main.c
    svc/svc_manager.c
    svc/svc_supervisor.c
    svc/svc_health.c
    svc/arkernel_main.c
    ipc/ar_ipc.c
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/core/log.c
)
set_property(TARGET arkernel PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(arkernel PUBLIC
    include
    ipc
    os/include
)
find_package(OpenSSL REQUIRED)
target_include_directories(arkernel PUBLIC ${OPENSSL_INCLUDE_DIR})
set_target_properties(arkernel PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/arcore/lib"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/arcore/lib"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/arcore/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/arcore/lib"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/arcore/lib"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/arcore/lib"
)

if(WIN32)
    target_link_libraries(arkernel PUBLIC ws2_32)
endif()

add_custom_command(TARGET arkernel POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/arcore/include"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/os/include/aros_hal.h"
        "${CMAKE_SOURCE_DIR}/arcore/include/aros_hal.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/include/ar_ipc.h"
        "${CMAKE_SOURCE_DIR}/arcore/include/ar_ipc.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/include/ar_kernel.h"
        "${CMAKE_SOURCE_DIR}/arcore/include/ar_kernel.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/include/ar_svc.h"
        "${CMAKE_SOURCE_DIR}/arcore/include/ar_svc.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/src/ALRIOS/core/log.h"
        "${CMAKE_SOURCE_DIR}/arcore/include/log.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/src/ALRIOS/core/arapp_parser.h"
        "${CMAKE_SOURCE_DIR}/arcore/include/arapp_parser.h"
    COMMENT "Installing ALRIOS kernel SDK headers into arcore/include"
)
