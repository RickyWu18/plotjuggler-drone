cmake_minimum_required(VERSION 3.22)
project(plotjuggler_drone_pj4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(PJ_BUILD_TESTS "Build unit tests" ON)

find_package(plotjuggler_sdk REQUIRED COMPONENTS plugin_sdk)
find_package(nlohmann_json REQUIRED)

if(MSVC)
  set(PJ_WARNING_FLAGS /W4 /WX /utf-8)
else()
  set(PJ_WARNING_FLAGS -Wall -Wextra -Werror)
endif()

# Plugins land next to each other so the host can scan one directory.
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
foreach(cfg ${CMAKE_CONFIGURATION_TYPES})
  string(TOUPPER ${cfg} CFG)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG} ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG} ${CMAKE_BINARY_DIR}/bin)
endforeach()

if(PJ_BUILD_TESTS)
  find_package(GTest REQUIRED)
  enable_testing()
endif()

# One subdirectory per plugin under plugins/ (each has its own CMakeLists.txt).
file(GLOB plugin_dirs RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/plugins ${CMAKE_CURRENT_SOURCE_DIR}/plugins/*)
foreach(dir ${plugin_dirs})
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/plugins/${dir}/CMakeLists.txt)
    add_subdirectory(plugins/${dir})
  endif()
endforeach()
