# DataLoadArdupilot (PJ4)

Loads ArduPilot `.BIN` flight logs directly into PlotJuggler 4.x.

## Features

- **No conversion needed:** open `.BIN` files directly — no pre-processing or format conversion required.
- **All fields, correct units:** every numeric field across all message types is automatically available as a plot series, with units and scaling applied. Optionally append the unit to each series name (e.g. `Roll(deg)`) via a checkbox before loading.
- **Multi-instance sensor support:** sensors with multiple instances (e.g. two GPS units) are kept separate and clearly labelled.
- **Info dialog** with three tabs:
  - **Parameters** — all flight parameters from the log, with live search filtering and one-click export to a `.param` file.
  - **Embedded Files** — any files embedded in the log (e.g. crash dumps, config backups), exportable to a folder.
  - **Messages** — all flight messages with timestamps.

> The official-naming compatibility option is only available in the PJ3 version.
