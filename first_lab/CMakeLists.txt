cmake_minimum_required(VERSION 3.15)
project(first_lab C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_FLAGS "-lncurses")
add_executable(first_lab main.c)