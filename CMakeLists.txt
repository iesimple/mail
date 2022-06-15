cmake_minimum_required(VERSION 3.5)

PROJECT(main)
set(SOURCE_FILE base64.c base64.h main.c)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/bin)

add_executable(main ${SOURCE_FILE})