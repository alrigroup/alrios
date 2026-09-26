# ====================================================================
# ALRIOS Master Test Suite Registration (CTest)
# ====================================================================

enable_testing()

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/alrios
    ${CMAKE_SOURCE_DIR}/src/ALRIOS/core
    ${CMAKE_SOURCE_DIR}/src/tools
)

set(TEST_OUTPUT_DIR "${CMAKE_BINARY_DIR}/tests_bin")

macro(register_alrios_test test_target test_source)
    add_executable(${test_target} ${test_source})
    target_link_libraries(${test_target} PRIVATE arsdk arkernel OpenSSL::SSL OpenSSL::Crypto Threads::Threads)
    set_target_properties(${test_target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${TEST_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${TEST_OUTPUT_DIR}"
    )
    add_test(NAME ${test_target} COMMAND ${test_target})
endmacro()

register_alrios_test(test_crypto_utils test_crypto_utils.c)
register_alrios_test(test_sha512_kat test_sha512_kat.c)
register_alrios_test(test_ed25519_verify test_ed25519_verify.c)
register_alrios_test(test_ml_dsa_65 test_ml_dsa_65.c)
register_alrios_test(test_aes_gcm_decrypt test_aes_gcm_decrypt.c)
register_alrios_test(test_arapp_format test_arapp_format.c)
register_alrios_test(test_ipc_channel test_ipc_channel.c)
register_alrios_test(test_epoll_multiplexer test_epoll_multiplexer.c)
register_alrios_test(test_http_serializer test_http_serializer.c)
register_alrios_test(test_memfd_creation test_memfd_creation.c)
register_alrios_test(test_memfd_seals test_memfd_seals.c)
register_alrios_test(test_vault test_vault.c)
register_alrios_test(tc_vault_006 tc_vault_006.c)
register_alrios_test(test_hooks test_hooks.c)
register_alrios_test(test_fd_pass test_fd_pass.c)
register_alrios_test(test_atomic_swap test_atomic_swap.c)
register_alrios_test(test_arapp_index test_arapp_index.c)
register_alrios_test(test_profiles test_profiles.c)
register_alrios_test(test_ar_sdk test_ar_sdk.c)
register_alrios_test(test_seccomp_filter test_seccomp_filter.c)
register_alrios_test(test_cgroups_init test_cgroups_init.c)
register_alrios_test(test_ns_unshare test_ns_unshare.c)
register_alrios_test(test_manifest_canon test_manifest_canon.c)
register_alrios_test(test_preimage test_preimage.c)
