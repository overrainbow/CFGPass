cmake_minimum_required(VERSION 3.1)

find_package(LLVM 3.9 REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

add_subdirectory(src cfgpass)
