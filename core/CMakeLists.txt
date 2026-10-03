# pj_drone_core — host-independent building blocks shared by the drone plugins (PJ3 and PJ4).
#
# Pure C++20: no Qt, no PlotJuggler SDK. Anything that needs either belongs in the plugin
# that consumes this library, not here.
cmake_minimum_required(VERSION 3.22)

if(NOT DEFINED PJ_WARNING_FLAGS)
  if(MSVC)
    set(PJ_WARNING_FLAGS /W4 /WX /utf-8)
  else()
    set(PJ_WARNING_FLAGS -Wall -Wextra -Werror)
  endif()
endif()

add_library(pj_drone_core STATIC
  src/ardupilot/parser.cpp
  src/ardupilot/log_tools.cpp
  src/io/mapped_file.cpp
  src/io/serial_ports.cpp
  src/mavlink/decoder.cpp)
add_library(pj_drone::core ALIAS pj_drone_core)

# MAVLink C headers (header-only). Private: no public header exposes MAVLink types.
set(PJ_MAVLINK_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/c_library_v2)
target_include_directories(pj_drone_core SYSTEM PRIVATE ${PJ_MAVLINK_INCLUDE_DIR})
target_include_directories(pj_drone_core PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(pj_drone_core PUBLIC cxx_std_20)
target_compile_options(pj_drone_core PRIVATE ${PJ_WARNING_FLAGS})
set_target_properties(pj_drone_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(PJ_BUILD_TESTS)
  find_package(GTest REQUIRED)
  add_executable(pj_drone_core_test
    tests/ardupilot_parser_test.cpp
    tests/log_tools_test.cpp
    tests/mavlink_decoder_test.cpp)
  target_compile_options(pj_drone_core_test PRIVATE ${PJ_WARNING_FLAGS})
  target_include_directories(pj_drone_core_test SYSTEM PRIVATE ${PJ_MAVLINK_INCLUDE_DIR})
  target_link_libraries(pj_drone_core_test PRIVATE pj_drone::core GTest::gtest_main)
  add_test(NAME pj_drone_core_test COMMAND pj_drone_core_test)
endif()
