cmake_minimum_required(VERSION 4.2)
project(task_1 C)

set(CMAKE_C_STANDARD 11)

add_executable(task_1 main.c)

target_compile_options(task_1 PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)