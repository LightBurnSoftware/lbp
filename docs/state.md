# Machine State & Telemetry

Commands in the `0x8000` MSN category let LightBurn query the controller's current state — axis positions, operational status, and usage telemetry.

---

## Command Summary

### Position Queries

| Command | Code | Arguments | Response | Required |
|---------|------|-----------|----------|----------|
| `pos_axis_x` | `0x8101` | none | int32 um | **Yes** |
| `pos_axis_y` | `0x8102` | none | int32 um | **Yes** |
| `pos_axis_z` | `0x8104` | none | int32 um | No |
| `pos_axis_u` | `0x8108` | none | int32 um | No |

### Telemetry (Optional)

| Command | Code | Arguments | Response | Required |
|---------|------|-----------|----------|----------|
| `total_on_time` | `0x8801` | none | uint32 seconds | No |
| `total_processing_time` | `0x8802` | none | uint32 seconds | No |
| `total_laser_on_time` | `0x8803` | laser_index (uint8) | uint32 seconds | No |
| `total_travel_x` | `0x8901` | none | uint32 meters | No |
| `total_travel_y` | `0x8902` | none | uint32 meters | No |
| `total_travel_z` | `0x8904` | none | uint32 meters | No |
| `total_travel_u` | `0x8908` | none | uint32 meters | No |

---

## Position Queries

LightBurn regularly polls the controller for current axis positions. These are displayed in the LightBurn UI and used for various calculations.

### `pos_axis_x` (`0x8101`) and `pos_axis_y` (`0x8102`)

**Required:** Yes

The controller must respond with the current position of the requested axis in **micrometers** as a signed 32-bit integer.

> **DRAFT** — The response format is being finalized. The expected behavior is that the controller sends back an LBP packet containing the position data. The exact framing of responses (same command code with data appended, or a dedicated response format) is TBD.

### Query Example

LightBurn sends:
```
Offset  Bytes           Description
------  --------------  ---------------------
 0      44 52 47 4E     DRGN header
 4      00 02           Payload size: 2 bytes
 6      81 01           Command: pos_axis_x
 8      XX XX           CRC16
```

> **DRAFT** — Expected response format TBD. The controller should return the current X position as an int32 in micrometers.

---

## Machine State

See [Job Control — Machine State Query](job-control.md#machine-state-query) for the `get_state` command (`0x857A`), which returns operational status flags.

---

## Telemetry

These optional commands allow LightBurn to display machine usage statistics. They are useful for maintenance tracking but are not required for basic operation.

### Time Counters

| Command | Response Type | Description |
|---------|---------------|-------------|
| `total_on_time` (`0x8801`) | uint32 seconds | Total time the machine has been powered on |
| `total_processing_time` (`0x8802`) | uint32 seconds | Total time spent executing jobs |
| `total_laser_on_time` (`0x8803`) | uint32 seconds | Total time the laser has been firing (per laser index) |

`total_laser_on_time` takes a uint8 laser index as an argument (unlike the other two, which take no arguments).

### Travel Counters

| Command | Response Type | Description |
|---------|---------------|-------------|
| `total_travel_x` (`0x8901`) | uint32 meters | Total distance traveled on X axis |
| `total_travel_y` (`0x8902`) | uint32 meters | Total distance traveled on Y axis |
| `total_travel_z` (`0x8904`) | uint32 meters | Total distance traveled on Z axis |
| `total_travel_u` (`0x8908`) | uint32 meters | Total distance traveled on U axis |

> **DRAFT** — Response format and exact return types for telemetry queries are being finalized.
