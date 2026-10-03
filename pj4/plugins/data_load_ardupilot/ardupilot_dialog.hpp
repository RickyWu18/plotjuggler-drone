/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <pj_plugins/sdk/dialog_plugin_typed.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "pj_drone_core/ardupilot/parser.hpp"

namespace ardupilot_detail {

/// Pre-import dialog: shows the log's parameters, messages and embedded files (scanned without decoding the
/// numeric data) and lets the user export them. OK imports the log, Cancel aborts.
class ArdupilotDialog : public PJ::DialogPluginTyped {
 public:
  void setFilePath(const std::string& filepath);

  // --- Dialog protocol ---
  std::string manifest() const override;
  std::string ui_content() const override;
  std::string widget_data() override;
  std::string saveConfig() const override;
  bool loadConfig(std::string_view config_json) override;
  void onAccepted(std::string_view /*json*/) override {}
  void onRejected() override {}

  bool onTextChanged(std::string_view name, std::string_view text) override;
  bool onSelectionChanged(std::string_view name, const std::vector<std::string>& selected) override;
  bool onFileSelected(std::string_view name, std::string_view path) override;
  bool onFolderSelected(std::string_view name, std::string_view path) override;

 private:
  void scanFile();
  void refilterParameters();

  std::string filepath_;
  std::string scanned_path_;
  std::string status_;

  std::vector<pj_drone::ardupilot::Parameter> params_;
  std::vector<pj_drone::ardupilot::EmbeddedFile> files_;
  std::vector<pj_drone::ardupilot::LogMessage> messages_;

  std::string filter_text_;
  std::vector<pj_drone::ardupilot::Parameter> visible_params_;  // filtered + sorted by name
  std::vector<std::string> selected_files_;
};

}  // namespace ardupilot_detail
