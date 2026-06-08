cmake_minimum_required(VERSION 3.16)

set(SRC
    ardupilot_parser.h          ardupilot_parser.cpp
    dataload_ardupilot.h        dataload_ardupilot.cpp
    ardupilot_info_dialog.h     ardupilot_info_dialog.cpp
    ardupilot_info_dialog.ui)

add_library(DataLoadArdupilot SHARED ${SRC})

target_include_directories(DataLoadArdupilot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PJ_INCLUDE_DIR})

target_compile_definitions(DataLoadArdupilot PRIVATE
    QT_PLUGIN
    FMT_HEADER_ONLY
    PJ_MAJOR_VERSION=${PJ_MAJOR_VERSION}
    PJ_MINOR_VERSION=${PJ_MINOR_VERSION}
    PJ_PATCH_VERSION=${PJ_PATCH_VERSION}
    # Show the diagnostic "Debug" tab only in RelWithDebInfo builds. This config
    # uses the Release CRT, so it links against the prebuilt (Release) PJ base lib.
    $<$<CONFIG:RelWithDebInfo>:ARDUPILOT_DEBUG_TAB>)

target_link_libraries(DataLoadArdupilot PRIVATE
    Qt5::Core
    Qt5::Widgets
    Qt5::Xml
    ${PJ_BASE_LIB})

install(TARGETS DataLoadArdupilot
    RUNTIME DESTINATION ${PJ_PLUGIN_INSTALL_DIRECTORY}/$<CONFIG>)
