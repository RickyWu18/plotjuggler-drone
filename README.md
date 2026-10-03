# PlotJuggler-Drone Plugins

> A self-maintained collection of [PlotJuggler](https://github.com/facontidavide/PlotJuggler) plugins for drone telemetry visualization and live data streaming.

[![License: MPL 2.0](https://img.shields.io/badge/License-MPL_2.0-brightgreen.svg)](https://opensource.org/licenses/MPL-2.0)

---

## Overview

PlotJuggler is a fast, open-source time-series visualization tool. This repository extends it with drone-specific plugins — primarily targeting MAVLink-based systems such as PX4 and ArduPilot — so that engineers and researchers can stream, inspect, and analyze flight telemetry directly inside PlotJuggler without any extra middleware.

<!-- TODO: Add a screenshot or GIF of the plugin streaming data inside PlotJuggler -->

---

## Available Plugins

| Plugin | Description | PJ3 support | PJ4 support |
|---|---|:---:|:---:|
| **DataStreamMavlink** | Streams live MAVLink telemetry over UDP, TCP, or Serial into PlotJuggler. Automatically discovers all message fields and populates them as time-series. Includes a built-in **Message Interval** dialog to inspect and tune per-message update rates on the vehicle. | ✅ | 🚧 |
| **DataLoadArdupilot** | Loads ArduPilot `.BIN` flight logs directly into PlotJuggler. All numeric fields are automatically available as plot series, with correct units applied. A post-load dialog shows flight **Parameters**, **Embedded Files**, and **Messages** from the log. | ✅ | 🚧 |

✅ supported · 🚧 planned / in progress

### DataStreamMavlink — Feature Highlights

- **Three transports:** UDP (default port 14550), TCP client, and Serial port — switchable from the connection dialog.
- **Zero-config field discovery:** every numeric field in every MAVLink message is automatically mapped to a plot series named `mav/<sysid>.<compid>/<MSG_NAME>/<field>`.
- **Multi-vehicle:** differentiates streams by `sysid.compid`, so data from multiple vehicles on the same link is kept separate.
- **Message Interval control:** via the **"Message Intervals…"** toolbar action, view live message rates and send `SET_MESSAGE_INTERVAL` commands back to the vehicle to tune what gets streamed and how fast.

### DataLoadArdupilot — Feature Highlights

- **No conversion needed:** open `.BIN` files directly — no pre-processing or format conversion required.
- **All fields, correct units:** every numeric field across all message types is automatically available as a plot series, with units and scaling applied. Optionally append the unit to each series name (e.g. `Roll(deg)`) via a checkbox before loading.
- **Multi-instance sensor support:** sensors with multiple instances (e.g. two GPS units) are kept separate and clearly labelled.
- **Official naming compat:** an optional checkbox matches the series naming convention used by the official ArduPilot PlotJuggler plugin, so saved layouts and scripts work without changes.
- **Post-load info dialog** with three tabs:
  - **Parameters** — all flight parameters from the log, with live search filtering and one-click export to a `.param` file.
  - **Embedded Files** — any files embedded in the log (e.g. crash dumps, config backups), exportable to a folder.
  - **Messages** — all flight messages with timestamps.

---

## How to Use

### PJ3 (PlotJuggler 3.x)

1. **Install PlotJuggler 3.x** from the [official GitHub releases page](https://github.com/facontidavide/PlotJuggler/releases).
2. **Download the plugin** for your platform from the [Releases](https://github.com/RickyWu18/plotjuggler-drone/releases) page of this repository.
3. **Drop it into the PlotJuggler plugin folder.** Add a custom folder in PlotJuggler Preferences, or place the file where PlotJuggler looks for plugins:

   | Platform | Plugin path |
   |---|---|
   | Windows | Same folder as `plotjuggler.exe` (e.g. `C:\Program Files\PlotJuggler\`) |
   | Linux | Same folder as the `plotjuggler` binary (e.g. `/usr/local/bin/`) |

4. Restart PlotJuggler. The plugin will load in the new session.

### PJ4 (PlotJuggler 4.x)

> 🚧 No released plugins yet.

---

## Building from Source

### PJ3 (PlotJuggler 3.x)

See [pj3/docs/build.md](pj3/docs/build.md) for workspace layout, PlotJuggler setup, and CMake build instructions.

### PJ4 (PlotJuggler 4.x)

See [pj4/docs/build.md](pj4/docs/build.md) for prerequisites, the `plotjuggler_sdk` setup, and build/package instructions. To add a plugin, see [pj4/docs/develop.md](pj4/docs/develop.md).

---

## License

This project is licensed under the [Mozilla Public License 2.0](https://opensource.org/licenses/MPL-2.0).
