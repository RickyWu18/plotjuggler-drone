# PlotJuggler-Drone Plugins

> An unofficial collection of [PlotJuggler](https://github.com/facontidavide/PlotJuggler) plugins for drone telemetry visualization and live data streaming.

[![License: MIT](https://img.shields.io/badge/License-MIT-brightgreen.svg)](LICENSE)

---

## Overview

PlotJuggler is a fast, open-source time-series visualization tool. This repository extends it with drone-specific plugins — primarily targeting MAVLink-based systems such as PX4 and ArduPilot — so that engineers and researchers can stream, inspect, and analyze flight telemetry directly inside PlotJuggler without any extra middleware.

---

## Available Plugins

| Plugin | Description | PJ3 | PJ4 |
|---|---|:---:|:---:|
| **DataStreamMavlink** | Streams live MAVLink telemetry over UDP, TCP, or Serial. ([PJ3](pj3/DataStreamMavlink/README.md) / [PJ4](pj4/plugins/data_stream_mavlink/README.md)) | [v0.2.0](https://github.com/RickyWu18/plotjuggler-drone/releases/tag/pj3-v0.2.0) | [v0.1.0](https://github.com/RickyWu18/plotjuggler-drone/releases/tag/data_stream_mavlink/v0.1.0) |
| **DataLoadArdupilot** | Loads ArduPilot `.BIN` flight logs with correct units. ([PJ3](pj3/DataLoadArdupilot/README.md) / [PJ4](pj4/plugins/data_load_ardupilot/README.md)) | [v0.2.0](https://github.com/RickyWu18/plotjuggler-drone/releases/tag/pj3-v0.2.0) | [v1.2.0](https://github.com/RickyWu18/plotjuggler-drone/releases/tag/data_load_ardupilot/v1.2.0) |

See each plugin's README for its feature list.

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

1. **Install PlotJuggler 4.x.**
2. **Open PlotJuggler 4.x Marketplace** at `File > Extensions Marketplace`.
3. **Find and Install the plugin**
   - **DataLoadArdupilot**: Ardupilot BIN Loader
   - **DataStreamMavlink**: MAVLink streamer — *coming soon, use Option B for now.*
4. **Restart PlotJuggler.** The plugin will load in the new session.

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
