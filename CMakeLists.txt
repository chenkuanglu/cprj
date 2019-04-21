cmake_minimum_required(VERSION 2.6)
project(cprj)

aux_source_directory(./app DIR_SRCS)
aux_source_directory(./core DIR_SRCS)
aux_source_directory(./lib DIR_SRCS)
aux_source_directory(./timer DIR_SRCS)

add_definitions("-DDEBUG")
add_definitions("-DPRINT_ARGS")

link_libraries(pthread)
link_libraries(curl)
link_libraries(m)

SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

add_executable(cprj ${DIR_SRCS})

