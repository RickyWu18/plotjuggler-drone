# pj_drone_core — host-independent building blocks shared by the drone plugins (PJ3 and PJ4).
#
# Pure C++17 (pj3 builds as C++17; pj4 uses a newer standard): no Qt, no PlotJuggler SDK. Anything that
# needs either belongs in the plugin that consumes this library, not here.
cmake_minimum_required(VERSION 3.22)

if(NOT DEFINED PJ_WARNING_FLAGS)
  if(MSVC)
    set(PJ_WARNING_FLAGS /W4 /WX /utf-8)
  else()
    set(PJ_WARNING_FLAGS -Wall -Wextra -Werror)
  endif()
endif()

# Built-in flight-mode names, generated from the MAVLink dialect (see scripts/gen_mode_tables.py).
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(PJ_MODE_TABLES_XML ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/c_library_v2/message_definitions/ardupilotmega.xml)
set(PJ_MODE_TABLES_INC ${CMAKE_CURRENT_BINARY_DIR}/generated/pj_drone_mode_tables.inc)
add_custom_command(
  OUTPUT ${PJ_MODE_TABLES_INC}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/generated
  COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/scripts/gen_mode_tables.py
          ${PJ_MODE_TABLES_XML} ${PJ_MODE_TABLES_INC}
  DEPENDS ${PJ_MODE_TABLES_XML} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/gen_mode_tables.py
  COMMENT "Generating flight-mode tables from ardupilotmega.xml"
  VERBATIM)

add_library(pj_drone_core STATIC
  src/ardupilot/parser.cpp
  src/ardupilot/deferred_modes.cpp
  src/ardupilot/log_tools.cpp
  src/ardupilot/mode_xml.cpp
  src/ardupilot/vehicle.cpp
  ${PJ_MODE_TABLES_INC}
  src/io/mapped_file.cpp
  src/io/serial_ports.cpp
  src/util/sha256.cpp
  src/mavlink/decoder.cpp)
add_library(pj_drone::core ALIAS pj_drone_core)

# MAVLink C headers (header-only). Private: no public header exposes MAVLink types.
set(PJ_MAVLINK_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/c_library_v2)
target_include_directories(pj_drone_core SYSTEM PRIVATE ${PJ_MAVLINK_INCLUDE_DIR})
target_include_directories(pj_drone_core PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_include_directories(pj_drone_core PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(pj_drone_core PUBLIC cxx_std_17)
target_compile_options(pj_drone_core PRIVATE ${PJ_WARNING_FLAGS})
set_target_properties(pj_drone_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Optional: reading a user's MAVLink XML (parseMavlinkModeXml) needs pugixml. Builds that provide it (PJ4, through
# Conan) get the feature; a build without it (PJ3) compiles the same sources and the call fails with an error.
if(NOT TARGET pugixml::pugixml)
  find_package(pugixml QUIET)
endif()
if(TARGET pugixml::pugixml)
  # The dependency is private, but a static library hands it on to its consumers: make the target visible to them.
  get_target_property(_pj_pugixml_imported pugixml::pugixml IMPORTED)
  if(_pj_pugixml_imported)
    set_target_properties(pugixml::pugixml PROPERTIES IMPORTED_GLOBAL TRUE)
  endif()
  target_link_libraries(pj_drone_core PRIVATE pugixml::pugixml)
  target_compile_definitions(pj_drone_core PUBLIC PJ_DRONE_HAS_MODE_XML)
  message(STATUS "pj_drone_core: MAVLink XML mode files enabled (pugixml found)")
else()
  message(STATUS "pj_drone_core: MAVLink XML mode files disabled (pugixml not found)")
endif()

if(PJ_BUILD_TESTS)
  find_package(GTest REQUIRED)
  add_executable(pj_drone_core_test
    tests/ardupilot_parser_test.cpp
    tests/log_tools_test.cpp
    tests/mavlink_decoder_test.cpp
    tests/mode_xml_test.cpp
    tests/sha256_test.cpp
    tests/vehicle_test.cpp)
  target_compile_definitions(pj_drone_core_test PRIVATE PJ_ARDUPILOTMEGA_XML="${PJ_MODE_TABLES_XML}")
  target_compile_options(pj_drone_core_test PRIVATE ${PJ_WARNING_FLAGS})
  target_include_directories(pj_drone_core_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_include_directories(pj_drone_core_test SYSTEM PRIVATE ${PJ_MAVLINK_INCLUDE_DIR})
  target_link_libraries(pj_drone_core_test PRIVATE pj_drone::core GTest::gtest_main)
  add_test(NAME pj_drone_core_test COMMAND pj_drone_core_test)
endif()
