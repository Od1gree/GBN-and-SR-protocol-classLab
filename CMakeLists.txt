cmake_minimum_required(VERSION 3.10)
project(UDP_server C)

set(CMAKE_C_STANDARD 11)

add_executable(UDP_server main.c common.c common.h queue.c queue.h)