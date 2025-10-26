
cmake_minimum_required(VERSION 4.1.2)
project(fmuTemplate)

include(CTest)
enable_testing()

set(LIB_NAME fmuTemplate)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(${LIB_NAME} STATIC
    src/model.c
)

target_include_directories(${LIB_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/fmi3/src
)

# add_subdirectory(test)
add_subdirectory(examples)