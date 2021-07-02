cmake_minimum_required(VERSION 3.10)
project(OS_Wet4)

set(CMAKE_CXX_STANDARD 14)

add_executable(OS_Wet4 malloc_3.cpp test.cpp)

if ("${CMAKE_SOURCE_DIR}/test_compile/malloc_3.cpp")
    execute_process(COMMAND rm ${CMAKE_SOURCE_DIR}/test_compile/malloc_3.cpp)
endif ("${CMAKE_SOURCE_DIR}/test_compile/malloc_3.cpp")

execute_process(COMMAND cp ${CMAKE_SOURCE_DIR}/malloc_3.cpp ${CMAKE_SOURCE_DIR}/test_compile/malloc_3.cpp)
execute_process(COMMAND g++ test.cpp malloc_3.cpp RESULT_VARIABLE CMD_ERROR WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test_compile)
if (${CMD_ERROR})
    MESSAGE(SEND_ERROR "Build as Segel Failed:" ${CMD_ERROR})
else ()
    MESSAGE(STATUS "Build as Segel pass")
endif (${CMD_ERROR})
