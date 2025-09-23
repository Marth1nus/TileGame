cmake_minimum_required(VERSION 3.20)
project(TileGame VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
endif()

include(FetchContent)
set(SHARED_DEPENDENCY_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/FetchContent")
FetchContent_Declare(glm GIT_REPOSITORY https://github.com/g-truc/glm GIT_TAG 1.0.1 SOURCE_DIR ${SHARED_DEPENDENCY_SOURCE_DIR}/glm)
FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw GIT_TAG 3.4 SOURCE_DIR ${SHARED_DEPENDENCY_SOURCE_DIR}/glfw)
FetchContent_Declare(box2d GIT_REPOSITORY https://github.com/erincatto/box2d GIT_TAG v3.0.0 SOURCE_DIR ${SHARED_DEPENDENCY_SOURCE_DIR}/box2d)
FetchContent_Declare(entt GIT_REPOSITORY https://github.com/skypjack/entt GIT_TAG v3.14.0 SOURCE_DIR ${SHARED_DEPENDENCY_SOURCE_DIR}/entt)
FetchContent_MakeAvailable(glm box2d entt)
set(LUA_VERSION "5.4.8")
add_subdirectory(vendor/lua)
add_subdirectory(vendor/stb)
add_subdirectory(vendor/imgui)

add_executable(${PROJECT_NAME}
  src/application.cpp
  src/application.hpp
  src/common.cpp
  src/common.hpp
  src/entity.cpp
  src/entity.hpp
  src/main.cpp
  src/render.cpp
  src/render.hpp
  vendor/imgui/imgui/backends/imgui_impl_glfw.cpp
  vendor/imgui/imgui/backends/imgui_impl_opengl3.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm stb box2d imgui lua EnTT::EnTT)

# Enable warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_compile_options(${PROJECT_NAME} PRIVATE
    /W4
    /WX)
  target_compile_definitions(${PROJECT_NAME} PRIVATE
    _CRT_SECURE_NO_WARNINGS)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|Emscripten")
  target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror)
  set_source_files_properties(
    vendor/imgui/imgui/backends/imgui_impl_glfw.cpp
    PROPERTIES COMPILE_OPTIONS
    "$<$<COMPILE_LANGUAGE:CXX>:-w>"
  )

endif()

target_include_directories(TileGame SYSTEM
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/FetchContent/glm
  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/FetchContent/entt/src
  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/imgui
  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/imgui/backends
  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/lua/lua-5.4.8/src
  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/FetchContent/box2d/src
)

if(CMAKE_SYSTEM_NAME MATCHES "Emscripten")
  set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME ${PROJECT_NAME}
    LINK_FLAGS "-s EXPORTED_RUNTIME_METHODS=['setCanvasSize']"
    SUFFIX .html)
  target_link_options(${PROJECT_NAME} PRIVATE
    -sUSE_GLFW=3
    -sUSE_WEBGL2=1
    -sFULL_ES3=1
    -sSTACK_SIZE=1048576 # 1MB aka 2^20
    -sINITIAL_MEMORY=33554432 # 32MB aka 2^25
    --preload-file ${CMAKE_CURRENT_SOURCE_DIR}/res@res
  )
  set(CMAKE_EXECUTABLE_SUFFIX ".html")
  if(DEFINED GITHUB_PAGES_DIR)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory ${GITHUB_PAGES_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG> ${GITHUB_PAGES_DIR}
    )
  endif()

else()
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(glfw)
  target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
  if(WIN32)
    add_definitions(-DUSE_GLAD)
    add_subdirectory(vendor/glad)
    target_link_libraries(${PROJECT_NAME} PRIVATE glad)
  else()
    target_link_libraries(${PROJECT_NAME} PRIVATE GLESv2 EGL)
  endif()
endif()
