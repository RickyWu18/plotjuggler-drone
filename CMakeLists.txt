cmake_minimum_required(VERSION 3.16)
project(plotjuggler_drone CXX C)

cmake_policy(SET CMP0177 NEW)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# ── Qt ─────────────────────────────────────────────────────────────────────
find_package(Qt5 REQUIRED COMPONENTS Core Widgets Xml Network Svg)

# ── PlotJuggler (out-of-tree, bypass broken installed Config.cmake) ─────────
# Point PJ_INSTALL at the prefix where PlotJuggler was installed.
# Default assumes the canonical workspace layout:
#   <ws>/
#     src/PlotJuggler-Drone/   <- here
#     install/                 <- PJ_INSTALL
set(PJ_INSTALL "${CMAKE_CURRENT_SOURCE_DIR}/../../install"
    CACHE PATH "PlotJuggler install prefix (contains bin/, include/, lib/)")

# Source tree needed for non-installed internal headers (timeseries_qwt.h, QWT)
set(PJ_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../PlotJuggler"
    CACHE PATH "PlotJuggler source directory")

find_library(PJ_BASE_LIB NAMES plotjuggler_base REQUIRED
    HINTS "${PJ_INSTALL}/lib")
find_library(PJ_QWT_LIB  NAMES plotjuggler_qwt  REQUIRED
    HINTS "${PJ_INSTALL}/lib")

set(PJ_INCLUDE_DIR "${PJ_INSTALL}/include")

# Plugins are installed alongside plotjuggler.exe so they are auto-discovered.
set(PJ_PLUGIN_INSTALL_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../build/Plotjuggler/bin"
    CACHE PATH "Destination for plugin DLLs/SOs")

# Read PlotJuggler version from the installed ConfigVersion file
file(STRINGS "${PJ_INSTALL}/lib/cmake/plotjuggler/plotjugglerConfigVersion.cmake"
     _pj_ver_line REGEX "^set\\(PACKAGE_VERSION \"[0-9.]+\"\\)")
string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ "${_pj_ver_line}")
set(PJ_MAJOR_VERSION ${CMAKE_MATCH_1})
set(PJ_MINOR_VERSION ${CMAKE_MATCH_2})
set(PJ_PATCH_VERSION ${CMAKE_MATCH_3})

message(STATUS "[PJ-Drone] PJ_INSTALL            = ${PJ_INSTALL}")
message(STATUS "[PJ-Drone] PJ_VERSION            = ${PJ_MAJOR_VERSION}.${PJ_MINOR_VERSION}.${PJ_PATCH_VERSION}")
message(STATUS "[PJ-Drone] PJ_BASE_LIB           = ${PJ_BASE_LIB}")
message(STATUS "[PJ-Drone] PJ_QWT_LIB            = ${PJ_QWT_LIB}")
message(STATUS "[PJ-Drone] PJ_PLUGIN_INSTALL_DIR = ${PJ_PLUGIN_INSTALL_DIRECTORY}")

# ── Output directories ─────────────────────────────────────────────────────
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ── Plugins ────────────────────────────────────────────────────────────────
# add_subdirectory(PluginTemplate)
add_subdirectory(DataStreamMavlink)
add_subdirectory(DataLoadArdupilot)
add_subdirectory(ToolboxRealtimeFFT)
