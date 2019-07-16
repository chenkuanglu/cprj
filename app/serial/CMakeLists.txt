aux_source_directory(. SUBDIR_SERIAL_SRCS)
add_executable(serial ${SUBDIR_SERIAL_SRCS})
target_link_libraries(serial clib)
