// SPDX-License-Identifier: MIT

#pragma once

#include <algorithm>
#include <nlohmann/json.hpp>
#include <pj_plugins/sdk/dialog_plugin_typed.hpp>
#include <pj_plugins/sdk/widget_data.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "mavlink_connect_ui.hpp"  // generated
#include "mavlink_manifest.hpp"    // generated
#include "pj_drone_core/io/serial_ports.hpp"

namespace mavlink_detail {

enum class Mode : int { kUdp = 0, kSerial = 1, kTcp = 2 };  // same numbering PJ3 stored in QSettings

/// Connection settings, shared by the dialog and the source.
struct ConnectionConfig {
  Mode mode = Mode::kUdp;
  int udp_port = 14550;
  std::string tcp_host = "localhost";
  int tcp_port = 5760;
  std::string serial_port;  // short name as listed in the combo (e.g. "COM3")
  int baud = 57600;

  static ConnectionConfig fromJson(std::string_view json) {
    ConnectionConfig c;
    auto cfg = nlohmann::json::parse(json, nullptr, false);
    if (cfg.is_discarded() || !cfg.is_object()) {
      return c;
    }
    const int mode = cfg.value("transport", 0);
    c.mode = (mode == 1) ? Mode::kSerial : (mode == 2) ? Mode::kTcp : Mode::kUdp;
    c.udp_port = cfg.value("udp_port", c.udp_port);
    c.tcp_host = cfg.value("tcp_host", c.tcp_host);
    c.tcp_port = cfg.value("tcp_port", c.tcp_port);
    c.serial_port = cfg.value("serial_port", c.serial_port);
    c.baud = cfg.value("baud", c.baud);
    return c;
  }

  std::string toJson() const {
    return nlohmann::json{{"transport", static_cast<int>(mode)}, {"udp_port", udp_port}, {"tcp_host", tcp_host},
                          {"tcp_port", tcp_port},                {"serial_port", serial_port}, {"baud", baud}}
        .dump();
  }
};

/// Connection dialog: transport choice (UDP / TCP / Serial) and its settings.
class MavlinkDialog : public PJ::DialogPluginTyped {
 public:
  MavlinkDialog() {
    refreshPorts();
  }

  std::string manifest() const override {
    return kMavlinkManifest;
  }

  std::string ui_content() const override {
    return kMavlinkConnectUi;
  }

  std::string widget_data() override {
    PJ::WidgetData wd;

    wd.setChecked("radioUdp", cfg_.mode == Mode::kUdp);
    wd.setChecked("radioTcp", cfg_.mode == Mode::kTcp);
    wd.setChecked("radioSerial", cfg_.mode == Mode::kSerial);
    wd.setVisible("udpGroup", cfg_.mode == Mode::kUdp);
    wd.setVisible("tcpGroup", cfg_.mode == Mode::kTcp);
    wd.setVisible("serialGroup", cfg_.mode == Mode::kSerial);

    wd.setValue("portSpin", cfg_.udp_port);
    wd.setText("tcpHostEdit", cfg_.tcp_host);
    wd.setValue("tcpPortSpin", cfg_.tcp_port);

    std::vector<std::string> names;
    names.reserve(ports_.size());
    for (const auto& p : ports_) {
      names.push_back(p.name);
    }
    wd.setItems("portCombo", names);
    wd.setCurrentIndex("portCombo", portIndex());

    wd.setItems("baudCombo", baudItems());
    wd.setCurrentIndex("baudCombo", baudIndex());
    return wd.toJson();
  }

  bool onToggled(std::string_view name, bool checked) override {
    if (!checked) {
      return false;
    }
    if (name == "radioUdp") {
      cfg_.mode = Mode::kUdp;
    } else if (name == "radioTcp") {
      cfg_.mode = Mode::kTcp;
    } else if (name == "radioSerial") {
      cfg_.mode = Mode::kSerial;
    } else {
      return false;
    }
    return true;
  }

  bool onTextChanged(std::string_view name, std::string_view text) override {
    if (name == "tcpHostEdit") {
      cfg_.tcp_host = std::string(text);
    }
    return false;
  }

  bool onValueChanged(std::string_view name, int value) override {
    if (name == "portSpin") {
      cfg_.udp_port = value;
    } else if (name == "tcpPortSpin") {
      cfg_.tcp_port = value;
    }
    return false;
  }

  bool onIndexChanged(std::string_view name, int index) override {
    if (name == "portCombo") {
      if (index >= 0 && static_cast<size_t>(index) < ports_.size()) {
        cfg_.serial_port = ports_[static_cast<size_t>(index)].name;
      }
    } else if (name == "baudCombo") {
      const auto& bauds = standardBaudRates();
      if (index >= 0 && static_cast<size_t>(index) < bauds.size()) {
        cfg_.baud = bauds[static_cast<size_t>(index)];
      }
    }
    return false;
  }

  bool onClicked(std::string_view name) override {
    if (name == "refreshBtn") {
      refreshPorts();
      return true;
    }
    return false;
  }

  void onAccepted(std::string_view /*json*/) override {}
  void onRejected() override {}

  std::string saveConfig() const override {
    return cfg_.toJson();
  }

  bool loadConfig(std::string_view config_json) override {
    cfg_ = ConnectionConfig::fromJson(config_json);
    refreshPorts();
    return true;
  }

  /// System location (path to open) of a port listed in the combo, by its short name.
  std::string systemLocationOf(const std::string& name) const {
    for (const auto& p : ports_) {
      if (p.name == name) {
        return p.system_location;
      }
    }
    return name;
  }

 private:
  static const std::vector<int>& standardBaudRates() {
    static const std::vector<int> kBauds = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
    return kBauds;
  }

  static std::vector<std::string> baudItems() {
    std::vector<std::string> items;
    for (int b : standardBaudRates()) {
      items.push_back(std::to_string(b));
    }
    return items;
  }

  int baudIndex() const {
    const auto& bauds = standardBaudRates();
    const auto it = std::find(bauds.begin(), bauds.end(), cfg_.baud);
    return it == bauds.end() ? 0 : static_cast<int>(it - bauds.begin());
  }

  // Like QComboBox::setCurrentText on a non-editable combo: an unknown name leaves the first entry selected.
  int portIndex() const {
    for (size_t i = 0; i < ports_.size(); ++i) {
      if (ports_[i].name == cfg_.serial_port) {
        return static_cast<int>(i);
      }
    }
    return 0;
  }

  void refreshPorts() {
    ports_ = pj_drone::io::availableSerialPorts();
    // The combo's current entry is what gets saved, as in PJ3 (portCombo->currentText()).
    if (!ports_.empty()) {
      cfg_.serial_port = ports_[static_cast<size_t>(portIndex())].name;
    } else {
      cfg_.serial_port.clear();
    }
  }

  ConnectionConfig cfg_;
  std::vector<pj_drone::io::SerialPortInfo> ports_;
};

}  // namespace mavlink_detail
