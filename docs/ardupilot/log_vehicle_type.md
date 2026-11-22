# Vehicle type information in a log

Scope: vehicle type only (Copter, Plane, Tracker, etc.). Airframe (frame class/type) is out of scope.

## 1. Three sources

| Source | Content | Written at | Decided at |
|---|---|---|---|
| `VER.BU` | Vehicle type number | `AP_Logger_Backend.cpp:621` (`fwver.vehicle_type`) | Compile time (waf sets `APM_BUILD_DIRECTORY`) |
| `VER.FWS` | Firmware string, e.g. `ArduCopter V4.8.0-dev (githash)` | `AP_Logger_Backend.cpp:616` | Compile time (`THISFIRMWARE` in `version.h`; can be replaced by `AP_CUSTOM_FIRMWARE_STRING` from hwdef) |
| `MSG` (first message) | The same firmware string | `LoggerMessageWriter.cpp:283-289` | Same as above |

Note: `FWS` and `MSG` come from the same `fw_string`. They are two copies of one piece of information and cannot verify each other. Only `BU` comes from a different field.

## 2. `BU` value table

Defined in `libraries/AP_Vehicle/AP_Vehicle_Type.h:23-35`.

| BU | Vehicle |
|---|---|
| 1 | Rover |
| 2 | ArduCopter |
| 3 | ArduPlane |
| 4 | AntennaTracker |
| 5 | UNKNOWN |
| 7 | ArduSub |
| 9 | AP_Periph |
| 12 | Blimp |
| 13 | Heli |

Values 6, 8, 10 and 11 were not checked.

## 3. `FWS` prefix per vehicle

| Vehicle | `THISFIRMWARE` |
|---|---|
| Copter | `ArduCopter V...` |
| Plane | `ArduPlane V...` |
| Tracker | `AntennaTracker V...` |
| Sub | `ArduSub V...` |
| Blimp | `Blimp V...` |
| Rover | `ArduRover V...` (not `Rover`) |

The prefix for Heli (`BU` = 13) and AP_Periph (`BU` = 9) was not checked.

## 4. Available sources by firmware version

`VER` and `BU` were added later.

| Field | Commit | Date | First release |
|---|---|---|---|
| `VER` message | `85b4ee289e` | 2022-02-02 | 4.2.0 |
| `VER.BU` | `69371e84fd` | 2023-10-12 | 4.5.0 |

| Firmware version | Available sources |
|---|---|
| 4.5.0 and later | `BU`, `FWS`, `MSG` |
| 4.2.0 to 4.4.x | `FWS`, `MSG` (no `BU`) |
| Before 4.2.0 | `MSG` only (no `VER`; the version in which the `MSG` firmware string was added was not verified) |

## 5. Reliability

- `BU` is the most reliable: it is a number and is not affected by renaming.
- `FWS` is next: a vendor can rename it with `AP_CUSTOM_FIRMWARE_STRING`.
- For custom firmware, `MSG` is written as `custom string [original string]`, so the original name is visible. In that case `MSG` carries more information than `FWS`.
- All three are self-reported by the firmware. There is no signature or hash protecting them. A log file can be edited directly, and self-built firmware can set any value, so they are not proof of firmware identity.
- The `FWS` field is limited to 64 characters. Longer strings are truncated.

## 6. Suggested procedure

1. Read `VER.BU`. If present, it is the vehicle type.
2. If `VER.FWS` is also present, check that its prefix matches `BU` (see the table in section 3).
   - Match: accept the result.
   - Mismatch: treat the log as suspect. Possible causes are a custom firmware name, an edited log, or a modified build. Report both values and do not pick one silently.
3. If there is no `BU` but `VER` exists, use the `FWS` prefix.
4. If there is no `VER`, use the first `MSG`.
5. If the string contains `[...]`, the bracketed text is the original firmware name and the firmware is custom. Use the original name for the vehicle type and note that it is custom.

Comparing `FWS` with `MSG` adds nothing, because they have the same source. It only detects a log edited in one place but not the other.
