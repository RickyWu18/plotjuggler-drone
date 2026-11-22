# pugixml is used by pj_drone::core (it reads the user's MAVLink XML mode files when it finds pugixml); required here
# so that this plugin is never built with that feature silently switched off.
find_package(pugixml REQUIRED)
add_library(data_load_ardupilot SHARED ardupilot_source.cpp ardupilot_dialog.cpp ardupilot_markers.cpp mode_overrides.cpp)
target_compile_options(data_load_ardupilot PRIVATE ${PJ_WARNING_FLAGS})
target_link_libraries(data_load_ardupilot PRIVATE
  plotjuggler_sdk::plugin_sdk
  pj_drone::core
  nlohmann_json::nlohmann_json)
pj_configure_plugin(data_load_ardupilot
  FAMILIES        data_source dialog
  MANIFEST_FILE   ${CMAKE_CURRENT_SOURCE_DIR}/manifest.json
  MANIFEST_HEADER generated/ardupilot_manifest.hpp
  MANIFEST_VAR    kArdupilotManifest
)
pj_embed_file(data_load_ardupilot
  FILE     ${CMAKE_CURRENT_SOURCE_DIR}/ardupilot_info.ui
  HEADER   generated/ardupilot_info_ui.hpp
  VAR_NAME kArdupilotInfoUi
)
