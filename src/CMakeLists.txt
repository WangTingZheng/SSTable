cmake_minimum_required(VERSION 3.2)
project(src)

set(SOURCE_FILES
        main.cc
        block_builder.h
        block_builder.cc
        block.cc
        block.h
        iterator.cc
        ../util/coding.h
        ../util/coding.cc
        ../util/status.cc
        ../util/options.cc
        ../include/options.h
        ../util/comparator.cc
        ../include/slice.h
        ../include/comparator.h
        ../include/env.h
        ../include/options.h
        ../include/iterator.h
        ../util/env_posix.cc

        ../util/env.cc
        )

add_executable(src ${SOURCE_FILES})