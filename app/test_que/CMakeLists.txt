aux_source_directory(. SUBDIR_TEST_QUE_SRCS)
add_executable(test_que ${SUBDIR_TEST_QUE_SRCS})
target_link_libraries(test_que clib)
