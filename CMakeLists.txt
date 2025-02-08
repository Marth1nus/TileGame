cmake_minimum_required(VERSION 3.20)
project(TileGame VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(glm   GIT_REPOSITORY https://github.com/g-truc/glm      GIT_TAG master)
FetchContent_Declare(box2d GIT_REPOSITORY https://github.com/erincatto/box2d GIT_TAG v3.0.0)
FetchContent_MakeAvailable(glm box2d)
add_subdirectory(vendor/stb)

add_executable(${PROJECT_NAME}
  src/application.cpp
  src/common.cpp
  src/main.cpp
  src/render.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm stb box2d)

if (CMAKE_SYSTEM_NAME MATCHES "Emscripten")
  set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME ${PROJECT_NAME}
    SUFFIX .html)
  target_link_options(${PROJECT_NAME} PRIVATE
    -sUSE_GLFW=3
    -sUSE_WEBGL2=1
    -sFULL_ES3=1
    -sSTACK_SIZE=1048576 # 1MB aka 2^20
    -sINITIAL_MEMORY=33554432 # 32MB aka 2^25
    --preload-file ${CMAKE_CURRENT_SOURCE_DIR}/res@res
  )
  
else()
  FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG master)
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