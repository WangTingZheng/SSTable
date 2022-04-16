cmake_minimum_required(VERSION 3.2)
project(src)

set(SOURCE_FILES
        main.cc
        block_builder.h
        block_builder.cc
        block.cc
        block.h
        ../util/coding.h
        ../util/coding.cc
        ../include/slice.h
)

add_executable(src ${SOURCE_FILES})