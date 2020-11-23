cmake_minimum_required(VERSION 3.10)
project(os_lab_6)

set(CMAKE_CXX_STANDARD 17)


add_executable(out main.cpp)
add_executable(childNode childNode.cpp)
add_library(functions sf.cpp sf.h)

target_link_libraries(functions zmq)
target_link_libraries(out zmq functions)
target_link_libraries(childNode zmq functions)