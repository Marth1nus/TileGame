cmake_minimum_required(VERSION 3.20.0)
project(TileGame VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(lua-cmake GIT_REPOSITORY https://github.com/Marth1nus/lua-cmake GIT_TAG master )
FetchContent_Declare(glm       GIT_REPOSITORY https://github.com/g-truc/glm          GIT_TAG master )
FetchContent_Declare(entt      GIT_REPOSITORY https://github.com/skypjack/entt       GIT_TAG master )
FetchContent_MakeAvailable(lua-cmake glm entt)

add_executable(${PROJECT_NAME} 
  src/main.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE lua glm::glm EnTT::EnTT)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)

if (CMAKE_SYSTEM_NAME MATCHES "Emscripten")
  target_link_options(${PROJECT_NAME} PUBLIC -sUSE_GLFW=3 -sUSE_WEBGL2=1)
  set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME} SUFFIX ".html")

else()
  include(FetchContent)
  FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG master )
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(glfw)
  add_subdirectory(vendor/glad)
  target_link_libraries(${PROJECT_NAME} PRIVATE glfw glad)

endif()