cmake_minimum_required(VERSION 4.3)
project(weather C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(weather main.c json/cJSON.c json/cJSON.h)

if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(weather PRIVATE -Wall -Wextra -Wpedantic)
endif ()

# Подключение libcurl
find_package(CURL REQUIRED)
target_link_libraries(weather CURL::libcurl)