# Changelog

## [0.1.0] - Unreleased

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
