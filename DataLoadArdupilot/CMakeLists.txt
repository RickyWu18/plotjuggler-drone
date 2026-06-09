cmake_minimum_required(VERSION 3.16)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# ── Git hashes for build-info header ──────────────────────────────────────────
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE PLUGIN_GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NOT PLUGIN_GIT_HASH)
    set(PLUGIN_GIT_HASH "unknown")
endif()

set(_MAVLINK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/c_library_v2")
if(EXISTS "${_MAVLINK_DIR}")
    execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY ${_MAVLINK_DIR}
        OUTPUT_VARIABLE MAVLINK_GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
endif()
if(NOT MAVLINK_GIT_HASH)
    set(MAVLINK_GIT_HASH "unknown")
endif()

execute_process(
    COMMAND git describe --tags --abbrev=0
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE _PLUGIN_GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(_PLUGIN_GIT_TAG)
    string(REGEX REPLACE "^v" "" PLUGIN_OWN_VERSION "${_PLUGIN_GIT_TAG}")
else()
    set(PLUGIN_OWN_VERSION "0.0.0")
endif()

# Dev if HEAD is not exactly on a tag, or if the working tree has changes.
execute_process(
    COMMAND git describe --tags --exact-match HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE _ON_TAG_RESULT
    OUTPUT_QUIET
    ERROR_QUIET)
execute_process(
    COMMAND git status --porcelain
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE _GIT_STATUS
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(_ON_TAG_RESULT EQUAL 0 AND NOT _GIT_STATUS)
    set(PLUGIN_IS_DEV FALSE)
else()
    set(PLUGIN_IS_DEV TRUE)
endif()

configure_file(ardupilot_build_info.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/ardupilot_build_info.h @ONLY)

# ── Generate mode-name lookup table from ardupilotmega.xml ────────────────────
set(_MAVLINK_XML "${_MAVLINK_DIR}/message_definitions/ardupilotmega.xml")
set(_GEN_MODE_TABLES_H "${CMAKE_CURRENT_BINARY_DIR}/ardupilot_mode_tables_gen.h")
add_custom_command(
    OUTPUT  ${_GEN_MODE_TABLES_H}
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/gen_mode_tables.py
            ${_MAVLINK_XML}
            ${_GEN_MODE_TABLES_H}
    DEPENDS ${_MAVLINK_XML}
            ${CMAKE_CURRENT_SOURCE_DIR}/gen_mode_tables.py
    COMMENT "Generating ardupilot mode tables from ardupilotmega.xml")
add_custom_target(ardupilot_gen_mode_tables DEPENDS ${_GEN_MODE_TABLES_H})

set(SRC
    ardupilot_parser.h          ardupilot_parser.cpp
    ardupilot_mode_tables.h
    dataload_ardupilot.h        dataload_ardupilot.cpp
    ardupilot_info_dialog.h     ardupilot_info_dialog.cpp
    ardupilot_info_dialog.ui)

add_library(DataLoadArdupilot SHARED ${SRC})
add_dependencies(DataLoadArdupilot ardupilot_gen_mode_tables)

target_include_directories(DataLoadArdupilot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${PJ_INCLUDE_DIR})

target_compile_definitions(DataLoadArdupilot PRIVATE
    QT_PLUGIN
    FMT_HEADER_ONLY
    PJ_MAJOR_VERSION=${PJ_MAJOR_VERSION}
    PJ_MINOR_VERSION=${PJ_MINOR_VERSION}
    PJ_PATCH_VERSION=${PJ_PATCH_VERSION}
    # Show the diagnostic "Debug" tab only in RelWithDebInfo builds. This config
    # uses the Release CRT, so it links against the prebuilt (Release) PJ base lib.
    $<$<CONFIG:RelWithDebInfo>:ARDUPILOT_DEBUG_TAB>
    $<$<BOOL:${PLUGIN_IS_DEV}>:ARDUPILOT_DEV_BUILD>)

target_link_libraries(DataLoadArdupilot PRIVATE
    Qt5::Core
    Qt5::Widgets
    Qt5::Xml
    ${PJ_BASE_LIB})

install(TARGETS DataLoadArdupilot
    RUNTIME DESTINATION ${PJ_PLUGIN_INSTALL_DIRECTORY}/$<CONFIG>)
