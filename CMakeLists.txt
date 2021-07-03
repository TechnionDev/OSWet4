cmake_minimum_required(VERSION 3.19)
project(OSWet4)

set(CMAKE_CXX_STANDARD 11)

add_executable(OSWet4Pt1 malloc_1.cpp)
add_executable(OSWet4Pt2 tamuz_tests_hw4_malloc2.cpp malloc_2.cpp)
add_executable(OSWet4Pt3 tests_ariel/test.cpp malloc_3.cpp)
add_executable(OSWet4Pt4 tests_ariel/test4.cpp malloc_4.cpp)