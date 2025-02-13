cmake_minimum_required(VERSION 3.20)
project(imgui VERSION 1.0.0 LANGUAGES C CXX)

set(IMGUI_URL "https://github.com/ocornut/imgui")
set(IMGUI_TAG "docking")
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/imgui")

if (NOT EXISTS ${IMGUI_DIR})
  message(STATUS "Clone ${IMGUI_URL} ${IMGUI_TAG}")
  execute_process(COMMAND git clone --branch ${IMGUI_TAG} ${IMGUI_URL}
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                  RESULT_VARIABLE GIT_CLONE_RESULT)
  if (NOT GIT_CLONE_RESULT EQUAL "0")
    message(FATAL_ERROR "[FAIL] Clone ${IMGUI_URL} ${IMGUI_TAG}")
  endif()
endif()

add_library(${PROJECT_NAME} 
  imgui/imgui_demo.cpp
  imgui/imgui_draw.cpp
  imgui/imgui_tables.cpp
  imgui/imgui_widgets.cpp
  imgui/imgui.cpp
)
target_include_directories(${PROJECT_NAME} PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)


