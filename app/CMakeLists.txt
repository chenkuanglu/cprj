aux_source_directory(. SUBDIR_TRIGG_SRCS)
add_executable(trigg ${SUBDIR_TRIGG_SRCS})
target_link_libraries(trigg clib)
