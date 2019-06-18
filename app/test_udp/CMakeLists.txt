aux_source_directory(. SUBDIR_TEST_UDP_SRCS)
add_executable(test_udp ${SUBDIR_TEST_UDP_SRCS})
target_link_libraries(test_udp clib)
