// SPDX-License-Identifier: MIT

#pragma once

#include <pj_plugins/sdk/dialog_plugin_typed.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "pj_drone_core/ardupilot/log_tools.hpp"
#include "pj_drone_core/ardupilot/parser.hpp"
#include "pj_drone_core/ardupilot/vehicle.hpp"

namespace ardupilot_detail {

/// Pre-import dialog: shows the log's parameters, messages and embedded files (scanned without decoding the
/// numeric data) and lets the user export them. OK imports the log, Cancel aborts.
class ArdupilotDialog : public PJ::DialogPluginTyped {
 public:
  void setFilePath(const std::string& filepath);

  /// Append the field's unit to its name, e.g. "Roll(deg)".
  bool showUnits() const {
    return show_units_;
  }

  /// The user's mode-name entries (empty when no file is chosen or it could not be read), looked at before the
  /// built-in tables.
  const pj_drone::ardupilot::ModeOverrides& modeOverrides() const {
    return mode_overrides_;
  }
  const std::string& modeConfigPath() const {
    return mode_config_path_;
  }

  // --- Dialog protocol ---
  std::string manifest() const override;
  std::string ui_content() const override;
  std::string widget_data() override;
  std::string saveConfig() const override;
  bool loadConfig(std::string_view config_json) override;
  void onAccepted(std::string_view /*json*/) override {}
  void onRejected() override {}

  bool onToggled(std::string_view name, bool checked) override;
  bool onTextChanged(std::string_view name, std::string_view text) override;
  bool onSelectionChanged(std::string_view name, const std::vector<std::string>& selected) override;
  bool onFileSelected(std::string_view name, std::string_view path) override;
  bool onFolderSelected(std::string_view name, std::string_view path) override;
  bool onClicked(std::string_view name) override;

 private:
  void runPendingExport(pj_drone::ardupilot::OverwritePolicy policy);
  void clearPendingExport();
  void scanFile();
  void refilterParameters();
  void setModeConfig(const std::string& path);
  std::string modeConfigText() const;

  std::string filepath_;
  std::string scanned_path_;
  std::string status_;
  bool show_units_ = false;

  std::string mode_config_path_;
  std::string mode_config_error_;  // why mode_config_path_ was not used; empty when it was
  pj_drone::ardupilot::ModeOverrides mode_overrides_;

  std::vector<pj_drone::ardupilot::Parameter> params_;
  std::vector<pj_drone::ardupilot::EmbeddedFile> files_;
  std::vector<pj_drone::ardupilot::LogMessage> messages_;

  std::string filter_text_;
  std::vector<pj_drone::ardupilot::Parameter> visible_params_;  // filtered + sorted by name
  std::vector<std::string> selected_files_;

  // An export waiting for the user to decide what to do about files that already exist in the target folder.
  std::vector<std::string> pending_names_;
  std::vector<std::string> pending_conflicts_;
  std::string pending_dir_;
};

}  // namespace ardupilot_detail
