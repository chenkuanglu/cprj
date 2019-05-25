cmake_minimum_required(VERSION 2.6)

project(apps)
aux_source_directory(lib DIR_MOD_SRCS)
aux_source_directory(core DIR_MOD_SRCS)
aux_source_directory(timer DIR_MOD_SRCS)

link_libraries(pthread)
link_libraries(curl)
link_libraries(m)

add_definitions("-DDEBUG")
add_definitions("-DPRINT_ARGS")

SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

add_library(clib ${DIR_MOD_SRCS})

add_subdirectory(app)

