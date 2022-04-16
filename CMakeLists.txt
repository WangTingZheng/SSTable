cmake_minimum_required(VERSION 3.2)

project(SSTable)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O3")
set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR})
set(SSTABLE_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)

include_directories(${SSTABLE_INCLUDE_DIR})

add_subdirectory(src)