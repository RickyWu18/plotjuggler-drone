# Changelog

## [0.1.0] - 2026-10-03

### Added
- First PJ4 port of the PJ3 `DataStreamMavlink` plugin: streams live MAVLink telemetry over UDP (default port 14550),
  TCP client (default `localhost:5760`) or a serial port (8N1, standard baud rates, default 57600), chosen in the
  connection dialog. Settings are remembered through the plugin config.
- Every message becomes a topic `mav/<sysid>.<compid>/<MSG_NAME>` (multiple vehicles stay separate); every numeric
  field is a pre-registered column, array fields are `<field>.<index>`, char fields are skipped.
- Timestamps follow PJ3: the payload's `time_usec` / `time_boot_ms` when present and non-zero, otherwise the wall-clock
  receive time. Stored as exact nanoseconds.

### Changed (vs. PJ3)
- Values keep their native MAVLink types (e.g. `uint64`) instead of being converted to `double`.
- A UDP bind failure is now reported as a start error (PJ3 failed silently). TCP and serial failures still show a
  warning message.
- Receive time is taken per datagram / read instead of once per read batch.

### Removed (vs. PJ3)
- The "Message Intervals..." window (message rates, `SET_MESSAGE_INTERVAL` commands). The PJ4 plugin only receives.
