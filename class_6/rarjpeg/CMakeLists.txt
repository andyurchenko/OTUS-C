cmake_minimum_required(VERSION 4.2)
project(rarjpeg C)

set(CMAKE_C_STANDARD 11)

add_executable(rarjpeg main.c)

target_compile_options(rarjpeg PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)