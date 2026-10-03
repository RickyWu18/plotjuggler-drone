# ArduPilot BIN Log — Decoding Steps

---

## Step 1: Open the File and Sync to the First Packet

Read bytes until you find the 2-byte magic sequence `0xA3 0x95`. The byte
immediately after is the `msgid`.

```
[0xA3] [0x95] [msgid] [payload...]
```

---

## Step 2: Build a Format Table by Parsing FMT Packets

`msgid == 128` is always a **FMT** packet. Its layout is hardcoded (89 bytes total):

```
[0xA3][0x95][0x80] | type(1) | length(1) | name(4) | format(16) | labels(64)
```

| Field    | Size | Description |
|----------|------|-------------|
| `type`   | 1    | The message ID this FMT defines |
| `length` | 1    | Total byte size of that message (header included) |
| `format` | 16   | Format string e.g. `"QffB"` — one char per field |
| `labels` | 64   | Comma-separated names e.g. `"TimeUS,Roll,Pitch,Flags"` |

Store these in a map:

```cpp
format_table[type] = { length, name, format, labels }
```

---

## Step 3: Parse UNIT, MULT, FMTU Packets for Scaling

When you encounter these message IDs, populate lookup tables:

- **UNIT** → `unit_table[char_id] = unit_string`  (e.g. `'m'` → `"m"`)
- **MULT** → `mult_table[char_id] = multiplier`   (e.g. `'F'` → `1e-6`)
- **FMTU** → `fmtu_table[format_type] = { units_string, multipliers_string }`
  — one character per field, positionally aligned with `format`

---

## Step 4: Decode Every Subsequent Packet

For any `msgid != 128`, look it up in `format_table`. Read exactly `length - 3`
payload bytes, then walk the `format` string character by character:

| Char     | Bytes | Interpret as                          |
|----------|-------|---------------------------------------|
| `Q`      | 8     | `uint64_t` (timestamp µs since boot)  |
| `q`      | 8     | `int64_t`                             |
| `d`      | 8     | `double` (IEEE 754)                   |
| `f`      | 4     | `float` (IEEE 754)                    |
| `I`      | 4     | `uint32_t`                            |
| `i`      | 4     | `int32_t`                             |
| `L`      | 4     | `int32_t` — lat/lon stored as deg×1e7 |
| `e`      | 4     | `int32_t` (legacy, treat as `i`)      |
| `E`      | 4     | `uint32_t` (legacy, treat as `I`)     |
| `H`      | 2     | `uint16_t`                            |
| `h`      | 2     | `int16_t`                             |
| `C`      | 2     | `uint16_t` (legacy, treat as `H`)     |
| `c`      | 2     | `int16_t` (legacy, treat as `h`)      |
| `B`/`M`  | 1     | `uint8_t`                             |
| `b`      | 1     | `int8_t`                              |
| `n`      | 4     | `char[4]` — zero-padded, not null-terminated |
| `N`      | 16    | `char[16]` — zero-padded, not null-terminated |
| `Z`      | 64    | `char[64]` — zero-padded, not null-terminated |
| `a`      | 64    | `int16_t[32]` array                   |

All multi-byte integers are **little-endian**. There is **no padding** between fields.

---

## Step 5: Apply Scaling to Get Physical Values

```
physical_value = raw_value * mult_table[ fmtu.multipliers[field_index] ]
unit_string    = unit_table[ fmtu.units[field_index] ]
```

**Example:** `TimeUS` has unit `'s'` and multiplier `'F'` (×1e-6).
Raw value `12345678` → `12345678 × 1e-6 = 12.345678 s`.

**Example:** `L`-type lat/lon field has multiplier `'G'` (×1e-7).
Raw value `-351332423` → `-351332423 × 1e-7 = -35.1332423 deg`.

> Do **not** infer scaling from the format character alone (especially legacy
> `c/C/e/E`). Always use the FMTU multiplier.

---

## Step 6: Handle Multi-Instance Sensors

Messages like `MAG`, `IMU`, and `GPS` include an instance field (`I` or `Instance`).
Split them into separate time series:

```
MAG[0]/MagX,  MAG[1]/MagX
IMU[0]/AccX,  IMU[1]/AccX
```

---

## Step 7: Handle Sync Errors and Truncated Files

If `bytes[0] != 0xA3` or `bytes[1] != 0x95`, the stream is out of sync
(common at the end of truncated or power-loss files). Scan forward one byte
at a time until the next `0xA3 0x95` pair is found.

---

## Step 8: Reassemble Embedded Files from FILE Messages

ArduPilot can embed on-vehicle files (system diagnostics, crash dumps, Lua scripts,
parameters, etc.) directly in the BIN stream as `FILE` messages. Each message carries
a 64-byte chunk:

```
filename[16]  — file name, zero-padded, NOT null-terminated
offset  (I)   — byte position of this chunk in the original file
length  (B)   — number of valid bytes in this chunk (1–64)
data    [64]  — raw chunk bytes
```

**Reassembly algorithm:**

```
file_chunks = {}   # dict: filename -> sorted list of (offset, data)

for each FILE message:
    name = strip_nul(msg.filename)
    file_chunks[name].append( (msg.offset, msg.data[:msg.length]) )

for name, chunks in file_chunks:
    chunks.sort(key=lambda c: c.offset)
    output = b"".join(data for _, data in chunks)
    save as output/<name>
```

**Files typically embedded on every arming:**

| Filename in BIN | Contents |
|---|---|
| `uarts.txt` | UART configuration |
| `memory.txt` | Heap / memory statistics |
| `threads.txt` | Active thread list |
| `timers.txt` | Timer usage |
| `hwdef.dat` | Hardware definition (board pinout) |
| `storage.bin` | Raw parameter storage |
| `crash_dump.bin` | Crash dump from previous run (if present) |
| `defaults.parm` | Compiled-in default parameters |

> Mission Planner's log browser surfaces these as downloadable attachments.
> `crash_dump.bin` is logged at 10× the normal rate to minimise logging time (~1 min for 450 KB).

---

## Summary: Parse Loop

```
open file
loop:
    read 3 bytes
    if magic mismatch → scan forward 1 byte, retry
    msgid = bytes[2]

    if msgid == 128:
        read 86 bytes → parse FMT → store in format_table
    elif msgid == UNIT_ID:
        read (length-3) bytes → store in unit_table
    elif msgid == MULT_ID:
        read (length-3) bytes → store in mult_table
    elif msgid == FMTU_ID:
        read (length-3) bytes → store in fmtu_table
    else:
        def = format_table[msgid]
        read (def.length - 3) bytes
        for each field in def.format:
            raw = read N bytes, interpret as C type
            physical = raw * mult_table[ fmtu_table[msgid].multipliers[i] ]
            label = def.labels[i]
            emit (timestamp, label, physical)
```
