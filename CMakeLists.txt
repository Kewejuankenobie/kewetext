
cmake_minimum_required(VERSION 3.30)
project(kewetext C)

set(CMAKE_C_STANDARD 11)

add_compile_options(-lconfig)

add_executable(kewetext main.c
        configuration.h
        configuration.c
        stack.h
        stack.c)
