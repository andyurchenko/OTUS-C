cmake_minimum_required(VERSION 4.3)
project(asm C)

set(CMAKE_C_STANDARD 11)

add_executable(asm main.c
        list.c
        list.h
        error.h
)
