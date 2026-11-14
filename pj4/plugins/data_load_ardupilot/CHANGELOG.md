# Changelog

## [Unreleased]

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
