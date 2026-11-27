# DataStreamMavlink (PJ3)

Streams live MAVLink telemetry over UDP, TCP, or Serial into PlotJuggler 3.x.

## Features

- **Three transports:** UDP (default port 14550), TCP client, and Serial port — switchable from the connection dialog.
- **Zero-config field discovery:** every numeric field in every MAVLink message is automatically mapped to a plot series named `mav/<sysid>.<compid>/<MSG_NAME>/<field>`.
- **Multi-vehicle:** differentiates streams by `sysid.compid`, so data from multiple vehicles on the same link is kept separate.
- **Message Interval control:** via the **"Message Intervals…"** toolbar action, view live message rates and send `SET_MESSAGE_INTERVAL` commands back to the vehicle to tune what gets streamed and how fast.
