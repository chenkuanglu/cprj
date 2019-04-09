cmake_minimum_required(VERSION 2.6)
project(cprj)

aux_source_directory(./app DIR_SRCS)
aux_source_directory(./core DIR_SRCS)
aux_source_directory(./lib DIR_SRCS)

add_definitions("-DDEBUG")
add_definitions("-DPRINT_ARGS")

link_libraries(pthread)
link_libraries(curl)
link_libraries(m)

add_executable(cprj ${DIR_SRCS})

