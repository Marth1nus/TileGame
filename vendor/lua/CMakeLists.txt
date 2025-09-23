cmake_minimum_required(VERSION 3.20)
project(lua-cmake VERSION 1.0.0 LANGUAGES C CXX)

set(LUA_VERSION "5.4.8" CACHE STRING "Version of Lua to download and build")
set(LUA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lua-${LUA_VERSION}")

if(NOT EXISTS ${LUA_DIR})
  set(LUA_TAR "lua-${LUA_VERSION}.tar.gz")
  set(LUA_URL "https://www.lua.org/ftp/${LUA_TAR}")

  message(STATUS "Downloading ${LUA_URL}")
  file(DOWNLOAD ${LUA_URL} ${CMAKE_CURRENT_SOURCE_DIR}/${LUA_TAR} SHOW_PROGRESS)

  message(STATUS "Extracting ${LUA_TAR}")
  execute_process(COMMAND tar zxf ${LUA_TAR} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

  message(STATUS "Removing ${LUA_TAR}")
  file(REMOVE ${LUA_TAR})

  message(STATUS "Writing ${LUA_DIR}/CMakeLists.txt")

  # get sources relative to lua dir
  file(GLOB LUA_SOURCES ${LUA_DIR}/src/*.c)
  set(LUA_SOURCES_RELATIVE "\n")
  foreach(LUA_SOURCE ${LUA_SOURCES})
    file(RELATIVE_PATH LUA_SOURCE_RELATIVE ${LUA_DIR} ${LUA_SOURCE})
    if(LUA_SOURCE_RELATIVE STREQUAL "src/lua.c"
      OR LUA_SOURCE_RELATIVE STREQUAL "src/luac.c")
      set(LUA_SOURCES_RELATIVE "${LUA_SOURCES_RELATIVE}\t# ${LUA_SOURCE_RELATIVE}\n")
    else()
      set(LUA_SOURCES_RELATIVE "${LUA_SOURCES_RELATIVE}\t${LUA_SOURCE_RELATIVE}\n")
    endif()
  endforeach(LUA_SOURCE)

  file(WRITE ${LUA_DIR}/CMakeLists.txt
    "cmake_minimum_required(VERSION 3.10)
project(lua VERSION ${LUA_VERSION} LANGUAGES C CXX)

add_library(\${PROJECT_NAME} ${LUA_SOURCES_RELATIVE})
target_include_directories(\${PROJECT_NAME} PUBLIC src)")

  message(STATUS "Available lua-${LUA_VERSION} (aka lua) in ${LUA_DIR}")
endif()

add_subdirectory(${LUA_DIR})

add_library(lua-${LUA_VERSION} INTERFACE)
target_link_libraries(lua-${LUA_VERSION} INTERFACE lua)

option(LUA_EXECUTABLES "Build lua and luac as executables" Off)
if(LUA_EXECUTABLES)
  # Build interpreter
  add_executable(luai ${LUA_DIR}/src/lua.c)
  target_link_libraries(luai PRIVATE lua)
  set_target_properties(luai PROPERTIES OUTPUT_NAME "lua")
  # Build compiler
  add_executable(luac ${LUA_DIR}/src/luac.c)
  target_link_libraries(luac PRIVATE lua)
endif()
