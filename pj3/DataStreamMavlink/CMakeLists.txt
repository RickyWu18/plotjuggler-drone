find_package(Qt5 COMPONENTS SerialPort Network QUIET)

if(Qt5SerialPort_FOUND AND Qt5Network_FOUND)
  message(STATUS "[DataStreamMavlink] Qt5::SerialPort and Qt5::Network found")

  set(MAVLINK_INCLUDE_DIR "${DRONE_ROOT_DIR}/3rdparty/c_library_v2")

  set(SRC
    mavlink/mavlink_constants.h
    mavlink/mavlink_endpoint.h
    mavlink/mavlink_parser.h        mavlink/mavlink_parser.cpp
    mavlink/mavlink_controller.h    mavlink/mavlink_controller.cpp
    transport/mavlink_transport.h
    transport/udp_transport.h       transport/udp_transport.cpp
    transport/serial_transport.h    transport/serial_transport.cpp
    transport/tcp_transport.h       transport/tcp_transport.cpp
    datastream_mavlink.h    datastream_mavlink.cpp    datastream_mavlink.ui
    interval_settings_dialog.h interval_settings_dialog.cpp interval_settings_dialog.ui)

  add_library(DataStreamMavlink SHARED ${SRC})

  target_include_directories(DataStreamMavlink PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/mavlink
    ${CMAKE_CURRENT_SOURCE_DIR}/transport
    ${PJ_INCLUDE_DIR}
    ${MAVLINK_INCLUDE_DIR})

  target_compile_definitions(DataStreamMavlink PRIVATE
    QT_PLUGIN
    FMT_HEADER_ONLY
    PJ_MAJOR_VERSION=${PJ_MAJOR_VERSION}
    PJ_MINOR_VERSION=${PJ_MINOR_VERSION}
    PJ_PATCH_VERSION=${PJ_PATCH_VERSION})

  target_link_libraries(DataStreamMavlink PRIVATE
    Qt5::Widgets Qt5::Xml Qt5::Network Qt5::SerialPort
    ${PJ_BASE_LIB})

  install(TARGETS DataStreamMavlink
    RUNTIME DESTINATION ${PJ_PLUGIN_INSTALL_DIRECTORY}/$<CONFIG>)
else()
  message(WARNING "[DataStreamMavlink] Qt5::SerialPort or Qt5::Network not found. Skipping.")
endif()
