# Job Settings

Commands in the `0x5000` MSN category define per-job runtime parameters — speed, boundaries, and cut type. These are sent inside the job header (between `job_header_begin` and `job_header_end`) and apply only to the current job. They are **not** persisted to the controller's configuration.

---

## Command Summary

### Speed

| Command | Code | Arguments | Types & Units | Required |
|---------|------|-----------|---------------|----------|
| `speed_xy` | `0x5103` | speed | int32 um/sec | **Yes** |
| `speed_z` | `0x5104` | speed | int32 um/sec | No |
| `speed_u` | `0x5108` | speed | int32 um/sec | No |

### Boundaries

| Command | Code | Arguments | Types & Units | Required |
|---------|------|-----------|---------------|----------|
| `bounds_min_x` | `0x5201` | x | int32 um | **Yes** |
| `bounds_max_x` | `0x5301` | x | int32 um | **Yes** |
| `bounds_min_y` | `0x5202` | y | int32 um | **Yes** |
| `bounds_max_y` | `0x5302` | y | int32 um | **Yes** |
| `bounds_min_z` | `0x5204` | z | int32 um | No |
| `bounds_max_z` | `0x5304` | z | int32 um | No |
| `bounds_min_u` | `0x5208` | u | int32 um | No |
| `bounds_max_u` | `0x5308` | u | int32 um | No |

### Cut Type

| Command | Code | Arguments | Types & Units | Required |
|---------|------|-----------|---------------|----------|
| `cut_type` | `0x5DC7` | type | int16 flags | **Yes** |

---

## Speed

### `speed_xy` (`0x5103`)

**Required:** Yes

Set the XY cutting/engraving speed for the current layer. Sent before the layer's motion commands in the job header.

**Arguments:**
| Byte Offset | Type | Description |
|-------------|------|-------------|
| 0-3 | int32 | Speed in micrometers per second |

**Payload size:** 6 bytes (2 cmd + 4 speed)

For example, a speed of 100 mm/sec is sent as 100,000 um/sec.

### `speed_z` (`0x5104`) and `speed_u` (`0x5108`)

Same format as `speed_xy` but for the Z and U axes respectively. Optional — only needed if the job involves Z or U axis movement.

### Example: Set XY Speed to 200 mm/sec

```
Offset  Bytes           Description
------  --------------  ------------------------------------
 0      44 52 47 4E     DRGN header
 4      00 06           Payload size: 6 bytes
 6      51 03           Command: speed_xy
 8      00 03 0D 40     Speed: 200000 um/sec (200 mm/sec)
12      XX XX           CRC16
```

---

## Boundaries

Boundary commands define the bounding box of the current job. The controller may use these for:

- Soft limit checks (reject a job that exceeds the work area)
- Display on an LCD panel
- Pre-calculating acceleration profiles

Each boundary command sets one edge of the bounding box for one axis. The argument is a **signed 32-bit integer in micrometers**.

### Reading Command Codes

Boundary command codes follow a readable nibble pattern: `0x52xx` = min boundary, `0x53xx` = max boundary, and the LSN indicates the axis (X=`0x1`, Y=`0x2`, Z=`0x4`, U=`0x8`). For example, `0x5301` = max boundary, X axis.

This structure is a naming convention for readability. Always use the exact command codes listed in the table above.

### Example: Set X Minimum Boundary to 50 mm

```
Offset  Bytes           Description
------  --------------  ------------------------------------
 0      44 52 47 4E     DRGN header
 4      00 06           Payload size: 6 bytes
 6      52 01           Command: bounds_min_x
 8      00 00 C3 50     X min: 50000 um (50 mm)
12      XX XX           CRC16
```

---

## Cut Type

### `cut_type` (`0x5DC7`)

**Required:** Yes

Set the cutting mode for the current layer. The argument is a **16-bit flag value** indicating the type of cut.

**Arguments:**
| Byte Offset | Type | Description |
|-------------|------|-------------|
| 0-1 | int16 | Cut type flags |

**Payload size:** 4 bytes (2 cmd + 2 flags)

### Defined Values

| Value | Meaning |
|-------|---------|
| `0x0000` | Normal cut (vector) |
| `0x0101` | Unidirectional scan, X axis |
| `0x0102` | Unidirectional scan, Y axis |
| `0x0201` | Bidirectional scan, X axis |
| `0x0202` | Bidirectional scan, Y axis |

- **Normal cut** — The laser follows vector paths. Used for cutting and line engraving.
- **Unidirectional scan** — The laser scans in one direction only, returning without firing. Slower but more consistent for image engraving.
- **Bidirectional scan** — The laser fires in both scan directions. Faster but requires scan offset calibration for alignment.

### Example: Set Bidirectional X Scan

```
Offset  Bytes           Description
------  --------------  ------------------------------------
 0      44 52 47 4E     DRGN header
 4      00 04           Payload size: 4 bytes
 6      5D C7           Command: cut_type
 8      02 01           Value: bidirectional scan, X axis
10      XX XX           CRC16
```
