cmake_minimum_required(VERSION 4.0)
project(mser)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.c" "include/*.hpp" "include/*.h")

add_executable(mser ${SOURCES} main.cpp)
