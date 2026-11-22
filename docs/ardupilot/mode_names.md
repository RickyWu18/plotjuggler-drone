# Flight mode names

The `MODE` message gets an extra string series, `ModeName`, next to the log's own numeric `Mode`.

## Where the names come from

A log is named by **one mode enum, taken as a whole**: either an entry of the user's file or the built-in table of the detected vehicle. The two are never mixed; a mode the chosen enum does not list is `Unknown(<number>)`, it is not looked up in the other one.

1. **The user's mode-names file**, when one is chosen in the dialog. The first entry whose `match` is found in the log's firmware string is the enum (an entry without `match` applies to every log). Each entry has to be a complete enum, because nothing is filled in from the built-in tables.
2. **Otherwise the built-in table** of the vehicle the log reports (see [log_vehicle_type.md](log_vehicle_type.md)). The tables are generated at build time from `3rdparty/c_library_v2/message_definitions/ardupilotmega.xml` (COPTER, PLANE, ROVER, SUB and TRACKER mode enums; the name is the entry's `<description>`) by `core/scripts/gen_mode_tables.py`. They have the same form as an entry of the user's file (one per vehicle, no `match`), so the two are handled by the same code. Heli uses the Copter table. Blimp and AP_Periph have no table.
3. `Unknown(<number>)` when the enum in use has no such mode, the vehicle is unknown, or `VER.BU` and `VER.FWS` name different vehicles and no entry of the user's file matches.

## When the names are decided

`MODE` rows are held back while the log is read and written when the whole log has been read (also when the import is stopped, so what was read stays). The vehicle and the matching entry are therefore decided once, from all of `VER` and `MSG`, and every `MODE` row of a log is named by the same rules even when `MODE` comes before `VER`. When a log holds more than one `VER`, the first one decides.

## File format

The file is either the JSON format below or a MAVLink `ardupilotmega.xml`; the format is picked from the content (text starting with `<` is XML).

### ardupilotmega.xml

Choose any version of the dialect file (for example the one in `c_library_v2/message_definitions/`, or a newer one) and no rebuild is needed. The `COPTER_MODE`, `PLANE_MODE`, `ROVER_MODE`, `SUB_MODE` and `TRACKER_MODE` enums are read; each becomes one entry that applies to logs of that vehicle (Heli uses `COPTER_MODE`). The name of a mode is the entry's `<description>`, else its enum name without the prefix, the same rule as the built-in tables.

Reading XML needs pugixml. PJ4 builds have it; a build without it (the PJ3 plugins) has the built-in tables and the JSON format, and a chosen XML file is rejected with "this build cannot read MAVLink XML". The run-time reader (`core/src/ardupilot/mode_xml.cpp`) and the build-time generator apply the same naming rule; `core/tests/mode_xml_test.cpp` checks that they agree on the dialect file in `3rdparty`.

An enum the file lacks is skipped, and that vehicle keeps the built-in names. A file with none of the five enums, a duplicate value or a value outside 0-255 is rejected as a whole. The built-in tables are used when the file is rejected. Unlike a JSON entry, an XML entry does not look at the firmware string; the vehicle decides.

### JSON

```json
{
  "firmwares": [
    {
      "match": "MyBuild",
      "modes": { "0": "STABILIZE", "2": "ALT HOLD", "5": "LOITER", "6": "RTL", "30": "FOO" }
    }
  ]
}
```

| Field | Required | Meaning |
|---|---|---|
| `firmwares` | yes | Array of entries, tried in order; the first one that matches the log is used. |
| `match` | no | Text looked for in the firmware string (`VER.FWS`, else the firmware banner in `MSG`); case-insensitive. Empty or missing matches every log. |
| `modes` | yes, not empty | The whole enum: mode number (0-255, as a string key) to name. |

A file with any error is rejected as a whole; the dialog shows the reason and the built-in tables are used. Unknown keys are ignored.
