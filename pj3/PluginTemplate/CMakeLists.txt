add_library(DronePluginTemplate SHARED
    datastream_template.h
    datastream_template.cpp)

target_include_directories(DronePluginTemplate PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PJ_INCLUDE_DIR})

target_compile_definitions(DronePluginTemplate PRIVATE
    QT_PLUGIN
    FMT_HEADER_ONLY
    PJ_MAJOR_VERSION=${PJ_MAJOR_VERSION}
    PJ_MINOR_VERSION=${PJ_MINOR_VERSION}
    PJ_PATCH_VERSION=${PJ_PATCH_VERSION})

target_link_libraries(DronePluginTemplate PRIVATE
    Qt5::Core
    Qt5::Widgets
    Qt5::Xml
    Qt5::Network
    ${PJ_BASE_LIB}
    ${PJ_QWT_LIB})

install(TARGETS DronePluginTemplate
    RUNTIME DESTINATION ${PJ_PLUGIN_INSTALL_DIRECTORY}/$<CONFIG>)
