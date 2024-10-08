cmake_minimum_required(VERSION 3.20.0)
project(glad VERSION 0.1.0 LANGUAGES C)
add_library(glad src/gles2.c)
target_include_directories(glad PUBLIC include)