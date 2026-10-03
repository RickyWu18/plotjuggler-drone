# ArduPilot BIN Log Format — Protocol Reference

> Derived from `ap_log/AP_Logger/` source code for implementing a PlotJuggler data loader plugin.

---

## 1. Overview

ArduPilot binary log files (`.bin`) are a flat stream of variable-length binary packets.
There is no global file header; the format is entirely self-describing through special
**FMT** packets that appear early in the stream and declare every other message type
that follows.

Each packet begins with a 3-byte header, followed by a payload whose layout is
defined by the corresponding FMT record.

---

## 2. Packet Structure

### 2.1 Packet Header

Every packet begins with the same 3-byte header (defined in `LogStructure.h`):

| Offset | Size | Value      | Description         |
|--------|------|------------|---------------------|
| 0      | 1    | `0xA3`     | HEAD_BYTE1 (magic)  |
| 1      | 1    | `0x95`     | HEAD_BYTE2 (magic)  |
| 2      | 1    | `uint8_t`  | Message type ID     |

```
#define HEAD_BYTE1  0xA3
#define HEAD_BYTE2  0x95
#define LOG_PACKET_HEADER_LEN  3
```

The message type ID maps to a **FMT** record (see §3). The special case is
`msgid == 0x80` (128), which is the FMT message itself — its format is hardcoded
and does not require a prior FMT declaration.

### 2.2 Parsing Algorithm

```
loop:
    read 3 bytes
    if bytes[0] != 0xA3 or bytes[1] != 0x95:
        seek forward 1 byte and retry   // re-sync on corruption
    msgid = bytes[2]
    if msgid == FMT_MSG_ID (128):
        read remaining FMT payload (86 bytes) → register new message format
    else:
        look up msgid in format table → read `length - 3` payload bytes
        decode fields according to format string
```

---

## 3. FMT — Format Definition Message

**Message type ID: 128 (0x80)** — this ID is fixed and must always be 128.

The `log_Format` struct (packed, little-endian):

```c
struct PACKED log_Format {
    uint8_t  head1;       // 0xA3
    uint8_t  head2;       // 0x95
    uint8_t  msgid;       // 128
    uint8_t  type;        // message type ID being defined
    uint8_t  length;      // total packet length in bytes (header + payload)
    char     name[4];     // message name, NOT null-terminated (padded with 0x00)
    char     format[16];  // format string (see §4), NOT null-terminated
    char     labels[64];  // comma-separated field names, NOT null-terminated
};
// Total: 3 + 86 = 89 bytes
```

**Key points:**
- `type` is the ID assigned to every future packet of this kind.
- `length` includes the 3-byte header; the payload is `length - 3` bytes.
- `name` is at most 4 characters (zero-padded, no null terminator in the struct).
- `format` is at most 16 characters; each character encodes one field (see §4).
- `labels` is at most 64 characters; comma-separated names match `format` one-for-one.
- String fields are NOT null-terminated in the binary — treat them as fixed-width.

---

## 4. Field Format Characters

Each character in a `format` string encodes the C storage type of one field.
Fields are packed with **no padding** (all structs are `PACKED`).
All multi-byte integers are **little-endian**.

| Char | C Type         | Size (bytes) | Notes |
|------|---------------|-------------|-------|
| `a`  | `int16_t[32]` | 64          | Array of 32 signed 16-bit integers |
| `b`  | `int8_t`      | 1           | |
| `B`  | `uint8_t`     | 1           | |
| `h`  | `int16_t`     | 2           | |
| `H`  | `uint16_t`    | 2           | |
| `i`  | `int32_t`     | 4           | |
| `I`  | `uint32_t`    | 4           | |
| `f`  | `float`       | 4           | IEEE 754 single precision |
| `d`  | `double`      | 8           | IEEE 754 double precision |
| `n`  | `char[4]`     | 4           | Short string, zero-padded |
| `N`  | `char[16]`    | 16          | Medium string, zero-padded |
| `Z`  | `char[64]`    | 64          | Long string, zero-padded |
| `L`  | `int32_t`     | 4           | Latitude or longitude × 1e7 (e.g. -35.133 → -351330000) |
| `M`  | `uint8_t`     | 1           | Flight mode enum value |
| `q`  | `int64_t`     | 8           | |
| `Q`  | `uint64_t`    | 8           | Timestamps use this type (microseconds since boot) |
| `g`  | `float16_t`   | 2           | Half-precision float (newer logs) |

### Legacy Format Characters (do not rely on for scaling)

These are deprecated; the raw storage type is still used but scaling via FMTU is preferred.

| Char | Equivalent    | Implicit scale |
|------|--------------|----------------|
| `c`  | `int16_t`    | ×100 (implied) |
| `C`  | `uint16_t`   | ×100 (implied) |
| `e`  | `int32_t`    | ×100 (implied) |
| `E`  | `uint32_t`   | ×100 (implied) |

> **Important:** A GCS/loader should NOT infer scaling from the format character alone.
> Use the FMTU message (§7) for authoritative scaling. The legacy `c/C/e/E` types
> only tell you the storage size; treat them exactly like their non-scaled counterparts
> (`h/H/i/I`) and rely on the multiplier from FMTU.

---

## 5. Message Type ID Allocation

Message type IDs are `uint8_t` (0–255):

| Range    | Usage                          |
|----------|-------------------------------|
| 0–31     | Vehicle-specific (e.g. Copter, Plane, Rover) |
| 32–127   | Common library messages        |
| 128      | **FMT** (always hardcoded)     |
| 129–254  | Common library messages (continued) |
| 255      | Reserved for future use        |

Selected well-known IDs (from `LogMessages` enum):

| ID  | Name    | Description |
|-----|---------|-------------|
| 128 | FMT     | Message format definition |
| 33  | –       | NavEKF2 IDs block start |
| 35  | MSG     | Text message |
| 36  | RCIN    | RC input channels 1-14 |
| 37  | RCI2    | RC input channels 15-16 + flags |
| 38  | RCOU    | Servo output channels 1-14 |
| 39  | RSSI    | RSSI/link quality |
| 32  | PARM    | Parameter value |
| See source | FMTU | Format units/multipliers |
| See source | UNIT | Unit string definition |
| See source | MULT | Multiplier definition |

The exact numeric value of most IDs is determined at compile time by the `LogMessages`
enum. Since the file is self-describing via FMT packets, a parser does **not** need to
hard-code these numbers — it simply builds a lookup table from FMT records.

---

## 6. Startup Sequence

At the start of every log file, before any data messages, ArduPilot emits a
sequence of housekeeping messages (written as "critical" blocks):

1. **FMT** records — one for every message type that will appear in the file.
2. **UNIT** records — define unit strings referenced by FMTU.
3. **MULT** records — define numeric multipliers referenced by FMTU.
4. **FMTU** records — associate units and multipliers with each FMT field.
5. **PARM** records — all vehicle parameter values at log start.
6. **VER** record — firmware version and board info.
7. **MSG** records — startup text messages.
8. Vehicle-specific startup messages (missions, rally points, etc.).

A robust parser should handle FMT records appearing anywhere in the stream
(not just at the start), since dynamic message types can be registered mid-flight.

---

## 7. UNIT, MULT, and FMTU Messages

These three message types together provide physical units and scaling for every field.

### 7.1 UNIT — Unit String Definition

```c
struct PACKED log_Unit {
    uint8_t  head1, head2, msgid;
    uint64_t time_us;    // Q — timestamp (µs since boot)
    char     type;       // b — single character ID
    char     unit[64];   // Z — unit string (e.g. "m/s", "deg")
};
```

Maps a single ASCII character → human-readable unit string.

### 7.2 MULT — Multiplier Definition

```c
struct PACKED log_Format_Multiplier {
    uint8_t  head1, head2, msgid;
    uint64_t time_us;    // Q
    char     type;       // b — single character ID
    double   multiplier; // d — numeric scale factor
};
```

Maps a single ASCII character → a `double` multiplier.

### 7.3 FMTU — Format Units Assignment

```c
struct PACKED log_Format_Units {
    uint8_t  head1, head2, msgid;
    uint64_t time_us;    // Q
    uint8_t  format_type; // B — the msg_type of the FMT record this applies to
    char     units[16];   // N — one character per field in FMT.format (UNIT IDs)
    char     multipliers[16]; // N — one character per field (MULT IDs)
};
```

Each character at position `i` in `units` / `multipliers` corresponds to field `i`
of the FMT record with `msg_type == format_type`.

To get the physical value of a raw field:

```
physical_value = raw_value * multiplier_lookup[fmtu.multipliers[i]]
unit_string    = unit_lookup[fmtu.units[i]]
```

### 7.4 Built-in Unit Characters

| Char | Unit      | Description                         |
|------|-----------|-------------------------------------|
| `-`  | (none)    | Dimensionless, string, or N/A       |
| `?`  | UNKNOWN   | Not yet determined                  |
| `s`  | s         | Seconds                             |
| `A`  | A         | Ampere                              |
| `a`  | Ah        | Ampere-hours                        |
| `d`  | deg       | Degrees (angle, -180 to 180)        |
| `h`  | degheading| Heading degrees (0 to 360)          |
| `D`  | deglatitude | Degrees of latitude               |
| `U`  | deglongitude | Degrees of longitude             |
| `k`  | deg/s     | Degrees per second                  |
| `e`  | deg/s/s   | Degrees per second squared          |
| `E`  | rad/s     | Radians per second                  |
| `r`  | rad       | Radians                             |
| `L`  | rad/s/s   | Radians per second squared          |
| `m`  | m         | Metres                              |
| `n`  | m/s       | Metres per second                   |
| `o`  | m/s/s     | Metres per second squared           |
| `v`  | V         | Volt                                |
| `P`  | Pa        | Pascal                              |
| `O`  | degC      | Degrees Celsius                     |
| `G`  | Gauss     | Magnetic field (Gauss)              |
| `%`  | %         | Percent                             |
| `S`  | satellites| Satellite count                     |
| `Y`  | us        | Microseconds (PWM pulse width)      |
| `z`  | Hz        | Hertz                               |
| `#`  | instance  | Sensor instance number              |
| `W`  | Watt      | Watts                               |
| `X`  | W.h       | Watt-hours                          |
| `J`  | W.s       | Joules                              |
| `i`  | A.s       | Ampere-seconds                      |
| `b`  | B         | Bytes                               |
| `B`  | B/s       | Bytes per second                    |
| `q`  | rpm       | Revolutions per minute              |
| `u`  | ppm       | Pulses per minute                   |

### 7.5 Built-in Multiplier Characters

| Char | Multiplier | Notes                                   |
|------|------------|-----------------------------------------|
| `-`  | 0          | No multiplier (strings)                 |
| `?`  | 1          | Unknown / identity                      |
| `2`  | 1e2        | ×100                                   |
| `1`  | 1e1        | ×10                                    |
| `0`  | 1e0        | ×1 (identity)                          |
| `A`  | 1e-1       | ×0.1                                   |
| `B`  | 1e-2       | ×0.01                                  |
| `C`  | 1e-3       | ×0.001                                 |
| `D`  | 1e-4       | ×0.0001                                |
| `E`  | 1e-5       | ×0.00001                               |
| `F`  | 1e-6       | ×0.000001                              |
| `G`  | 1e-7       | ×0.0000001                             |
| `I`  | 1e-9       | ×1e-9                                  |
| `!`  | 3.6        | km/h → m/s; mAh → A·s                  |
| `/`  | 3600       | Ah → A·s                               |

---

## 8. Common Message Formats

Below are the most important messages for flight data analysis. All timestamps
(`TimeUS`) are `uint64_t` in microseconds since system boot.

### 8.1 FMT (128)
Already described in §3. No TimeUS field.

### 8.2 PARM — Parameters
```
Format: QNff
Labels: TimeUS,Name,Value,Default
```
| Field   | Type    | Description                   |
|---------|---------|-------------------------------|
| TimeUS  | Q       | Timestamp µs                  |
| Name    | N (16)  | Parameter name                |
| Value   | f       | Current value                 |
| Default | f       | Compiled-in default value     |

### 8.3 MSG — Text Messages
```
Format: QZ
Labels: TimeUS,Message
```
| Field   | Type    | Description              |
|---------|---------|--------------------------|
| TimeUS  | Q       | Timestamp µs             |
| Message | Z (64)  | Human-readable text      |

### 8.4 MODE — Flight Mode
```
Format: QMBB
Labels: TimeUS,Mode,ModeNum,Rsn
```
| Field   | Type | Description                       |
|---------|------|-----------------------------------|
| TimeUS  | Q    | Timestamp µs                      |
| Mode    | M    | Flight mode (vehicle-specific enum) |
| ModeNum | B    | Same as Mode (alias)              |
| Rsn     | B    | Reason for mode change            |

### 8.5 ARM — Arming Status
```
Format: QBIBB
Labels: TimeUS,ArmState,ArmChecks,Forced,Method
```

### 8.6 EV — Events
```
Format: QB
Labels: TimeUS,Id
```
`Id` values are from the `LogEvent` enum (see §9.1).

### 8.7 ERR — Errors
```
Format: QBB
Labels: TimeUS,Subsys,ECode
```
`Subsys` values from `LogErrorSubsystem` enum (see §9.2).

### 8.8 RCIN — RC Input Channels 1–14
```
Format: QHHHHHHHHHHHHHH
Labels: TimeUS,C1..C14
Units: sYYYYYYYYYYYYYY (µs PWM values)
```
Each channel is `uint16_t`, typically 1000–2000 µs PWM.

### 8.9 RCI2 — RC Input Channels 15–16 + Flags
```
Format: QHHHB
Labels: TimeUS,C15,C16,OMask,Flags
```
`Flags` bits: bit 0 = HAS_VALID_INPUT, bit 1 = IN_RC_FAILSAFE.

### 8.10 RCOU / RCO2 / RCO3 — Servo Outputs
```
RCOU: Format QHHHHHHHHHHHHHH → channels 1–14
RCO2: Format QHHHH            → channels 15–18
RCO3: Format QHHHHHHHHHHHHHH → channels 19–32
Units: sYYYY... (µs PWM)
```

### 8.11 PIDR/PIDP/PIDY/PIDA/PIDS/PIDN/PIDE — PID Controllers
```
Format: QffffffffffB
Labels: TimeUS,Tar,Act,Err,P,I,D,FF,DFF,Dmod,SRate,Flags
```
| Field  | Type | Description                    |
|--------|------|--------------------------------|
| TimeUS | Q    | Timestamp µs                   |
| Tar    | f    | Target / desired value         |
| Act    | f    | Actual / achieved value        |
| Err    | f    | Error (Tar - Act)              |
| P      | f    | Proportional term output       |
| I      | f    | Integral term output           |
| D      | f    | Derivative term output         |
| FF     | f    | Feed-forward term              |
| DFF    | f    | Derivative feed-forward        |
| Dmod   | f    | D-gain modifier                |
| SRate  | f    | Slew rate                      |
| Flags  | B    | Bitmask (bit0=LIMIT, bit1=PD_SUM_LIMIT, bit2=RESET, bit3=I_TERM_SET) |

### 8.12 MAG — Magnetometer
```
Format: QBhhhhhhhhhBI
Labels: TimeUS,I,MagX,MagY,MagZ,OfsX,OfsY,OfsZ,MOX,MOY,MOZ,Health,S
```
Raw field values in milli-Gauss; multiply by `1e-3` (MULT `C`) to get Gauss.

### 8.13 ARSP — Airspeed Sensor
```
Format: QBffcffBBffB
Labels: TimeUS,I,Airspeed,DiffPress,Temp,RawPress,Offset,U,H,Hp,TR,Pri
```

### 8.14 POWR — Power Status
```
Format: QffHHB
Labels: TimeUS,Vcc,VServo,Flags,AccFlags,Safety
```

### 8.15 MCU — Microcontroller Monitor
```
Format: Qffff
Labels: TimeUS,MTemp,MVolt,MVmin,MVmax
```

### 8.16 RFND — Rangefinder
```
Format: QBCBBb
Labels: TimeUS,Instance,Dist,Stat,Orient,Quality
```
`Dist` is `uint16_t` type `C` (legacy) — value in centimetres (divide by 100 for metres).

### 8.17 RAD — Telemetry Radio
```
Format: QBBBBBHH
Labels: TimeUS,RSSI,RemRSSI,TxBuf,Noise,RemNoise,RxErrors,Fixed
```

### 8.18 PM — Performance Monitor
```
Format: QHHHIIHHIIIIIIQ
Labels: TimeUS,LR,NLon,NL,MaxT,Mem,Load,ErrL,InE,ErC,SPIC,I2CC,I2CI,Ex,R
```

### 8.19 VER — Firmware Version
```
Format: QBHBBBBIZHBB
Labels: TimeUS,BT,BST,Maj,Min,Pat,FWT,GH,FWS,APJ,BU,FV
```
`FWS` is type `Z` (64-byte string) containing the full firmware version string.

### 8.20 FILE — Embedded File Content

ArduPilot can embed the contents of arbitrary on-vehicle files directly into the BIN
stream using `FILE` messages. Each message carries a 64-byte chunk of one file; the
receiver reassembles the original file from chunks ordered by `offset`.

```c
struct PACKED log_File {
    LOG_PACKET_HEADER;        // 3 bytes: 0xA3 0x95 <msgid>
    char     filename[16];    // N — file name (zero-padded, NOT null-terminated)
    uint32_t offset;          // I — byte offset of this chunk within the original file
    uint8_t  length;          // B — number of valid bytes in data[] (1–64)
    char     data[64];        // raw file bytes for this chunk
};
// Total: 3 + 16 + 4 + 1 + 64 = 88 bytes
```

**Reassembly rule:** collect all `FILE` messages with the same `filename`, sort by
`offset`, concatenate `data[0..length-1]` from each chunk in order.

#### Files logged automatically on arming

Every time the vehicle arms, ArduPilot schedules the following files for embedding
(defined in `AP_Logger::prepare_at_arming_sys_file_logging()`):

| Embedded filename | Source path | Contents |
|---|---|---|
| `uarts.txt` | `@SYS/uarts.txt` | UART configuration |
| `memory.txt` | `@SYS/memory.txt` | Heap / memory statistics |
| `threads.txt` | `@SYS/threads.txt` | Active thread list |
| `timers.txt` | `@SYS/timers.txt` | Timer usage |
| `hwdef.dat` | `@ROMFS/hwdef.dat` | Hardware definition (board pinout) |
| `storage.bin` | `@SYS/storage.bin` | Raw parameter storage area |
| `crash_dump.bin` | `@SYS/crash_dump.bin` | Crash dump from previous run (if present) |
| `defaults.parm` | `@ROMFS/defaults.parm` | Compiled-in default parameters |

> `dma.txt` is also logged in debug builds.

**`crash_dump.bin` fast-path:** if a crash dump exists, ArduPilot logs it at **10×**
the normal chunk rate (≈ one chunk per main-loop tick instead of one per 10 ticks) so
that a full ~450 KB dump completes in roughly 1 minute. A GCS status message is sent:
`"Logging @SYS/crash_dump.bin"`.

Additionally, `AP_Logger::log_file_content(const char *filename)` may be called at any
time by vehicle code (e.g. by Lua scripting) to embed arbitrary additional files on the
`normal_file_content` channel.

---

## 9. Enumeration Reference

### 9.1 LogEvent (EV.Id)

| Value | Name                        |
|-------|-----------------------------|
| 10    | ARMED                       |
| 11    | DISARMED                    |
| 15    | AUTO_ARMED                  |
| 17    | LAND_COMPLETE_MAYBE         |
| 18    | LAND_COMPLETE               |
| 19    | LOST_GPS                    |
| 21    | FLIP_START                  |
| 22    | FLIP_END                    |
| 25    | SET_HOME                    |
| 38    | SAVE_TRIM                   |
| 41    | FENCE_ENABLE                |
| 42    | FENCE_DISABLE               |
| 52    | LANDING_GEAR_DEPLOYED       |
| 53    | LANDING_GEAR_RETRACTED      |
| 54    | MOTORS_EMERGENCY_STOPPED    |
| 60    | EKF_ALT_RESET               |
| 62    | EKF_YAW_RESET               |
| 67    | GPS_PRIMARY_CHANGED         |

### 9.2 LogErrorSubsystem (ERR.Subsys)

| Value | Name                  |
|-------|-----------------------|
| 1     | MAIN                  |
| 2     | RADIO                 |
| 3     | COMPASS               |
| 5     | FAILSAFE_RADIO        |
| 6     | FAILSAFE_BATT         |
| 8     | FAILSAFE_GCS          |
| 9     | FAILSAFE_FENCE        |
| 10    | FLIGHT_MODE           |
| 11    | GPS                   |
| 12    | CRASH_CHECK           |
| 16    | EKFCHECK              |
| 17    | FAILSAFE_EKFINAV      |
| 18    | BARO                  |
| 19    | CPU                   |
| 21    | TERRAIN               |
| 22    | NAVIGATION            |
| 24    | EKF_PRIMARY           |
| 30    | INTERNAL_ERROR        |

---

## 10. Parsing Implementation Guide

### 10.1 Data Structures

```cpp
struct FieldDef {
    char     format_char;   // from FMT.format
    uint8_t  field_size;    // bytes: computed from format_char
    std::string label;      // from FMT.labels (split by comma)
    char     unit_id;       // from FMTU.units[i]
    char     mult_id;       // from FMTU.multipliers[i]
};

struct MessageDef {
    uint8_t  msg_type;
    uint8_t  msg_len;
    std::string name;
    std::vector<FieldDef> fields;
};

std::unordered_map<uint8_t, MessageDef> format_table;
std::unordered_map<char, std::string>   unit_table;
std::unordered_map<char, double>        mult_table;
```

### 10.2 Field Size Lookup

```cpp
int field_size(char fmt_char) {
    switch (fmt_char) {
        case 'b': case 'B': case 'M':       return 1;
        case 'h': case 'H': case 'c':
        case 'C': case 'g':                 return 2;
        case 'i': case 'I': case 'f':
        case 'L': case 'e': case 'E':       return 4;
        case 'q': case 'Q': case 'd':       return 8;
        case 'n':                           return 4;
        case 'N':                           return 16;
        case 'Z':                           return 64;
        case 'a':                           return 64;  // int16_t[32]
        default:                            return -1;  // unknown
    }
}
```

### 10.3 Reading a Packet

```cpp
bool read_packet(Stream &s, FormatTable &fmts, Record &out) {
    uint8_t hdr[3];
    if (!s.read(hdr, 3)) return false;
    if (hdr[0] != 0xA3 || hdr[1] != 0x95) {
        // sync lost — caller must seek to next 0xA3 0x95
        return false;
    }
    uint8_t msg_type = hdr[2];
    if (msg_type == 128) {
        // FMT packet: fixed 89-byte total, read remaining 86 bytes
        uint8_t payload[86];
        s.read(payload, 86);
        parse_fmt(payload, fmts);
        return true;
    }
    auto it = fmts.find(msg_type);
    if (it == fmts.end()) return false; // unknown type
    const MessageDef &def = it->second;
    size_t payload_len = def.msg_len - 3;
    std::vector<uint8_t> payload(payload_len);
    s.read(payload.data(), payload_len);
    decode_payload(payload, def, out);
    return true;
}
```

### 10.4 Applying Scaling

```cpp
double scaled_value(double raw, char mult_id) {
    if (mult_id == '-') return raw;             // string / no-scale
    auto it = mult_table.find(mult_id);
    if (it == mult_table.end()) return raw;
    return raw * it->second;
}
```

### 10.5 Handling the FMT Message Itself

The FMT message has a hardcoded structure (it is NOT described by another FMT):

```
Total size: 89 bytes
Offsets after the 3-byte header:
  [0]     uint8_t  type    — message type ID being defined
  [1]     uint8_t  length  — total size of that message (incl. header)
  [2..5]  char[4]  name
  [6..21] char[16] format
  [22..85]char[64] labels
```

When parsing FMT.format, split `labels` by comma to align with `format` characters.
Build the FieldDef list simultaneously.

---

## 11. Timestamp Handling

- All timestamps are `uint64_t` **microseconds since system boot** (field name `TimeUS`, format char `Q`).
- There is no absolute wall-clock timestamp in BIN format by default.
- GPS messages contain GPS time of week which can be used to establish absolute time.
- The PM (performance) message contains an `R` (RTC) field with seconds since Unix epoch.
- The PARM message emitted at startup records all parameters; GPS time parameters can help anchor absolute time.

---

## 12. File Naming Conventions

Files produced by the filesystem backend (`AP_Logger_File`) follow:

```
<log_directory>/XXXXXXXXX.BIN
```

where `XXXXXXXXX` is a zero-padded sequence number (e.g. `00000001.BIN`).
A `LASTLOG.TXT` file in the same directory contains the most recently written log number as a decimal string.

---

## 13. Known Gotchas

1. **FMT before use** — FMT packets for a message type appear before any instance of that type. However, do not assume all FMTs come before all data; read FMTs dynamically.

2. **String fields are NOT null-terminated** inside structs. When copying `char[N]` fields, use `strnlen(buf, N)` to find the actual length, or copy all N bytes.

3. **FMTU alignment** — The `units` and `multipliers` strings in FMTU are aligned to fields **after** the implicit `TimeUS` first field in most messages. The first character of `units` corresponds to field index 0 of the format string (which is `Q` for `TimeUS`). Typically `units[0] == 's'` (seconds) and `multipliers[0] == 'F'` (×1e-6, converting µs → s).

4. **Legacy c/C/e/E types** — Treat these identically to `h/H/i/I` for size. Do NOT multiply by 100. The FMTU multiplier column provides the correct scale.

5. **`L` type (latitude/longitude)** — Raw value is `int32_t` representing degrees × 1e7. To get degrees: `raw / 1e7`. Note: The FMTU multiplier for `L` fields is typically `G` (×1e-7).

6. **`M` (flight mode)** — The numeric value is vehicle-specific. Copter, Plane, Rover, and Sub each have different mode enumerations; use the vehicle type from PARM/VER messages to interpret.

7. **Padding / alignment** — All log structs use `PACKED` attribute. There is no padding between fields. Parse sequentially byte-by-byte according to the format string.

8. **Corrupt data recovery** — BIN files may be truncated (vehicle lost power) or contain partially written pages. Implement sync recovery: on header mismatch, scan forward for the next `0xA3 0x95` byte pair.

9. **Multi-instance sensors** — Sensors like IMU, compass, and GPS may appear multiple times per timestamp with an `I` or `instance` field. These are separate time series and should be split into separate PlotJuggler channels (e.g. `IMU[0]/AccX`, `IMU[1]/AccX`).

---

## 14. Source File Reference

| File                              | Relevance |
|-----------------------------------|-----------|
| `AP_Logger/LogStructure.h`        | All struct definitions, FMT types, unit/multiplier tables, LogMessages enum |
| `AP_Logger/AP_Logger.h`           | LogEvent, LogErrorSubsystem, LogErrorCode enums; AP_Logger class API |
| `AP_Logger/LogFile.cpp`           | Fill_Format, Fill_Format_Units, Write_* implementations showing exact packing |
| `AP_Logger/AP_Logger_File.h`      | File backend: naming, buffering, write chunk size (4096 bytes) |
| `AP_Logger/AP_Logger.cpp` (≥1546) | `prepare_at_arming_sys_file_logging()`, `log_file_content()`, `file_content_update()` — FILE message emission |
| `AP_Logger/AP_Logger_Block.h`     | Flash block backend: PageHeader (FilePage+FileNumber), FileHeader (utc_secs) |
| `AP_Logger/README.md`             | Official quick-reference for format chars, units, and multipliers |
