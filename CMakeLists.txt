cmake_minimum_required(VERSION 3.20)
project(TileGame)

include(FetchContent)

FetchContent_Declare(glm  GIT_REPOSITORY https://github.com/g-truc/glm GIT_TAG master)
FetchContent_MakeAvailable(glm)
add_subdirectory(vendor/stb)

add_executable(${PROJECT_NAME}
  src/main.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm stb)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)

if (CMAKE_SYSTEM_NAME MATCHES "Emscripten")
  message(STATUS "Emscripten build detected")
  set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}" SUFFIX ".html")
  target_link_options(${PROJECT_NAME} PRIVATE
    -sUSE_GLFW=3 
    -sUSE_WEBGL2=1 
    -sFULL_ES3=1 
    -sINITIAL_MEMORY=33554432
    -std=c++23
    -O3
    --preload-file ${CMAKE_CURRENT_SOURCE_DIR}/res@res
  )
  
else()
  FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG master )
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(glfw)
  target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
  if (WIN32)
    add_definitions(-DUSE_GLAD)
    add_subdirectory(vendor/glad)
  target_link_libraries(${PROJECT_NAME} PRIVATE glad)
endif()

endif()