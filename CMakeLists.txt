cmake_minimum_required(VERSION 2.6)
project(cprj)

aux_source_directory(./app DIR_SRCS)
aux_source_directory(./core DIR_SRCS)
aux_source_directory(./lib DIR_SRCS)

link_libraries(pthread)
add_executable(cprj ${DIR_SRCS})

#add_library(lib lib/timetick.c)
