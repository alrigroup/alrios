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
if(WIN32)
    target_link_libraries(arkernel PUBLIC ws2_32 OpenSSL::SSL OpenSSL::Crypto)
else()
    target_link_libraries(arkernel PUBLIC OpenSSL::SSL OpenSSL::Crypto)
endif()
