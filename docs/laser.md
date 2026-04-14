# Laser Commands

Commands in the `0x1000` MSN category control laser output — power levels, frequency, enable/disable, and emission on/off.

---

## Command Summary

| Command | Code | Arguments | Types & Units | Required |
|---------|------|-----------|---------------|----------|
| `laser_power_min` | `0x15A0` | laser_index, power | int8, int16 percent | **Yes** |
| `laser_power_max` | `0x15A1` | laser_index, power | int8, int16 percent | **Yes** |
| `laser_freq` | `0x15F0` | laser_index, freq | int8, int32 Hz | **Yes** |
| `laser_enable` | `0x15E1` | laser_index | int8 | **Yes** |
| `laser_disable` | `0x15E2` | laser_index | int8 | **Yes** |
| `laser_on` | `0x15C2` | laser_index | int8 | **Yes** |
| `laser_off` | `0x15C1` | laser_index | int8 | **Yes** |
| `laser_offset_xy` | `0x15B3` | laser_index, x, y | int8, int32 um, int32 um | No |
| `focus_z` | `0x1F04` | — | — | DRAFT |

---

## Laser Index

The first argument to most laser commands is an **int8 laser index**:

| Index | Meaning |
|-------|---------|
| 0 | Layer default (for `laser_on` / `laser_off` only) |
| 1 | First laser tube |
| 2 | Second laser tube |

Most machines have a single laser tube (index 1). Dual-tube machines use indices 1 and 2. Index 0 is a convenience value for on/off commands that means "use the default laser for this layer."

---

## Power Control

### `laser_power_min` (`0x15A0`)

**Required:** Yes

Set the minimum power level for a laser. This defines the lower bound of the power range for the current layer.

**Arguments:**
| Byte Offset | Type | Description |
|-------------|------|-------------|
| 0 | int8 | Laser index |
| 1-2 | int16 | Power (percent, 0-100) |

**Payload size:** 5 bytes (2 cmd + 1 index + 2 power)

### `laser_power_max` (`0x15A1`)

**Required:** Yes

Set the maximum power level for a laser. This defines the upper bound of the power range for the current layer. During cutting, the actual power varies between min and max based on speed.

**Arguments:** Same format as `laser_power_min`.

### Example: Set Laser 1 Max Power to 80%

```
Offset  Bytes           Description
------  --------------  ------------------------------------
 0      44 52 47 4E     DRGN header
 4      00 05           Payload size: 5 bytes
 6      15 A1           Command: laser_power_max
 8      01              Laser index: 1
 9      00 50           Power: 80%
11      XX XX           CRC16
```

---

## Frequency

### `laser_freq` (`0x15F0`)

**Required:** Yes

Set the PWM frequency for a laser tube in Hz. This is particularly important for RF-excited CO2 tubes.

**Arguments:**
| Byte Offset | Type | Description |
|-------------|------|-------------|
| 0 | int8 | Laser index |
| 1-4 | int32 | Frequency in Hz |

**Payload size:** 7 bytes (2 cmd + 1 index + 4 frequency)

Typical default: 20,000 Hz (20 kHz).

---

## Enable / Disable

### `laser_enable` (`0x15E1`)

**Required:** Yes

Prepare the laser subsystem for operation. This should activate any warm-up procedures, interlocks, or safety systems required before the laser can fire.

**Arguments:**
| Byte Offset | Type | Description |
|-------------|------|-------------|
| 0 | int8 | Laser index |

**Payload size:** 3 bytes (2 cmd + 1 index)

### `laser_disable` (`0x15E2`)

**Required:** Yes

Shut down the laser subsystem. This should deactivate the laser and any associated safety systems.

**Arguments:** Same format as `laser_enable`.

---

## On / Off

### `laser_on` (`0x15C2`) and `laser_off` (`0x15C1`)

**Required:** Yes

Control actual laser emission during cutting operations. These commands are sent rapidly during a job — `laser_on` before a cutting move, `laser_off` after.

**Arguments:**
| Byte Offset | Type | Description |
|-------------|------|-------------|
| 0 | int8 | Laser index (0 = layer default) |

**Payload size:** 3 bytes (2 cmd + 1 index)

### Enable vs On

The distinction between enable/disable and on/off is important:

- **`enable` / `disable`** — Prepare or shut down the laser subsystem. Called once at the start and end of a job (or layer).
- **`on` / `off`** — Control emission. Called many times during a job, surrounding each cutting move.

A typical sequence during a job:

```
laser_enable (index 1)
laser_power_max (index 1, 80%)
laser_power_min (index 1, 10%)
laser_freq (index 1, 20000 Hz)
  ... motion commands ...
  laser_on (index 1)
  move_rel_xy (cut path)
  laser_off (index 1)
  ... more motion and laser commands ...
laser_disable (index 1)
```

### Example: Laser On

```
Offset  Bytes           Description
------  --------------  ---------------------
 0      44 52 47 4E     DRGN header
 4      00 03           Payload size: 3 bytes
 6      15 C2           Command: laser_on
 8      01              Laser index: 1
 9      XX XX           CRC16
```

### Example: Laser Off

```
Offset  Bytes           Description
------  --------------  ---------------------
 0      44 52 47 4E     DRGN header
 4      00 03           Payload size: 3 bytes
 6      15 C1           Command: laser_off
 8      01              Laser index: 1
 9      XX XX           CRC16
```

---

## Laser Offset

### `laser_offset_xy` (`0x15B3`)

**Required:** No

Set the X/Y offset of a specific laser relative to the primary head position. Used on dual-head machines where the second laser is physically offset from the first.

**Arguments:**
| Byte Offset | Type | Description |
|-------------|------|-------------|
| 0 | int8 | Laser index |
| 1-4 | int32 | X offset in micrometers |
| 5-8 | int32 | Y offset in micrometers |

**Payload size:** 11 bytes (2 cmd + 1 index + 4 x + 4 y)

---

## Focus Z

### `focus_z` (`0x1F04`)

> **DRAFT** — This command is not yet finalized. The argument format may change.
