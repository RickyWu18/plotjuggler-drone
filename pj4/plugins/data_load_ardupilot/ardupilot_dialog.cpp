// SPDX-License-Identifier: MIT

#include "ardupilot_dialog.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <pj_plugins/sdk/widget_data.hpp>

#include "ardupilot_info_ui.hpp"  // generated
#include "ardupilot_manifest.hpp"  // generated
#include "pj_drone_core/ardupilot/log_tools.hpp"
#include "pj_drone_core/io/mapped_file.hpp"

namespace ardupilot_detail {

namespace ap = pj_drone::ardupilot;

namespace {

std::filesystem::path pathFromUtf8(std::string_view s) {
  return std::filesystem::path(std::u8string(s.begin(), s.end()));
}

}  // namespace

void ArdupilotDialog::setFilePath(const std::string& filepath) {
  filepath_ = filepath;
  if (filepath_ != scanned_path_) {
    scanFile();
  }
}

std::string ArdupilotDialog::manifest() const {
  return kArdupilotManifest;
}

std::string ArdupilotDialog::ui_content() const {
  return kArdupilotInfoUi;
}

std::string ArdupilotDialog::saveConfig() const {
  return nlohmann::json{{"filepath", filepath_}, {"show_units", show_units_}}.dump();
}

bool ArdupilotDialog::loadConfig(std::string_view config_json) {
  auto cfg = nlohmann::json::parse(config_json, nullptr, false);
  if (cfg.is_discarded()) {
    return false;
  }
  show_units_ = cfg.value("show_units", false);
  setFilePath(cfg.value("filepath", std::string{}));
  return true;
}

std::string ArdupilotDialog::widget_data() {
  PJ::WidgetData wd;

  // Parameters tab: only the rows accepted by the search filter.
  std::vector<std::vector<std::string>> param_rows;
  param_rows.reserve(visible_params_.size());
  for (const auto& p : visible_params_) {
    param_rows.push_back({p.name, ap::formatParamValue(p.value)});
  }
  wd.setTableHeaders("tableParams", {"Parameter", "Value"});
  wd.setTableRows("tableParams", param_rows);
  wd.setSaveFilePicker("btnExport", "Export Params...", "Parameter files (*.param)", "Export Parameters");

  // Messages tab
  std::vector<std::vector<std::string>> msg_rows;
  msg_rows.reserve(messages_.size());
  for (const auto& m : messages_) {
    msg_rows.push_back({ap::formatMessageTimestamp(m.timestamp), m.message});
  }
  wd.setTableHeaders("tableMsgs", {"Timestamp (s)", "Message"});
  wd.setTableRows("tableMsgs", msg_rows);

  // Embedded Files tab
  std::vector<std::vector<std::string>> file_rows;
  file_rows.reserve(files_.size());
  for (const auto& f : files_) {
    file_rows.push_back({f.name, std::to_string(f.data.size()) + " bytes"});
  }
  wd.setTableHeaders("tableFiles", {"Filename", "Size"});
  wd.setTableRows("tableFiles", file_rows);
  wd.setFolderPicker("btnExportSelected", "Export Selected...", "Select Export Folder");
  wd.setFolderPicker("btnExportAll", "Export All...", "Select Export Folder");
  wd.setEnabled("btnExportSelected", !selected_files_.empty());
  wd.setEnabled("btnExportAll", !files_.empty());

  wd.setChecked("cbShowUnits", show_units_);

  wd.setLabel("labelStatus", status_);
  return wd.toJson();
}

bool ArdupilotDialog::onToggled(std::string_view name, bool checked) {
  if (name == "cbShowUnits") {
    show_units_ = checked;
    return true;
  }
  return false;
}

bool ArdupilotDialog::onTextChanged(std::string_view name, std::string_view text) {
  if (name != "searchEdit") {
    return false;
  }
  filter_text_ = std::string(text);
  refilterParameters();
  return true;
}

bool ArdupilotDialog::onSelectionChanged(std::string_view name, const std::vector<std::string>& selected) {
  if (name != "tableFiles") {
    return false;
  }
  selected_files_ = selected;
  return true;
}

bool ArdupilotDialog::onFileSelected(std::string_view name, std::string_view path) {
  if (name != "btnExport") {
    return false;
  }
  // The save picker does not append the filter's extension, so add ".param" when missing.
  std::string out_path(path);
  auto fs_path = pathFromUtf8(out_path);
  std::string ext = fs_path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
  if (ext != ".param") {
    out_path += ".param";
    fs_path = pathFromUtf8(out_path);
  }
  // Only the rows currently shown (search filter applied) are exported.
  if (ap::writeTextFile(fs_path, ap::buildParamFileText(visible_params_))) {
    status_ = "Exported " + std::to_string(visible_params_.size()) + " parameter(s) to:\n" + out_path;
  } else {
    status_ = "Export Failed: could not open file for writing:\n" + out_path;
  }
  return true;
}

bool ArdupilotDialog::onFolderSelected(std::string_view name, std::string_view path) {
  std::vector<const ap::EmbeddedFile*> to_export;
  if (name == "btnExportSelected") {
    for (const auto& file : files_) {
      if (std::find(selected_files_.begin(), selected_files_.end(), file.name) != selected_files_.end()) {
        to_export.push_back(&file);
      }
    }
    if (to_export.empty()) {
      return false;
    }
  } else if (name == "btnExportAll") {
    for (const auto& file : files_) {
      to_export.push_back(&file);
    }
  } else {
    return false;
  }

  const auto result = ap::exportEmbeddedFiles(to_export, pathFromUtf8(path));
  status_ = "Exported " + std::to_string(result.exported) + " file(s) to:\n" + std::string(path);
  if (!result.failed.empty()) {
    status_ += "\n\nFailed to write:";
    for (const auto& failed : result.failed) {
      status_ += "\n" + failed;
    }
  }
  return true;
}

void ArdupilotDialog::refilterParameters() {
  visible_params_ = ap::filterParameters(params_, ap::ParamFilter(filter_text_));
}

void ArdupilotDialog::scanFile() {
  scanned_path_ = filepath_;
  params_.clear();
  files_.clear();
  messages_.clear();
  selected_files_.clear();
  status_.clear();

  if (filepath_.empty()) {
    refilterParameters();
    return;
  }

  pj_drone::io::MappedFile file;
  std::string error;
  if (!file.open(pathFromUtf8(filepath_), &error)) {
    status_ = "ArduPilot: " + error + ": " + filepath_;
    refilterParameters();
    return;
  }

  // Metadata only: no sink, so numeric samples are not decoded.
  ap::Parser parser(file.data(), file.size());
  params_ = parser.parameters();
  files_ = parser.embeddedFiles();
  messages_ = parser.logMessages();
  refilterParameters();
}

}  // namespace ardupilot_detail
