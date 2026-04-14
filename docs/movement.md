# Movement Commands

Commands in the `0x6000` MSN category control all physical motion — absolute and relative moves, jogging, homing, and dwell. Movement is one of the first capabilities a controller must implement.

---

## Command Summary

### Absolute Moves

| Command | Code | Arguments | Types & Units | Required |
|---------|------|-----------|---------------|----------|
| `move_abs_x` | `0x6A01` | x | int32 um | **Yes** |
| `move_abs_y` | `0x6A02` | y | int32 um | **Yes** |
| `move_abs_xy` | `0x6A03` | x, y | int32 um, int32 um | **Yes** |
| `move_abs_z` | `0x6A04` | z | int32 um | No |
| `move_abs_u` | `0x6A08` | u | int32 um | No |
| `move_abs_xyz` | `0x6A07` | x, y, z | int32 um x3 | No |
| `move_abs_xyzu` | `0x6A0F` | x, y, z, u | int32 um x4 | No |

### Relative Moves

| Command | Code | Arguments | Types & Units | Required |
|---------|------|-----------|---------------|----------|
| `move_rel_x` | `0x6101` | x | int32 um | **Yes** |
| `move_rel_y` | `0x6102` | y | int32 um | **Yes** |
| `move_rel_xy` | `0x6103` | x, y | int32 um, int32 um | **Yes** |
| `move_rel_z` | `0x6104` | z | int32 um | No |
| `move_rel_u` | `0x6108` | u | int32 um | No |
| `move_rel_xyz` | `0x6107` | x, y, z | int32 um x3 | No |
| `move_rel_xyzu` | `0x610F` | x, y, z, u | int32 um x4 | No |

### Homing

| Command | Code | Arguments | Required |
|---------|------|-----------|----------|
| `home_x` | `0x6801` | none | No |
| `home_y` | `0x6802` | none | No |
| `home_xy` | `0x6803` | none | **Yes** |
| `home_z` | `0x6804` | none | No |
| `home_u` | `0x6808` | none | No |
| `home_xyz` | `0x6807` | none | No |
| `home_xyzu` | `0x680F` | none | No |

### Other

| Command | Code | Arguments | Types & Units | Required |
|---------|------|-----------|---------------|----------|
| `dwell` | `0x602B` | duration | int32 microseconds | **Yes** |

### Jogging

| Command | Code | Arguments | Required |
|---------|------|-----------|----------|
| `jog_x_pos_start` | `0x6301` | none | Recommended |
| `jog_x_pos_stop` | `0x6201` | none | Recommended |
| `jog_x_neg_start` | `0x6501` | none | Recommended |
| `jog_x_neg_stop` | `0x6401` | none | Recommended |
| `jog_y_pos_start` | `0x6302` | none | Recommended |
| `jog_y_pos_stop` | `0x6202` | none | Recommended |
| `jog_y_neg_start` | `0x6502` | none | Recommended |
| `jog_y_neg_stop` | `0x6402` | none | Recommended |
| `jog_z_pos_start` | `0x6304` | none | Recommended |
| `jog_z_pos_stop` | `0x6204` | none | Recommended |
| `jog_z_neg_start` | `0x6504` | none | Recommended |
| `jog_z_neg_stop` | `0x6404` | none | Recommended |
| `jog_u_pos_start` | `0x6308` | none | Recommended |
| `jog_u_pos_stop` | `0x6208` | none | Recommended |
| `jog_u_neg_start` | `0x6508` | none | Recommended |
| `jog_u_neg_stop` | `0x6408` | none | Recommended |

---

## Reading Command Codes

Movement command codes follow a consistent nibble structure that makes them easy to identify in a hex viewer or protocol analyzer:

- **MSN `0x6`** — Identifies this as a movement command.
- **Second nibble** — Indicates the movement type:
  - `0xA` — Absolute move
  - `0x1` — Relative move
  - `0x2` — Jog positive stop
  - `0x3` — Jog positive start
  - `0x4` — Jog negative stop
  - `0x5` — Jog negative start
  - `0x8` — Home
- **LSN** — Indicates which axes are involved: X=`0x1`, Y=`0x2`, Z=`0x4`, U=`0x8`.

For example, `0x6A03` breaks down as: `6` (movement) + `A` (absolute) + `03` (X + Y).

This structure is a **naming convention for readability**, not a construction mechanism. Always use the exact command codes listed in the tables above — do not attempt to derive new commands by combining nibbles.

---

## Absolute Moves

Move to a specific position in machine coordinates. Arguments are **signed 32-bit big-endian integers** in **micrometers**.

Multi-axis moves pack arguments in axis order: **X, then Y, then Z, then U**. Only the axes indicated by the axis flags in the command code are present. For example:
- `move_abs_x` (`0x6A01`): 1 argument (X)
- `move_abs_xy` (`0x6A03`): 2 arguments (X, Y)
- `move_abs_xyzu` (`0x6A0F`): 4 arguments (X, Y, Z, U)

### Example: Absolute XY Move

Move to (10 mm, 12 mm):

```
Offset  Bytes           Description
------  --------------  ------------------------------------
 0      44 52 47 4E     DRGN header
 4      00 0A           Payload size: 10 bytes
 6      6A 03           Command: move_abs_xy
 8      00 00 27 10     X: 10000 um (10 mm)
12      00 00 2E E0     Y: 12000 um (12 mm)
16      XX XX           CRC16
```

---

## Relative Moves

Same format as absolute moves, but positions are relative to the current location. A positive value moves in the positive direction; a negative value moves in the negative direction.

### Example: Relative X Move

Move 5 mm in the positive X direction:

```
Offset  Bytes           Description
------  --------------  ------------------------------------
 0      44 52 47 4E     DRGN header
 4      00 06           Payload size: 6 bytes
 6      61 01           Command: move_rel_x
 8      00 00 13 88     X: +5000 um (5 mm)
12      XX XX           CRC16
```

---

## Homing

Home commands have no arguments. The controller should execute its homing sequence for the indicated axes — typically moving to limit switches and resetting position counters to zero (or to the configured home offset).

`home_xy` (`0x6803`) is the most common homing command. LightBurn sends this at startup and when the user clicks "Home" in the UI.

### Example: Home XY

```
Offset  Bytes           Description
------  --------------  ---------------------
 0      44 52 47 4E     DRGN header
 4      00 02           Payload size: 2 bytes
 6      68 03           Command: home_xy
 8      XX XX           CRC16
```

---

## Dwell

**Command:** `0x602B`
**Required:** Yes

Pause in place for a specified duration. The single argument is a **signed 32-bit integer** representing the dwell time in **microseconds**.

**Maximum value:** 60,000,000 microseconds (60 seconds).

Dwell is used between operations that need settling time — for example, after a rapid move to allow vibrations to dampen before starting a cut.

### Example: 100ms Dwell

```
Offset  Bytes           Description
------  --------------  ------------------------------------
 0      44 52 47 4E     DRGN header
 4      00 06           Payload size: 6 bytes
 6      60 2B           Command: dwell
 8      00 01 86 A0     Duration: 100000 us (100 ms)
12      XX XX           CRC16
```

---

## Jogging

Jog commands provide continuous motion for manual positioning via the LightBurn UI. They use a start/stop pattern:

1. **`jog_x_pos_start`** (`0x6301`) — Begin moving X in the positive direction
2. **`jog_x_pos_stop`** (`0x6201`) — Stop the positive X movement

The controller determines the jog speed based on its configuration (see `cfg_x_key_jumpoff_speed` and `cfg_x_key_accel` in [Configuration](configuration.md)).

Jog commands have **no arguments** — the payload is just the 2-byte command code.

Jogging is **strongly recommended** for a good user experience. Without jog support, users cannot manually position the laser head from LightBurn's interface.

### Example: Start and Stop X Positive Jog

Start:
```
Offset  Bytes           Description
------  --------------  ---------------------
 0      44 52 47 4E     DRGN header
 4      00 02           Payload size: 2 bytes
 6      63 01           Command: jog_x_pos_start
 8      XX XX           CRC16
```

Stop:
```
Offset  Bytes           Description
------  --------------  ---------------------
 0      44 52 47 4E     DRGN header
 4      00 02           Payload size: 2 bytes
 6      62 01           Command: jog_x_pos_stop
 8      XX XX           CRC16
```
