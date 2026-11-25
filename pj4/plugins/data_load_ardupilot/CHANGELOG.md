# Changelog

## [Unreleased]

### Added
- When the log has a valid GPS time (first `GPS` row with a 3D fix, a non-zero `GWk` and a `GMS` inside the week), all
  timestamps, including the markers, are UTC seconds since the Unix epoch instead of seconds since boot. GPS time is
  converted with the leap seconds in force on the log's date (IERS list built in); a log without a valid GPS row keeps the boot clock.
- `MSG`, `MODE` and `ARM` are also published as plot markers, on a new topic `_markers`. Each field of it is a series
  of NaN (it draws nothing and stays out of the axis range) with the marker set addressed to it; drag a field onto a
  plot to see its markers there (the curve's "show markers" flag hides them again):
  - `_markers/message` (from `MSG`): a Label per message (`category=log_message`); when the log has a `Severity` field the message is
    an Event coloured by that level instead.
  - `_markers/mode` (from `MODE`): a Region per mode (`category=flight_mode`, label = mode name, `metadata` has the mode number and
    `Rsn`), ending at the next mode change or at the end of the log. Each mode number has its own colour (a palette of
    8 without red, which is the arming colour; modes 8 apart share one).
  - `_markers/arm event` (from `ARM`): an Event per arm/disarm (`category=arm_event`; a forced change is a warning). Repeated rows
    of the same state are ignored.
  - `_markers/arm` (from `ARM`): a Region per armed span (`category=arm_region`, label `ARMED`, error colour).
- `MODE` has a new string series `ModeName` with the flight mode as text (e.g. `LOITER`). The vehicle type comes from
  `VER.BU`, then `VER.FWS`, then the firmware banner in `MSG`. A mode that has no name, an unknown vehicle, or a
  `VER.BU` that disagrees with `VER.FWS` gives `Unknown(<number>)`. The built-in names are generated from the MAVLink
  `ardupilotmega.xml` at build time.
- The dialog has a "Mode names" file (JSON) with complete mode enums, for custom firmware and for logs whose vehicle
  sources disagree. A log it matches is named by that enum only; the built-in names are never mixed in. The path is saved with the plugin config; an unusable file is reported in the
  dialog and the built-in names are used. Format: `docs/ardupilot/mode_names.md`.
- The "Mode names" file can also be a MAVLink `ardupilotmega.xml` of any version (e.g. from `c_library_v2`): its
  `COPTER_MODE`, `PLANE_MODE`, `ROVER_MODE`, `SUB_MODE` and `TRACKER_MODE` enums replace the built-in names of those
  vehicles, with no rebuild.
- `MODE` rows are written after the rest of the log has been read, so one log is always named by one vehicle type and
  one set of rules, also when `MODE` comes before `VER`. With several `VER` records the first one decides.

## [1.1.0] - 2026-11-15

### Changed
- Embedded files are no longer assembled in memory while the log is scanned; the content is built only when you
  export, streamed straight to disk.
- The size shown for an embedded file is the size actually assembled from its FILE chunks. The size implied by the
  FILE offsets is only used to warn when the two differ (also for overlapping chunks; where chunks overlap, the later
  one in the log wins). The list also shows how many of those bytes are really present in the log ("Data").
- A file with chunks missing in the middle is flagged and not exported, instead of being padded with zeros.
- Export maps the log again and verifies a SHA-256 over the file's FILE records taken at scan time, without parsing
  the log a second time; a log that changed or was truncated in between is refused.

### Security
- Embedded file names are validated before export (absolute / drive-relative paths, `..`, reserved or invalid
  characters, Windows device names, symbolic links leaving the chosen folder); unsafe names are reported, not written.
- Embedded file names are shown and exported as printable ASCII. Every other byte (and `%` itself) is percent-encoded,
  so `a\xFF.lua` becomes `a%FF.lua` and names stay distinct.
- Several embedded files that would resolve to the same export path are all reported instead of overwriting each other.

### Added
- When the export folder already contains a file with the same name, the dialog asks whether to overwrite or skip
  the existing files.

### Fixed
- A corrupt record (unknown message id or impossible length) no longer ends the import: the parser resynchronizes on
  the next header and keeps the rest of the log.
- Message id 0 is no longer mistaken for an undefined UNIT / MULT / FMTU / FILE message.
- A topic or field the host rejects, or a FMT label repeated with the same type, now only drops that group or column
  instead of failing the whole import.

## [1.0.0] - 2026-11-08

### Added
- macOS arm64 (Apple silicon) release build, alongside Linux x86_64 / arm64 and Windows x86_64.

### Fixed
- With "Append units to series names", a `.` in a unit (`W.h`, `W.s`, `A.s`) is dropped (`Wh`, `Ws`, `As`) so it no
  longer splits the series path into extra tree levels.

## [0.1.1] - 2026-10-04

### Added
- "Append units to series names" option in the loader dialog (e.g. `Roll` -> `Roll(deg)`); `/` in a unit is shown as
  U+2215 (`∕`) so `m/s` does not split the field path. The setting is saved in the plugin config (`show_units`).

### Fixed
- Exporting parameters now appends `.param` when the chosen file name has no such extension.

## [0.1.0] - 2026-10-03

### Added
- First PJ4 port of the PJ3 `DataLoadArdupilot` plugin: loads ArduPilot DataFlash `.BIN` logs.
- Each message type (and each instance of a multi-instance message, e.g. `GPS/0`) becomes a topic; numeric fields
  keep their native types, or `double` when the log scales them (MULT / FMTU, `c` `C` `e` `E` `L`).
- Timestamps are stored as exact nanoseconds (`TimeUS * 1000`).
- Pre-import dialog with Parameters (regex search, `.param` export), Messages and Embedded Files (export) tabs.

### Changed (vs. PJ3)
- The three load-settings checkboxes were removed; the PJ3 defaults apply (no units in names, `GPS/0` naming).
- The log info dialog is shown before the import (as in the ULog plugin) instead of after it.
- Cancelling the import keeps the data decoded so far.
