aux_source_directory(. SUBDIR_TEST_SRCS)
add_executable(test ${SUBDIR_TEST_SRCS})
target_link_libraries(test clib)
