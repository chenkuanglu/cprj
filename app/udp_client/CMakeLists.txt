aux_source_directory(. SUBDIR_UDP_CLIENT_SRCS)
add_executable(udp_client ${SUBDIR_UDP_CLIENT_SRCS})
target_link_libraries(udp_client clib)
