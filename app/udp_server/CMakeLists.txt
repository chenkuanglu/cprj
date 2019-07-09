aux_source_directory(. SUBDIR_UDP_SERVER_SRCS)
add_executable(udp_server ${SUBDIR_UDP_SERVER_SRCS})
target_link_libraries(udp_server clib)
