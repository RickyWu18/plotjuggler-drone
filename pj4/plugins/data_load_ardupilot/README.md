# DataLoadArdupilot (PJ4)

Loads ArduPilot `.BIN` flight logs directly into PlotJuggler 4.x.

## Features

- **No conversion needed:** open `.BIN` files directly — no pre-processing or format conversion required.
- **All fields, correct units:** every numeric field across all message types is automatically available as a plot series, with units and scaling applied. Optionally append the unit to each series name (e.g. `Roll(deg)`) via a checkbox before loading.
- **Multi-instance sensor support:** sensors with multiple instances (e.g. two GPS units) are kept separate and clearly labelled.
- **UTC time axis:** when the log contains a valid GPS time, all timestamps are UTC seconds since the Unix epoch;
  otherwise the boot clock (seconds since boot) is used.
- **Plot markers:** log messages (`MSG`), flight modes (`MODE`) and arm/disarm state (`ARM`) are also published as
  markers on the `_markers` topic, which can be overlaid on any plot. See [docs/markers.md](docs/markers.md).
- **Flight mode names:** `MODE` gets a `ModeName` text series (e.g. `LOITER`), with names resolved from the vehicle type
  found in the log.
- **Info dialog** with four tabs:
  - **Parameters** — all flight parameters from the log, with live search filtering and export of the listed
    parameters to a `.param` file.
  - **Embedded Files** — any files embedded in the log (e.g. crash dumps, config backups), exportable to a folder.
    File names are validated before export, incomplete files are flagged and not exported, and you are asked
    before existing files are overwritten.
  - **Messages** — all flight messages with timestamps.
  - **Loader Settings** — the unit-in-name option and an optional custom mode names file (JSON or MAVLink
    `ardupilotmega.xml`) for custom firmware. See [docs/settings.md](docs/settings.md).

> The official-naming compatibility option is only available in the PJ3 version.
