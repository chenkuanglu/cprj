aux_source_directory(. SUBDIR_TEMPLATE_SRCS)
add_executable(template ${SUBDIR_TEMPLATE_SRCS})
target_link_libraries(template clib)
