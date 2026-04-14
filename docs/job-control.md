# Job Control

Commands in the `0x0000` MSN category handle connection management, job lifecycle, and execution control. These are among the first commands a controller must support.

---

## Command Summary

| Command | Code | Arguments | Types & Units | Required |
|---------|------|-----------|---------------|----------|
| `handshake` | `0x01B8` | none | — | **Yes** |
| `stop` | `0x0CFF` | none | — | **Yes** |
| `pause` | `0x0C77` | none | — | **Yes** |
| `continue` | `0x0C88` | none | — | **Yes** |
| `get_state` | `0x857A` | none | — | **Yes** |
| `job_header_begin` | `0x078B` | none | — | **Yes** |
| `job_header_end` | `0x078E` | none | — | **Yes** |
| `job_body_begin` | `0x07BB` | none | — | **Yes** |
| `job_body_end` | `0x07BE` | none | — | **Yes** |
| `job_begin` | `0x070B` | none | — | **Yes** |
| `job_end` | `0x070E` | none | — | **Yes** |
| `frame_begin` | `0x0FAB` | none | — | No |
| `frame_end` | `0x0FAE` | none | — | No |

All commands in this category take no arguments. The payload is exactly 2 bytes (the command code only).

---

## Handshake

**Command:** `0x01B8`
**Required:** Yes

The first command sent when LightBurn connects to a controller. The controller should acknowledge that it is ready to receive commands.

> **DRAFT** — The response mechanism for handshake is not yet finalized. The controller's expected reply format is being defined and will be documented here once complete.

### Example

```
Offset  Bytes           Description
------  --------------  ---------------------
 0      44 52 47 4E     DRGN header
 4      00 02           Payload size: 2 bytes
 6      01 B8           Command: handshake
 8      XX XX           CRC16
```

---

## Stop / Pause / Continue

These three commands control job execution and are **safety-critical**.

### Stop (`0x0CFF`)

**Required:** Yes

Immediately halt all motion and laser output. The current job is aborted. This is the emergency stop command — the controller must respond to it as quickly as possible, regardless of what it is currently doing.

### Pause (`0x0C77`)

**Required:** Yes

Suspend the current operation. The controller should:
- Stop motion at a safe deceleration
- Turn off laser emission
- Retain its current position and job progress

The job can be resumed with `continue` or aborted with `stop`.

### Continue (`0x0C88`)

**Required:** Yes

Resume execution after a pause. The controller should pick up the job from where it was suspended.

### Examples

```
Offset  Bytes           Description
------  --------------  ---------------------
 0      44 52 47 4E     DRGN header
 4      00 02           Payload size: 2 bytes
 6      0C FF           Command: stop
 8      XX XX           CRC16
```

```
Offset  Bytes           Description
------  --------------  ---------------------
 0      44 52 47 4E     DRGN header
 4      00 02           Payload size: 2 bytes
 6      0C 77           Command: pause
 8      XX XX           CRC16
```

```
Offset  Bytes           Description
------  --------------  ---------------------
 0      44 52 47 4E     DRGN header
 4      00 02           Payload size: 2 bytes
 6      0C 88           Command: continue
 8      XX XX           CRC16
```

---

## Job Structure

A job in LBP is bracketed by header and body markers. This gives the controller a clear structure for the incoming command stream.

### Sequence

```
job_begin            (0x070B)
  job_header_begin   (0x078B)
    ... per-job settings: speeds, boundaries, laser config ...
  job_header_end     (0x078E)
  job_body_begin     (0x07BB)
    ... motion and laser commands (the actual cut/engrave) ...
  job_body_end       (0x07BE)
job_end              (0x070E)
```

### Job Begin / End (`0x070B` / `0x070E`)

`job_begin` and `job_end` bracket the entire job. The controller should treat these as the outermost delimiters — all job header and body content is nested between them.

### Job Header (`0x078B` / `0x078E`)

The job header contains per-job settings that apply to the entire job or to individual layers within it. Between `job_header_begin` and `job_header_end`, LightBurn sends commands such as:

- Speed settings (see [Settings](settings.md))
- Boundary definitions (see [Settings](settings.md))
- Laser power and frequency (see [Laser](laser.md))
- Cut type selection (see [Settings](settings.md))

The controller should accept and store these settings for use during the job body.

### Job Body (`0x07BB` / `0x07BE`)

The job body contains the actual cutting and engraving operations. Between `job_body_begin` and `job_body_end`, LightBurn sends:

- Movement commands (see [Movement](movement.md))
- Laser on/off commands (see [Laser](laser.md))
- Dwell commands (see [Movement](movement.md))
- Air assist on/off (see [Tools](tools.md))

The controller executes these commands in sequence, using the settings established in the job header.

---

## Framing

**Commands:** `frame_begin` (`0x0FAB`), `frame_end` (`0x0FAE`)
**Required:** No (recommended)

Framing is a preview operation where LightBurn traces the bounding box of a job without firing the laser. This lets the user verify the position and size of the job on their material before committing to a cut.

Between `frame_begin` and `frame_end`, LightBurn sends movement commands that trace the job boundary. The controller should execute these moves but must not fire the laser, regardless of any laser-on commands that may be included.

---

## Machine State Query

**Command:** `get_state` (`0x857A`)
**Required:** Yes

LightBurn periodically polls the controller for its current state using this command. The controller should respond with a set of status flags indicating what it is currently doing.

Note that `get_state` uses code `0x857A` — this places it in the State category (`0x8000`), consistent with other observable-state queries. See also [Machine State & Telemetry](state.md).

> **DRAFT** — The response format for `get_state` is being finalized. The following flag definitions are preliminary and may change.

### State Flags

The response is a 32-bit value composed of the following flags:

| Bit | Mask | Meaning |
|-----|------|---------|
| 0 | `0x00000001` | Moving — machine is in motion (job or user control) |
| 1 | `0x00000002` | Executing Job — machine is running a job |
| 2 | `0x00000004` | Executing Frame — machine is running a framing operation |
| 3 | `0x00000008` | Paused — machine is paused mid-job |
| 4 | `0x00000010` | Receiving — machine is receiving a file from LightBurn |
| 5 | `0x00000020` | File Loaded — a file is loaded and ready to execute |
| 6 | `0x00000040` | Computing — machine is performing a non-trivial computation |

A value of `0x00000000` indicates idle — the machine is powered on but not moving or executing.
