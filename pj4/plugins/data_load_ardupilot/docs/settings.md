# Loader Settings

The **Loader Settings** tab of the pre-import dialog holds the options that change how the log is loaded. Both are
saved with the plugin config, so they come back when a layout is reloaded.

## Series names

**Append units to series names** (config key `show_units`, default off)

Adds the field's unit to its series name: `Roll` becomes `Roll(deg)`.

## Flight mode names

**Mode names file** (config key `mode_config`, default none)

The `MODE` message gets a `ModeName` text series (e.g. `LOITER`). By default the names come from tables built into the
plugin, picked by the vehicle type found in the log. Choose a file here to override them, for example for custom
firmware or for logs whose vehicle information is missing or contradictory.

The built-in tables are generated at build time from the MAVLink `ardupilotmega.xml` in a git submodule
(`3rdparty/c_library_v2`). They are therefore fixed to the submodule version the plugin was built with and do not
follow newer ArduPilot releases; modes added later show as `Unknown(<number>)` until the submodule is updated and the
plugin rebuilt, or a newer `ardupilotmega.xml` is chosen here.

- **Choose...** opens a file picker; **Clear** goes back to the built-in tables.
- Accepted files: a JSON file with complete mode enums, or a MAVLink `ardupilotmega.xml` of any version.
- The label under the path shows the result: "Built-in tables only", "N entries loaded", or the reason the file was
  rejected (the built-in tables are then used).
- A log matched by the file is named only by the file's enum; the built-in names are never mixed in. A mode the enum
  does not list is shown as `Unknown(<number>)`.

### Getting a MAVLink XML file

Use the `ardupilotmega.xml` dialect file from the [MAVLink repository](https://github.com/mavlink/mavlink)
(`message_definitions/v1.0/ardupilotmega.xml`), or the copy inside a generated `c_library_v2`. Any version works and
no rebuild is needed. The `COPTER_MODE`, `PLANE_MODE`, `ROVER_MODE`, `SUB_MODE` and `TRACKER_MODE` enums replace the
built-in names of those vehicles; a vehicle whose enum is missing keeps the built-in names.

### JSON format

Use JSON for custom firmware, where each entry is a complete enum (mode number to name):

```json
{
  "firmwares": [
    {
      "match": "MyBuild",
      "modes": { "0": "STABILIZE", "5": "LOITER", "6": "RTL", "30": "FOO" }
    }
  ]
}
```

- `firmwares`: entries tried in order; the first whose `match` is found in the log's firmware string is used.
- `match` (optional): case-insensitive text to look for; empty or missing matches every log.
- `modes`: the whole enum, mode number (0-255, as a string) to name.

A file with any error is rejected as a whole. For the full rules see
[`docs/ardupilot/mode_names.md`](../../../../docs/ardupilot/mode_names.md).
