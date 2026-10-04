# PlotJuggler-Drone Plugins

> An unofficial collection of [PlotJuggler](https://github.com/facontidavide/PlotJuggler) plugins for drone telemetry visualization and live data streaming.

[![License: MIT](https://img.shields.io/badge/License-MIT-brightgreen.svg)](LICENSE)

---

## Overview

PlotJuggler is a fast, open-source time-series visualization tool. This repository extends it with drone-specific plugins — primarily targeting MAVLink-based systems such as PX4 and ArduPilot — so that engineers and researchers can stream, inspect, and analyze flight telemetry directly inside PlotJuggler without any extra middleware.

---

## Available Plugins

| Plugin | Description | PJ3 support | PJ4 support |
|---|---|:---:|:---:|
| **DataStreamMavlink** | Streams live MAVLink telemetry over UDP, TCP, or Serial into PlotJuggler. Automatically discovers all message fields and populates them as time-series. | ✅ | ✅ |
| **DataLoadArdupilot** | Loads ArduPilot `.BIN` flight logs directly into PlotJuggler. All numeric fields are automatically available as plot series, with correct units applied. A dialog shows flight **Parameters**, **Embedded Files**, and **Messages** from the log. | ✅ | ✅ |

### DataStreamMavlink — Feature Highlights

- **Three transports:** UDP (default port 14550), TCP client, and Serial port — switchable from the connection dialog.
- **Zero-config field discovery:** every numeric field in every MAVLink message is automatically mapped to a plot series named `mav/<sysid>.<compid>/<MSG_NAME>/<field>`.
- **Multi-vehicle:** differentiates streams by `sysid.compid`, so data from multiple vehicles on the same link is kept separate.
- **Message Interval control (PJ3 only):** via the **"Message Intervals…"** toolbar action, view live message rates and send `SET_MESSAGE_INTERVAL` commands back to the vehicle to tune what gets streamed and how fast.

### DataLoadArdupilot — Feature Highlights

- **No conversion needed:** open `.BIN` files directly — no pre-processing or format conversion required.
- **All fields, correct units:** every numeric field across all message types is automatically available as a plot series, with units and scaling applied. Optionally append the unit to each series name (e.g. `Roll(deg)`) via a checkbox before loading.
- **Multi-instance sensor support:** sensors with multiple instances (e.g. two GPS units) are kept separate and clearly labelled.
- **Info dialog** with three tabs:
  - **Parameters** — all flight parameters from the log, with live search filtering and one-click export to a `.param` file.
  - **Embedded Files** — any files embedded in the log (e.g. crash dumps, config backups), exportable to a folder.
  - **Messages** — all flight messages with timestamps.
- **Official naming compat (PJ3 only):** an optional checkbox matches the series naming convention used by the [official ArduPilot PlotJuggler plugin](https://github.com/ArduPilot/plotjuggler-apbin-plugins), so saved layouts and scripts work without changes.

---

## How to Use

### PJ3 (PlotJuggler 3.x)

1. **Install PlotJuggler 3.x** from the [official GitHub releases page](https://github.com/facontidavide/PlotJuggler/releases).
2. **Download the plugin** for your platform from the [Releases](https://github.com/RickyWu18/plotjuggler-drone/releases) page of this repository.
3. **Drop it into the PlotJuggler plugin folder.**
   - **Method A:** Add a custom folder in PlotJuggler Preferences and drop the library file into the folder.
   - **Method B:** Place the file where PlotJuggler looks for plugins:

   | Platform | Plugin path |
   |---|---|
   | Windows | Same folder as `plotjuggler.exe` (e.g. `C:\Program Files\PlotJuggler\`) |
   | Linux | Same folder as the `plotjuggler` binary (e.g. `/usr/local/bin/`) |

4. Restart PlotJuggler. The plugins will load in the new session.

### PJ4 (PlotJuggler 4.x)

#### Option A - Marketplace

> **Coming soon:** Marketplace installation is being prepared.

<!--
1. **Install PlotJuggler 4.x.**
2. **Open PlotJuggler 4.x Marketplace** at `File > Extensions Marketplace`.
3. **Find and Install the plugins**
   - **DataLoadArdupilot**: Ardupilot BIN Loader
   - **DataStreamMavlink**: MAVLink streamer
4. **Restart PlotJuggler.** The plugins will load in the new session.
-->

#### Option B - Install Locally

1. **Download the plugin** from the [Releases](https://github.com/RickyWu18/plotjuggler-drone/releases) page. Each package contains the plugin library and its manifest.
2. **Click `Install local...`** in PlotJuggler 4.x Marketplace.
3. **Select the downloaded .zip file.**
4. **Restart PlotJuggler.** The plugins will load in the new session.

---

## Building from Source

### PJ3 (PlotJuggler 3.x)

See [pj3/docs/build.md](pj3/docs/build.md) for workspace layout, PlotJuggler setup, and CMake build instructions.

### PJ4 (PlotJuggler 4.x)

See [pj4/docs/build.md](pj4/docs/build.md) for prerequisites, the `plotjuggler_sdk` setup, and build/package instructions.

---

## Contributing

Bug reports and feature requests are welcome via [Issues](https://github.com/RickyWu18/plotjuggler-drone/issues).
For development setup, see [pj3/docs/develop.md](pj3/docs/develop.md) and [pj4/docs/develop.md](pj4/docs/develop.md).

---

## License

This project is licensed under the [MIT License](LICENSE).
