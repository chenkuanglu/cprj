aux_source_directory(. SUBDIR_TEST_SRCS)
add_executable(testlib ${SUBDIR_TEST_SRCS})
target_link_libraries(testlib clib)
