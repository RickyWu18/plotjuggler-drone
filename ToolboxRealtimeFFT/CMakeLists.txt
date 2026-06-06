cmake_minimum_required(VERSION 3.16)

# Build KissFFT as an OBJECT library with Qt auto-tools disabled
add_library(kissfft_obj OBJECT
    3rdparty/KissFFT/kiss_fft.c
    3rdparty/KissFFT/kiss_fftr.c)

set_target_properties(kissfft_obj PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF)

target_compile_definitions(kissfft_obj PRIVATE kiss_fft_scalar=float)
target_include_directories(kissfft_obj PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/KissFFT)

add_library(ToolboxRealtimeFFT SHARED
    toolbox_realtime_fft.cpp
    toolbox_realtime_fft.h
    toolbox_realtime_fft.ui
    $<TARGET_OBJECTS:kissfft_obj>)

target_include_directories(ToolboxRealtimeFFT PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty
    ${PJ_INCLUDE_DIR}
    ${PJ_SOURCE_DIR}/plotjuggler_base/src
    ${PJ_SOURCE_DIR}/3rdparty/qwt/src)

target_compile_definitions(ToolboxRealtimeFFT PRIVATE
    QT_PLUGIN
    kiss_fft_scalar=float
    PJ_MAJOR_VERSION=${PJ_MAJOR_VERSION}
    PJ_MINOR_VERSION=${PJ_MINOR_VERSION}
    PJ_PATCH_VERSION=${PJ_PATCH_VERSION})

target_link_libraries(ToolboxRealtimeFFT PRIVATE
    Qt5::Widgets
    Qt5::Xml
    Qt5::Svg
    ${PJ_BASE_LIB}
    ${PJ_QWT_LIB})

install(TARGETS ToolboxRealtimeFFT
    RUNTIME DESTINATION ${PJ_PLUGIN_INSTALL_DIRECTORY}/$<CONFIG>)
