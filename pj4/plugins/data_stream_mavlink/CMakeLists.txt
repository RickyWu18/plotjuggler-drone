find_package(asio REQUIRED)

add_library(data_stream_mavlink SHARED mavlink_source.cpp)
target_compile_options(data_stream_mavlink PRIVATE ${PJ_WARNING_FLAGS})
target_link_libraries(data_stream_mavlink PRIVATE
  plotjuggler_sdk::plugin_sdk
  pj_drone::core
  nlohmann_json::nlohmann_json
  asio::asio)
pj_configure_plugin(data_stream_mavlink
  FAMILIES        data_source dialog
  MANIFEST_FILE   ${CMAKE_CURRENT_SOURCE_DIR}/manifest.json
  MANIFEST_HEADER generated/mavlink_manifest.hpp
  MANIFEST_VAR    kMavlinkManifest
)
pj_embed_file(data_stream_mavlink
  FILE     ${CMAKE_CURRENT_SOURCE_DIR}/mavlink_connect.ui
  HEADER   generated/mavlink_connect_ui.hpp
  VAR_NAME kMavlinkConnectUi
)
