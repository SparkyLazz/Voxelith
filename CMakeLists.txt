cmake_minimum_required(VERSION 3.25)
project(VoxelEditor CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(
        /W4
        /utf-8
        /Zc:preprocessor
        /MP
    )
endif()

add_executable(VoxelEditor
    src/main.cpp
)
