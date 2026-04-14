# Configuration

Commands in the `0xC000` MSN category manage persistent machine configuration — axis parameters, laser defaults, motion tuning, and more. Unlike [Job Settings](settings.md) which apply per-job, configuration values are stored in the controller's non-volatile memory and persist across power cycles.

---

## The Set-and-Commit Pattern

Configuration changes use a **staged commit** pattern:

1. LightBurn sends individual config commands to set values. The controller buffers these in memory.
2. Once all changes are sent, LightBurn sends `cfg_commit` (`0x0CCC`).
3. On commit, the controller writes all buffered values to persistent storage.

This ensures that configuration updates are atomic — either all changes are persisted, or none are (if the commit never arrives).

### Example Sequence

```
cfg_x_max_speed   (value: 500000 um/sec)
cfg_x_max_accel   (value: 8000000 um/sec^2)
cfg_y_max_speed   (value: 500000 um/sec)
cfg_y_max_accel   (value: 8000000 um/sec^2)
cfg_commit        ← persists all four values
```

---

## Argument Format

Unless otherwise noted, all configuration commands take a **single int32 (signed 32-bit big-endian integer)** as their argument.

**Payload size:** 6 bytes (2-byte command code + 4-byte argument)

---

## Per-Axis Configuration

Each axis (X, Y, Z, U) has an identical set of configuration parameters. The axis is encoded in the command code's LSN using the standard axis flags:

- X axis: flag `0x01` (command codes end in `1`)
- Y axis: flag `0x02` (command codes end in `2`)
- Z axis: flag `0x04` (command codes end in `4`)
- U axis: flag `0x08` (command codes end in `8`)

The table below uses X as the reference. For other axes, replace the last nibble of the command code with the corresponding axis flag.

| Command | Code (X) | Code (Y) | Code (Z) | Code (U) | Unit | Required |
|---------|----------|----------|----------|----------|------|----------|
| `cfg_*_settings` | `0xCA11` | `0xCA12` | `0xCA14` | `0xCA18` | binary flags | **Yes** |
| `cfg_*_step_length` | `0xCA21` | `0xCA22` | `0xCA24` | `0xCA28` | nanometers | **Yes** |
| `cfg_*_max_speed` | `0xCA31` | `0xCA32` | `0xCA34` | `0xCA38` | um/sec | **Yes** |
| `cfg_*_jumpoff_speed` | `0xCA41` | `0xCA42` | `0xCA44` | `0xCA48` | um/sec | **Yes** |
| `cfg_*_max_accel` | `0xCA51` | `0xCA52` | `0xCA54` | `0xCA58` | um/sec^2 | **Yes** |
| `cfg_*_breadth` | `0xCA61` | `0xCA62` | `0xCA64` | `0xCA68` | nanometers | **Yes** |
| `cfg_*_key_jumpoff_speed` | `0xCA71` | `0xCA72` | `0xCA74` | `0xCA78` | um/sec | No |
| `cfg_*_key_accel` | `0xCA81` | `0xCA82` | `0xCA84` | `0xCA88` | um/sec^2 | No |
| `cfg_*_estop_accel` | `0xCA91` | `0xCA92` | `0xCA94` | `0xCA98` | um/sec^2 | No |
| `cfg_*_home_offset` | `0xCAA1` | `0xCAA2` | `0xCAA4` | `0xCAA8` | um | No |
| `cfg_*_backlash` | `0xCAB1` | `0xCAB2` | `0xCAB4` | `0xCAB8` | um | No |

### Parameter Descriptions

- **settings** — Binary flags for axis configuration (e.g., direction inversion, enable state). The flag bit definitions are controller-specific.
- **step_length** — Distance per motor step in nanometers. Used to convert between logical positions and stepper pulses.
- **max_speed** — Maximum velocity for this axis in micrometers per second.
- **jumpoff_speed** — Starting velocity for acceleration ramps in micrometers per second.
- **max_accel** — Maximum acceleration in micrometers per second squared.
- **breadth** — Total travel length of this axis in nanometers (the work area dimension).
- **key_jumpoff_speed** — Starting velocity for manual jog moves (keyboard/panel control).
- **key_accel** — Acceleration for manual jog moves.
- **estop_accel** — Deceleration rate for emergency stops.
- **home_offset** — Offset from the home switch to the logical zero position, in micrometers.
- **backlash** — Backlash compensation distance in micrometers.

### Example: Set X Max Speed to 500 mm/sec

```
Offset  Bytes           Description
------  --------------  ------------------------------------
 0      44 52 47 4E     DRGN header
 4      00 06           Payload size: 6 bytes
 6      CA 31           Command: cfg_x_max_speed
 8      00 07 A1 20     Value: 500000 um/sec (500 mm/sec)
12      XX XX           CRC16
```

---

## Laser Configuration

| Command | Code | Unit | Required |
|---------|------|------|----------|
| `cfg_laser1_freq` | `0xC111` | Hz | **Yes** |
| `cfg_laser1_min_power` | `0xC121` | percent | **Yes** |
| `cfg_laser1_max_power` | `0xC131` | percent | **Yes** |
| `cfg_laser1_preig_freq` | `0xC141` | Hz | No |
| `cfg_laser1_preig_pct` | `0xC151` | percent * 10 | No |
| `cfg_laser2_freq` | `0xC112` | Hz | No |
| `cfg_laser2_min_power` | `0xC122` | percent | No |
| `cfg_laser2_max_power` | `0xC132` | percent | No |
| `cfg_laser2_preig_freq` | `0xC142` | Hz | No |
| `cfg_laser2_preig_pct` | `0xC152` | percent * 10 | No |
| `cfg_head_dist` | `0xC01E` | nanometers | No |

- **freq** — Default PWM frequency for the laser tube. Typical: 20,000 Hz.
- **min_power / max_power** — Default power range as a percentage (0-100).
- **preig_freq / preig_pct** — Pre-ignition frequency and power for RF tubes. Pre-ignition power is expressed as percent * 10 (e.g., 50 = 5.0%).
- **head_dist** — Distance between dual laser heads in nanometers.

Laser 2 parameters are only needed for dual-tube machines.

---

## Cut and Motion Configuration

| Command | Code | Unit | Required |
|---------|------|------|----------|
| `cfg_idle_speed` | `0xC201` | um/sec | **Yes** |
| `cfg_idle_acc` | `0xC202` | um/sec^2 | **Yes** |
| `cfg_idle_delay` | `0xC203` | microseconds | No |
| `cfg_start_speed` | `0xC204` | um/sec | **Yes** |
| `cfg_min_acc` | `0xC205` | um/sec^2 | **Yes** |
| `cfg_max_acc` | `0xC206` | um/sec^2 | **Yes** |
| `cfg_acc_factor_pct` | `0xC207` | percent | No |
| `cfg_G0_acc_factor_pct` | `0xC208` | percent | No |
| `cfg_speed_factor_pct` | `0xC209` | percent | No |

- **idle_speed / idle_acc** — Speed and acceleration for non-cutting (traverse) moves.
- **idle_delay** — Delay after a traverse move before starting a cut.
- **start_speed** — Minimum speed at the start of acceleration ramps.
- **min_acc / max_acc** — Acceleration range for cutting moves.
- **acc_factor_pct** — Scaling factor for acceleration (percent).
- **G0_acc_factor_pct** — Scaling factor for rapid (G0) move acceleration.
- **speed_factor_pct** — Global speed scaling factor.

---

## Engrave Configuration

| Command | Code | Unit |
|---------|------|------|
| `cfg_engrave_x_start_speed` | `0xCE51` | um/sec |
| `cfg_engrave_y_start_speed` | `0xCE52` | um/sec |
| `cfg_engrave_x_acc` | `0xCEA1` | um/sec^2 |
| `cfg_engrave_y_acc` | `0xCEA2` | um/sec^2 |
| `cfg_line_shift_speed` | `0xCE01` | um/sec |
| `cfg_facula_size_pct` | `0xCE02` | percent * 10 |
| `cfg_engrave_factor_pct` | `0xCE03` | percent |

- **engrave_*_start_speed** — Starting velocity for engraving scan lines.
- **engrave_*_acc** — Acceleration during engraving scans.
- **line_shift_speed** — Speed for moving between scan lines.
- **facula_size_pct** — Laser spot size compensation, expressed as percent * 10.
- **engrave_factor_pct** — Engraving speed scaling factor.

---

## Homing Configuration

| Command | Code | Unit |
|---------|------|------|
| `cfg_xy_home_speed` | `0xCAC3` | um/sec |
| `cfg_z_home_speed` | `0xCAC4` | um/sec |
| `cfg_z_work_speed` | `0xCAD4` | um/sec |
| `cfg_u_home_speed` | `0xCAC8` | um/sec |
| `cfg_u_work_speed` | `0xCAD8` | um/sec |

- **xy_home_speed** — Speed for X and Y homing sequences.
- **z_home_speed / u_home_speed** — Speed for Z and U axis homing.
- **z_work_speed / u_work_speed** — Speed for Z and U axis work moves (non-homing).

---

## Focus, Material, and Return Location

| Command | Code | Unit | Description |
|---------|------|------|-------------|
| `cfg_material_thick` | `0xC301` | um | Material thickness |
| `cfg_focus_distance` | `0xC211` | um | Focal length of the lens |
| `cfg_return_location` | `0xC212` | flags | Where to move after job completes |

### Return Location Flags

| Value | Meaning |
|-------|---------|
| `0x0000` | Return to origin |
| `0x8000` | Return to absolute origin |
| `0x4000` | No return (stay in place) |

---

## Docking Position

| Command | Code | Unit |
|---------|------|------|
| `cfg_docking_position_x` | `0xCD01` | um |
| `cfg_docking_position_y` | `0xCD02` | um |
| `cfg_docking_position_z` | `0xCD04` | um |

The docking position is where the laser head parks when not in use.

---

## Delays

| Command | Code | Unit |
|---------|------|------|
| `cfg_reset_delay` | `0xC213` | milliseconds |
| `cfg_status_on_delay` | `0xC214` | milliseconds |
| `cfg_status_off_delay` | `0xC215` | milliseconds |
| `cfg_finish_delay` | `0xC216` | milliseconds |

- **reset_delay** — Delay after a reset before accepting commands.
- **status_on_delay / status_off_delay** — Delays for status indicator transitions.
- **finish_delay** — Delay after job completion before returning to idle.

---

## Feeder Configuration

| Command | Code | Unit |
|---------|------|------|
| `cfg_feed_flags` | `0xC217` | flags |
| `cfg_feed_pre_delay` | `0xC218` | milliseconds |
| `cfg_feed_post_delay` | `0xC219` | milliseconds |
| `cfg_feed_backlash` | `0xC21A` | um |

### Feed Flags

| Flag | Meaning |
|------|---------|
| `0x2000` | Last feeding off |
| `0x0800` | Progressive feed on |

---

## Rotary Configuration

| Command | Code | Unit |
|---------|------|------|
| `cfg_rotary_enable` | `0xC221` | 0 or 1 |
| `cfg_rotary_pulses_per_rotation` | `0xC222` | steps * 1000 |
| `cfg_rotary_diameter` | `0xC223` | um |

- **rotary_enable** — 0 = disabled, 1 = enabled. Value 4 = wireless panel speed shift enable.
- **rotary_pulses_per_rotation** — Motor steps per full rotation, multiplied by 1000.
- **rotary_diameter** — Diameter of the rotary workpiece in micrometers.

---

## Wireless Panel

| Command | Code | Unit |
|---------|------|------|
| `cfg_wireless_panel_fast` | `0xC224` | um/sec |
| `cfg_wireless_panel_slow` | `0xC225` | um/sec |

Speeds for a wireless control panel's fast and slow jog modes.

---

## Auto Layout and Auto Home

| Command | Code | Description |
|---------|------|-------------|
| `cfg_autolayout` | `0xC401` | Auto layout flags |
| `cfg_axis_auto_home` | `0xC402` | Auto home on startup flags |

---

## User Origin

| Command | Code | Unit |
|---------|------|------|
| `cfg_user_origin_x` | `0xC061` | um |
| `cfg_user_origin_y` | `0xC062` | um |

The user-defined origin point. Coordinates are in micrometers.

---

## Commit

### `cfg_commit` (`0x0CCC`)

**Required:** Yes

Persist all buffered configuration changes to non-volatile storage. This command takes no arguments.

```
Offset  Bytes           Description
------  --------------  ---------------------
 0      44 52 47 4E     DRGN header
 4      00 02           Payload size: 2 bytes
 6      0C CC           Command: cfg_commit
 8      XX XX           CRC16
```

The controller must not write configuration to persistent storage until this command is received. This ensures atomic updates — either all changes from a configuration session are persisted, or none are.
