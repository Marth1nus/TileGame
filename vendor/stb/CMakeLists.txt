cmake_minimum_required(VERSION 3.20)
project(stb VERSION 1.0.0 LANGUAGES C)

set(STB_URL "https://github.com/nothings/stb.git")
set(STB_TAG "master")
set(STB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/stb")

if (NOT EXISTS ${STB_DIR})
  message(STATUS "Clone ${STB_URL} ${STB_TAG}")
  execute_process(COMMAND git clone --branch ${STB_TAG} ${STB_URL}
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                  RESULT_VARIABLE GIT_CLONE_RESULT)
  if (NOT GIT_CLONE_RESULT EQUAL "0")
    message(FATAL_ERROR "[FAIL] Clone ${STB_URL} ${STB_TAG}")
  endif()
endif()

add_library(${PROJECT_NAME} stb.c)
target_include_directories(${PROJECT_NAME} PUBLIC ${STB_DIR})


