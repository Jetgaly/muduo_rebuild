cmake_minimum_required(VERSION 3.28)
project(muduo_rebuild)

set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

#Debug
set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_CXX_STANDARD 11)

aux_source_directory(./ SRC_LIST)


add_library(muduo_rebuild SHARED ${SRC_LIST})