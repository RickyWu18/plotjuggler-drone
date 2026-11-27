# DataLoadArdupilot (PJ3)

Loads ArduPilot `.BIN` flight logs directly into PlotJuggler 3.x.

## Features

- **No conversion needed:** open `.BIN` files directly — no pre-processing or format conversion required.
- **All fields, correct units:** every numeric field across all message types is automatically available as a plot series, with units and scaling applied. Optionally append the unit to each series name (e.g. `Roll(deg)`) via a checkbox before loading.
- **Multi-instance sensor support:** sensors with multiple instances (e.g. two GPS units) are kept separate and clearly labelled.
- **UTC time axis:** when the log contains a valid GPS time, all timestamps are UTC seconds since the Unix epoch;
  otherwise the boot clock (seconds since boot) is used.
- **Flight mode names:** `MODE` gets a `ModeName` text series (e.g. `LOITER`), with names resolved from the vehicle type
  found in the log. The names come from built-in tables generated from the MAVLink `ardupilotmega.xml` in a submodule,
  so they are fixed to the version the plugin was built with.
- **Info dialog** with three tabs:
  - **Parameters** — all flight parameters from the log, with live search filtering and one-click export to a `.param` file.
  - **Embedded Files** — any files embedded in the log (e.g. crash dumps, config backups), exportable to a folder.
  - **Messages** — all flight messages with timestamps.
- **Official naming compat:** an optional checkbox matches the series naming convention used by the [official ArduPilot PlotJuggler plugin](https://github.com/ArduPilot/plotjuggler-apbin-plugins), so saved layouts and scripts work without changes.
