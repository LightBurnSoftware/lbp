# LBP Vendor Documentation — Implementation Plan

> **For agentic workers:** Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create the complete set of vendor-facing LBP documentation as described in `docs/spec-design.md`.

**Architecture:** Markdown files in `docs/`, one per MSN command category, plus an introduction, index, and self-contained HTML preview page. All content is derived from the protocol definitions in `lbp/include/lbp/spec.h`. No proprietary internals — only what a vendor firmware engineer needs.

**Tech Stack:** Markdown, HTML/CSS/JS (preview page uses marked.js + highlight.js from CDN)

**Source files to reference:**
- `lbp/include/lbp/spec.h` — all command codes, constants, flags, MSN categories
- `lbp/include/lbp/checksum.h` — CRC16 algorithm reference
- `lbp/include/lbp/message.h` — message construction patterns
- `lbp/include/lbp/parser.h` — parser state machine (HeaderSync -> Size -> Data -> Checksum)
- `docs/spec-design.md` — approved design spec with conventions, structure, and quick reference

---

### Task 1: Introduction Page

**Files:**
- Create: `docs/introduction.md`

This is the non-technical overview for sales/management. No command codes or hex dumps. Explain what LBP is, why it exists, and what it enables.

- [ ] **Step 1: Write `docs/introduction.md`**

Content must cover:
- What is the LightBurn Protocol (LBP)? A standardized binary communication protocol that lets laser hardware talk to LightBurn software.
- Why does it exist? So laser manufacturers have a clear, documented path to LightBurn compatibility. Instead of reverse-engineering or custom integrations, they implement one spec.
- What does it enable? Any manufacturer can build LBP support into their controller firmware and achieve LightBurn compatibility.
- How does it work at a high level? LightBurn sends commands (move, fire laser, configure) as structured binary packets over a serial/USB/TCP connection. The controller executes them and reports state back. No mention of specific hex codes — keep it conceptual.
- Transport flexibility: works over any byte-stream transport the manufacturer prefers.
- Extensibility: vendors can define custom commands for hardware-specific features (0xD000 range — but don't say the hex code, just say "a reserved command range").
- What's in the rest of the docs: brief description of each spec page for technical readers.

Tone: Professional, concise, welcoming. This is the first thing a prospective partner reads.

- [ ] **Step 2: Verify the page renders correctly in preview**

Open `docs/preview.html` in a browser (built in Task 12). Confirm introduction.md renders with proper headings, paragraphs, no broken markdown.

- [ ] **Step 3: Commit**

```bash
git add docs/introduction.md
git commit -m "docs: add LBP introduction page for non-technical audience"
```

---

### Task 2: Framing Page

**Files:**
- Create: `docs/framing.md`

**Reference:** `lbp/include/lbp/spec.h` (size constants), `lbp/include/lbp/checksum.h` (CRC algorithm), `lbp/include/lbp/parser.h` (parser states).

This is the foundational spec page. Every other page depends on understanding the wire format.

- [ ] **Step 1: Write `docs/framing.md`**

Content must cover:

**Message structure diagram:**
```
+----------------+--------+---------+-------+
| Header (4B)    | Size   | Payload | CRC16 |
| 0x4452474E     | (2B)   | (var)   | (2B)  |
+----------------+--------+---------+-------+
```

**Header:** The constant `0x4452474E` ("DRGN" in ASCII). Every message starts with these 4 bytes. Used for synchronization — parser scans byte stream until it finds this pattern.

**Size field:** 2 bytes, big-endian. Represents the size of the Payload section only (does not include the header, size field, or CRC). Minimum value: 2 (a command code with no arguments). Maximum value: 504 (for file chunk messages).

**Payload:** Starts with a 2-byte command code, followed by zero or more argument bytes. The command code determines how many argument bytes follow and how to interpret them. See individual command pages for argument formats.

**CRC16:** CRC16-CCITT appended after the payload.
- Polynomial: 0x1021 (reversed: 0x8408)
- Initial value: 0xFFFF
- Computed over the payload bytes only (not the header or size field)
- 2 bytes, big-endian

**Byte order:** All multi-byte integers in LBP are big-endian (network byte order). This applies to the size field, command codes, all arguments, and the CRC.

**Message sizes:**
- Minimum message: 10 bytes (4 header + 2 size + 2 command + 2 CRC)
- Maximum command message: 26 bytes (10 + 16 bytes of arguments)
- File chunk message: 512 bytes (fixed size for bulk transfers)

**Parsing:** Describe the state machine: HeaderSync (scan for 0x4452474E) -> Size (read 2 bytes) -> Data (read `size` bytes of payload) -> Checksum (read 2 bytes, verify CRC). On CRC failure, discard and return to HeaderSync.

**Complete annotated example** — a `move_abs_xy` command moving to (10mm, 12mm):
```
44 52 47 4E   DRGN header
00 0A         payload size: 10 bytes
6A 03         command: move_abs_xy (0x6A03)
00 00 27 10   x: 10000 micrometers (10 mm)
00 00 2E E0   y: 12000 micrometers (12 mm)
XX XX         CRC16 (computed over the 10 payload bytes)
```

**Simplest possible example** — a `stop` command with no arguments:
```
44 52 47 4E   DRGN header
00 02         payload size: 2 bytes
0C FF         command: stop (0x0CFF)
XX XX         CRC16
```

- [ ] **Step 2: Commit**

```bash
git add docs/framing.md
git commit -m "docs: add LBP wire format and framing spec"
```

---

### Task 3: Job Control Page

**Files:**
- Create: `docs/job-control.md`

**Reference:** `spec.h` — handshake, get_state, job begin/end, job header/body, stop/pause/continue, frame.

- [ ] **Step 1: Write `docs/job-control.md`**

**Command summary table:**

| Command | Code | Arguments | Types & Units | Required |
|---------|------|-----------|---------------|----------|
| `handshake` | `0x01B8` | none | — | **Yes** |
| `stop` | `0x0CFF` | none | — | **Yes** |
| `pause` | `0x0C77` | none | — | **Yes** |
| `continue` | `0x0C88` | none | — | **Yes** |
| `get_state` | `0x857A` | none | — | **Yes** |
| `job_begin` | `0x070B` | none | — | **Yes** |
| `job_end` | `0x070E` | none | — | **Yes** |
| `job_header_begin` | `0x078B` | none | — | **Yes** |
| `job_header_end` | `0x078E` | none | — | **Yes** |
| `job_body_begin` | `0x07BB` | none | — | **Yes** |
| `job_body_end` | `0x07BE` | none | — | **Yes** |
| `frame_begin` | `0x0FAB` | none | — | No |
| `frame_end` | `0x0FAE` | none | — | No |

**Sections to write:**

- **Handshake (`0x01B8`):** First command sent to establish communication. Explain what the controller should do when it receives this (acknowledge connection readiness). Mark response mechanism as DRAFT — waiting on engineer input for how the controller responds.

- **Stop / Pause / Continue:** Safety-critical commands. `stop` (`0x0CFF`) must immediately halt all motion and laser output. `pause` (`0x0C77`) suspends the current operation. `continue` (`0x0C88`) resumes after a pause. These must be implemented for any LBP-compatible controller.

- **Job structure:** A job is bracketed by header/body begin/end markers:
  ```
  job_header_begin  (0x078B)
    ... settings commands (speeds, boundaries, laser config) ...
  job_header_end    (0x078E)
  job_body_begin    (0x07BB)
    ... motion and laser commands ...
  job_body_end      (0x07BE)
  ```
  Explain that header contains per-job settings and body contains the actual cut/engrave commands.

- **Job begin/end (`0x070B` / `0x070E`):** Outermost delimiters for a complete job. All job header and body content is nested between these.

- **Framing (`0x0FAB` / `0x0FAE`):** Used for "frame" operations where LightBurn traces the bounding box of a job without firing the laser. Optional but recommended.

- **Machine state (`0x857A`):** DRAFT section. LightBurn polls this to understand what the machine is doing. Reference the state flags from `spec.h` but mark the bit definitions as draft. Include the known flags:
  - Bit 0 (`0x0001`): Moving
  - Bit 1 (`0x0002`): Executing Job
  - Bit 2 (`0x0004`): Executing Frame
  - Bit 3 (`0x0008`): Paused
  - Bit 4 (`0x0010`): Receiving
  - Bit 5 (`0x0020`): File Loaded
  - Bit 6 (`0x0040`): Computing

- Hex example for each required command (these are all no-argument commands, so each is just header + size(2) + cmd + CRC).

- [ ] **Step 2: Commit**

```bash
git add docs/job-control.md
git commit -m "docs: add job control spec (handshake, stop/pause, job structure)"
```

---

### Task 4: Movement Page

**Files:**
- Create: `docs/movement.md`

**Reference:** `spec.h` (movement commands, jog commands, dwell, home).

- [ ] **Step 1: Write `docs/movement.md`**

**Command summary table:**

| Command | Code | Arguments | Types & Units | Required |
|---------|------|-----------|---------------|----------|
| `move_abs_x` | `0x6A01` | x | int32 um | **Yes** |
| `move_abs_y` | `0x6A02` | y | int32 um | **Yes** |
| `move_abs_xy` | `0x6A03` | x, y | int32 um, int32 um | **Yes** |
| `move_abs_z` | `0x6A04` | z | int32 um | No |
| `move_abs_u` | `0x6A08` | u | int32 um | No |
| `move_abs_xyz` | `0x6A07` | x, y, z | int32 um x3 | No |
| `move_abs_xyzu` | `0x6A0F` | x, y, z, u | int32 um x4 | No |
| `move_rel_x` | `0x6101` | x | int32 um | **Yes** |
| `move_rel_y` | `0x6102` | y | int32 um | **Yes** |
| `move_rel_xy` | `0x6103` | x, y | int32 um, int32 um | **Yes** |
| `move_rel_z` | `0x6104` | z | int32 um | No |
| `move_rel_u` | `0x6108` | u | int32 um | No |
| `move_rel_xyz` | `0x6107` | x, y, z | int32 um x3 | No |
| `move_rel_xyzu` | `0x610F` | x, y, z, u | int32 um x4 | No |
| `home_xy` | `0x6803` | none | — | **Yes** |
| `home_x` | `0x6801` | none | — | No |
| `home_y` | `0x6802` | none | — | No |
| `home_z` | `0x6804` | none | — | No |
| `home_u` | `0x6808` | none | — | No |
| `home_xyz` | `0x6807` | none | — | No |
| `home_xyzu` | `0x680F` | none | — | No |
| `dwell` | `0x602B` | duration | int32 microseconds | **Yes** |
| `jog_x_pos_start` | `0x6301` | none | — | Recommended |
| `jog_x_pos_stop` | `0x6201` | none | — | Recommended |
| `jog_x_neg_start` | `0x6501` | none | — | Recommended |
| `jog_x_neg_stop` | `0x6401` | none | — | Recommended |
| (same pattern for y, z, u) | | | | Recommended |

**Sections to write:**

- **Command composition:** Explain how movement commands are composed from flags. `flag_move (0x6000)` + direction flags (`0x0A00` abs / `0x0100` rel) + axis flags (x=0x01, y=0x02, z=0x04, u=0x08). This means a firmware engineer can decode movement commands by inspecting bits rather than needing a lookup table for every combination.

- **Absolute moves:** Move to a specific position in machine coordinates. Arguments are signed int32 values in micrometers. Multi-axis moves pack arguments in axis order: x, then y, then z, then u. Only the axes indicated by the flags are present.

- **Relative moves:** Same format as absolute, but positions are relative to current location.

- **Homing:** No arguments. Controller should execute its homing sequence for the indicated axes. `home_xy` is the most common — LightBurn sends this at startup and when the user clicks "Home."

- **Dwell:** Pause in place for the specified number of microseconds. Max value: 60,000,000 (60 seconds). Used between operations that need settling time.

- **Jogging:** Start/stop continuous motion in a direction. Used for manual positioning via the LightBurn UI. `jog_x_pos_start` begins moving X in the positive direction; `jog_x_pos_stop` stops it. The controller determines jog speed (or uses configured key jog speed). Strongly recommended for a good user experience.

- Hex-annotated examples for: `move_abs_xy`, `move_rel_x`, `home_xy`, `dwell`, and one jog start/stop pair.

- [ ] **Step 2: Commit**

```bash
git add docs/movement.md
git commit -m "docs: add movement command spec (moves, homing, jog, dwell)"
```

---

### Task 5: Laser Page

**Files:**
- Create: `docs/laser.md`

**Reference:** `spec.h` (laser commands).

- [ ] **Step 1: Write `docs/laser.md`**

**Command summary table:**

| Command | Code | Arguments | Types & Units | Required |
|---------|------|-----------|---------------|----------|
| `laser_power_min` | `0x15A0` | laser_index, power | uint8, int16 percent | **Yes** |
| `laser_power_max` | `0x15A1` | laser_index, power | uint8, int16 percent | **Yes** |
| `laser_freq` | `0x15F0` | laser_index, freq | uint8, int32 Hz | **Yes** |
| `laser_enable` | `0x15E1` | laser_index | uint8 | **Yes** |
| `laser_disable` | `0x15E2` | laser_index | uint8 | **Yes** |
| `laser_on` | `0x15C2` | laser_index | uint8 | **Yes** |
| `laser_off` | `0x15C1` | laser_index | uint8 | **Yes** |
| `laser_offset_xy` | `0x15B3` | laser_index, x, y | uint8, int32 um, int32 um | No |
| `focus_z` | `0x1F04` | (see below) | — | No |

**Sections to write:**

- **Laser index:** The first argument to most laser commands is a uint8 laser index. Index 0 means "layer default" for on/off commands. Index 1 = first laser tube, index 2 = second laser tube. Most machines have one laser (index 1). Dual-tube machines use indices 1 and 2.

- **Power control:** `laser_power_min` and `laser_power_max` set the power range. Power is expressed as an int16 percentage (0-100). These are per-layer settings sent in the job header.

- **Frequency:** `laser_freq` sets the PWM frequency in Hz. Typical default is 20000 Hz (20 kHz). Important for RF-excited CO2 tubes.

- **Enable/Disable vs On/Off:** `enable`/`disable` prepare the laser subsystem (warm-up, interlocks). `on`/`off` control actual emission during cutting. A typical sequence: enable -> set power -> on -> [cutting moves] -> off -> disable.

- **Laser offset:** Optional. For dual-head machines, sets the X/Y offset of a specific laser relative to the primary. Arguments: laser index (uint8), x offset (int32 um), y offset (int32 um).

- **Focus Z:** DRAFT — mark as not yet finalized.

- Hex examples for: power_max, laser_on, laser_off.

- [ ] **Step 2: Commit**

```bash
git add docs/laser.md
git commit -m "docs: add laser command spec (power, frequency, enable, on/off)"
```

---

### Task 6: Settings Page

**Files:**
- Create: `docs/settings.md`

**Reference:** `spec.h` (settings commands — speed, bounds, cut type).

- [ ] **Step 1: Write `docs/settings.md`**

**Command summary table:**

| Command | Code | Arguments | Types & Units | Required |
|---------|------|-----------|---------------|----------|
| `speed_xy` | `0x5103` | speed | int32 um/sec | **Yes** |
| `speed_z` | `0x5104` | speed | int32 um/sec | No |
| `speed_u` | `0x5108` | speed | int32 um/sec | No |
| `bounds_min_x` | `0x5201` | x | int32 um | **Yes** |
| `bounds_max_x` | `0x5301` | x | int32 um | **Yes** |
| `bounds_min_y` | `0x5202` | y | int32 um | **Yes** |
| `bounds_max_y` | `0x5302` | y | int32 um | **Yes** |
| `bounds_min_z` | `0x5204` | z | int32 um | No |
| `bounds_max_z` | `0x5304` | z | int32 um | No |
| `bounds_min_u` | `0x5208` | u | int32 um | No |
| `bounds_max_u` | `0x5308` | u | int32 um | No |
| `cut_type` | `0x51C7` | type | int16 flags | **Yes** |

**Sections to write:**

- **Context:** Settings are per-job values sent inside the job header (between `job_header_begin` and `job_header_end`). They are not persisted — they apply only to the current job.

- **Speed:** XY cutting speed for the current layer. Sent before the layer's motion commands. Single int32 in micrometers/sec.

- **Boundaries:** Define the bounding box of the current job. The controller may use these for soft limit checks or display purposes. One int32 per axis per command.

- **Cut type:** Flags indicating the cutting mode. Explain the defined values:
  - `0x0000` — Normal cut (vector)
  - `0x0101` — Unidirectional scan, X axis
  - `0x0102` — Unidirectional scan, Y axis
  - `0x0201` — Bidirectional scan, X axis
  - `0x0202` — Bidirectional scan, Y axis

- Hex examples for: speed_xy, bounds_min_x, cut_type.

- [ ] **Step 2: Commit**

```bash
git add docs/settings.md
git commit -m "docs: add per-job settings spec (speed, bounds, cut type)"
```

---

### Task 7: State Page

**Files:**
- Create: `docs/state.md`

**Reference:** `spec.h` (state flags, position, telemetry).

- [ ] **Step 1: Write `docs/state.md`**

**Command summary table:**

| Command | Code | Arguments | Types & Units | Required |
|---------|------|-----------|---------------|----------|
| `pos_axis_x` | `0x8101` | (query) | response: int32 um | **Yes** |
| `pos_axis_y` | `0x8102` | (query) | response: int32 um | **Yes** |
| `pos_axis_z` | `0x8104` | (query) | response: int32 um | No |
| `pos_axis_u` | `0x8108` | (query) | response: int32 um | No |
| `total_on_time` | `0x8801` | (query) | response: uint32 seconds | No |
| `total_processing_time` | `0x8802` | (query) | response: uint32 seconds | No |
| `total_laser_on_time` | `0x8803` | laser_index | response: uint32 seconds | No |
| `total_travel_x` | `0x8901` | (query) | response: uint32 meters | No |
| `total_travel_y` | `0x8902` | (query) | response: uint32 meters | No |
| `total_travel_z` | `0x8904` | (query) | response: uint32 meters | No |
| `total_travel_u` | `0x8908` | (query) | response: uint32 meters | No |

**Sections to write:**

- **Position queries:** LightBurn regularly polls the controller for current axis positions. The controller must respond with the current position in micrometers. X and Y are required for the position display in LightBurn's UI.

- **DRAFT: Response format** — Mark prominently as draft. Note that the response mechanism is being finalized. State the expected behavior: the controller sends back an LBP packet containing the requested data. Exact framing of responses is TBD.

- **Machine state flags (`get_state`):** DRAFT section. Reference the flag bits from `spec.h`:
  - Bit 0 (`0x0001`): Moving
  - Bit 1 (`0x0002`): Executing Job
  - Bit 2 (`0x0004`): Executing Frame
  - Bit 3 (`0x0008`): Paused
  - Bit 4 (`0x0010`): Receiving
  - Bit 5 (`0x0020`): File Loaded
  - Bit 6 (`0x0040`): Computing

- **Telemetry (optional):** Total on-time, processing time, laser-on time, and travel distance per axis. These are "nice to have" for machine maintenance tracking. All marked as optional and several are DRAFT.

- [ ] **Step 2: Commit**

```bash
git add docs/state.md
git commit -m "docs: add state and telemetry spec (position, machine state, counters)"
```

---

### Task 8: Configuration Page

**Files:**
- Create: `docs/configuration.md`

**Reference:** `spec.h` (all cfg_* commands).

- [ ] **Step 1: Write `docs/configuration.md`**

This is the largest page. Organize into sub-sections by category.

**Key concept — set-and-commit pattern:** Configuration changes are staged, not immediate. LightBurn sends individual config commands to set values, then sends `cfg_commit` (`0x0CCC`) to persist them all at once. The controller should buffer incoming config values and only write them to persistent storage on commit.

**Sub-sections with their command tables:**

**Per-axis configuration** (X, Y, Z, U — same structure per axis, use X as the template and note the axis flag substitution pattern):

| Command | Code (X axis) | Argument | Unit | Required |
|---------|---------------|----------|------|----------|
| `cfg_x_settings` | `0xCA11` | flags | binary flags | **Yes** |
| `cfg_x_step_length` | `0xCA21` | length | nanometers | **Yes** |
| `cfg_x_max_speed` | `0xCA31` | speed | um/sec | **Yes** |
| `cfg_x_jumpoff_speed` | `0xCA41` | speed | um/sec^2 | **Yes** |
| `cfg_x_max_accel` | `0xCA51` | accel | um/sec^2 | **Yes** |
| `cfg_x_breadth` | `0xCA61` | width | nanometers | **Yes** |
| `cfg_x_key_jumpoff_speed` | `0xCA71` | speed | um/sec | No |
| `cfg_x_key_accel` | `0xCA81` | accel | um/sec^2 | No |
| `cfg_x_estop_accel` | `0xCA91` | accel | um/sec^2 | No |
| `cfg_x_home_offset` | `0xCAA1` | offset | um | No |
| `cfg_x_backlash` | `0xCAB1` | distance | um | No |

Note: Y axis uses flag 0x02 (codes end in 2), Z uses 0x04 (codes end in 4), U uses 0x08 (codes end in 8).

**Laser configuration:**

| Command | Code | Argument | Unit | Required |
|---------|------|----------|------|----------|
| `cfg_laser1_freq` | `0xC111` | frequency | Hz | **Yes** |
| `cfg_laser1_min_power` | `0xC121` | power | percent | **Yes** |
| `cfg_laser1_max_power` | `0xC131` | power | percent | **Yes** |
| `cfg_laser1_preig_freq` | `0xC141` | frequency | Hz | No |
| `cfg_laser1_preig_pct` | `0xC151` | power | percent * 10 | No |
| `cfg_laser2_*` | (same pattern, 2 suffix) | — | — | No |
| `cfg_head_dist` | `0xC01E` | distance | nanometers | No |

**Cut and motion configuration:**

| Command | Code | Argument | Unit | Required |
|---------|------|----------|------|----------|
| `cfg_idle_speed` | `0xC201` | speed | um/sec | **Yes** |
| `cfg_idle_acc` | `0xC202` | accel | um/sec^2 | **Yes** |
| `cfg_idle_delay` | `0xC203` | delay | microseconds | No |
| `cfg_start_speed` | `0xC204` | speed | um/sec | **Yes** |
| `cfg_min_acc` | `0xC205` | accel | um/sec^2 | **Yes** |
| `cfg_max_acc` | `0xC206` | accel | um/sec^2 | **Yes** |
| `cfg_acc_factor_pct` | `0xC207` | factor | percent | No |
| `cfg_G0_acc_factor_pct` | `0xC208` | factor | percent | No |
| `cfg_speed_factor_pct` | `0xC209` | factor | percent | No |
| `cfg_material_thick` | `0xC301` | thickness | um | No |
| `cfg_focus_distance` | `0xC211` | distance | um | No |
| `cfg_return_location` | `0xC212` | flags | see below | No |

**Engrave configuration:**

| Command | Code | Argument | Unit |
|---------|------|----------|------|
| `cfg_engrave_x_start_speed` | `0xCE51` | speed | um/sec |
| `cfg_engrave_y_start_speed` | `0xCE52` | speed | um/sec |
| `cfg_engrave_x_acc` | `0xCEA1` | accel | um/sec^2 |
| `cfg_engrave_y_acc` | `0xCEA2` | accel | um/sec^2 |
| `cfg_line_shift_speed` | `0xCE01` | speed | um/sec |
| `cfg_facula_size_pct` | `0xCE02` | size | percent * 10 |
| `cfg_engrave_factor_pct` | `0xCE03` | factor | percent |

**Homing configuration:**

| Command | Code | Argument | Unit |
|---------|------|----------|------|
| `cfg_xy_home_speed` | `0xCAC3` | speed | um/sec |
| `cfg_z_home_speed` | `0xCAC4` | speed | um/sec |
| `cfg_z_work_speed` | `0xCAD4` | speed | um/sec |
| `cfg_u_home_speed` | `0xCAC8` | speed | um/sec |
| `cfg_u_work_speed` | `0xCAD8` | speed | um/sec |

**Docking, rotary, wireless panel, feeder, user origin, delays, auto-home, autolayout, return location, commit** — include tables for all remaining config commands from `spec.h`. All are optional except `cfg_commit`.

| Command | Code | Argument | Unit | Required |
|---------|------|----------|------|----------|
| `cfg_commit` | `0x0CCC` | none | — | **Yes** |

**Explain the commit pattern** with a worked example showing a sequence of config commands followed by commit.

- All config command arguments are a single int32 unless otherwise noted.

- [ ] **Step 2: Commit**

```bash
git add docs/configuration.md
git commit -m "docs: add configuration spec (axis, laser, cut, engrave, commit pattern)"
```

---

### Task 9: Files Page

**Files:**
- Create: `docs/files.md`

**Reference:** `spec.h` (file commands).

- [ ] **Step 1: Write `docs/files.md`**

**Command summary table:**

| Command | Code | Arguments | Types & Units | Required |
|---------|------|-----------|---------------|----------|
| `begin_file` | `0x4404` | file_size | int32 bytes | No |
| `end_file` | `0x4405` | none | — | No |
| `file_chunk` | `0x44FC` | data | raw bytes (up to 498B) | No |
| `execute` | `0x0C66` | (TBD) | — | No |
| `get_filename` | `0x4401` | file_index | int16 | DRAFT |
| `set_filename` | `0x4402` | file_index | int16 | DRAFT |
| `delete_file` | `0x4403` | file_index | int16 | DRAFT |
| `new_file` | `0x4406` | (TBD) | — | DRAFT |
| `file_count` | `0x4407` | (TBD) | — | DRAFT |
| `file_time` | `0x4408` | (TBD) | — | DRAFT |
| `calc_file_time` | `0x4409` | file_index | int16 | DRAFT |
| `flash_available` | `0x45FA` | (query) | response: int64 bytes | DRAFT |
| `mainboard_version` | `0x45B0` | (query) | — | DRAFT |

**Sections to write:**

- **File upload sequence:** `begin_file` (with total size) -> repeated `file_chunk` commands -> `end_file`. Each chunk payload is up to 498 bytes (512-byte message minus framing). The controller buffers chunks and assembles the complete file.

- **File execution:** `execute` (`0x0C66`) runs a previously uploaded file. Note: this command is in the Fundamentals category, not Files. DRAFT — argument format TBD.

- **File management:** get/set filename, delete, count — all DRAFT.

- **Flash/version queries:** DRAFT.

- Mark this entire page as optional for minimum viable support. File transfer is an advanced feature — basic LBP support streams commands in real-time.

- [ ] **Step 2: Commit**

```bash
git add docs/files.md
git commit -m "docs: add file transfer spec (upload chunking, execute, management)"
```

---

### Task 10: Tools Page

**Files:**
- Create: `docs/tools.md`

**Reference:** `spec.h` (tool commands).

- [ ] **Step 1: Write `docs/tools.md`**

**Command summary table:**

| Command | Code | Arguments | Types & Units | Required |
|---------|------|-----------|---------------|----------|
| `air_on` | `0x7FF1` | none | — | No |
| `air_off` | `0x7FF0` | none | — | No |

**Sections to write:**

- **Air assist:** Controls the air assist solenoid. `air_on` activates compressed air flow to the cutting head; `air_off` deactivates it. No arguments. LightBurn sends these around cutting operations when air assist is enabled for a layer.

- **Future tools:** The 0x7000 range is reserved for tool control. Additional tool commands (e.g., rotary chuck, water cooling, exhaust) may be added in future protocol revisions.

- This is a short page. Mark air assist as optional — many machines handle air assist independently of software control.

- [ ] **Step 2: Commit**

```bash
git add docs/tools.md
git commit -m "docs: add tool commands spec (air assist)"
```

---

### Task 11: Vendor Extensions Page

**Files:**
- Create: `docs/vendor-extensions.md`

**Reference:** `spec.h` — `0xD000` reserved for vendor-specific configuration.

- [ ] **Step 1: Write `docs/vendor-extensions.md`**

**Sections to write:**

- **Purpose:** The `0xD000` MSN range is reserved for vendor-specific commands. Manufacturers can define custom commands in this range for hardware features that aren't covered by the standard LBP command set.

- **Rules:**
  - Vendor commands MUST use command codes in the range `0xD000`-`0xDFFF`.
  - Vendor commands MUST follow standard LBP framing (DRGN header, size, payload, CRC16).
  - Vendor commands SHOULD document their argument format for internal use.
  - LightBurn will ignore (skip) any vendor-specific commands it doesn't recognize — they will not cause errors.

- **Use cases:** Custom homing sequences, proprietary sensor readings, machine-specific calibration, vendor diagnostic commands.

- **Coordination:** If a vendor believes their custom command would be useful as a standard LBP command, they can propose it for inclusion in the `0xE000` (Extended Future Capabilities) range. Contact information TBD.

- **Also note `0xE000`:** Reserved for future standard extensions. Not currently defined. Vendors should NOT use this range.

- [ ] **Step 2: Commit**

```bash
git add docs/vendor-extensions.md
git commit -m "docs: add vendor extensions spec (0xD000 custom command range)"
```

---

### Task 12: Index Page

**Files:**
- Create: `docs/index.md`

- [ ] **Step 1: Write `docs/index.md`**

This is the landing page and navigation hub. Content:

- **Title:** LightBurn Protocol (LBP) Documentation
- **One-paragraph summary:** What this documentation is and who it's for.
- **Minimum Viable Implementation Path** — numbered list with links:
  1. [Introduction](introduction.md) — Start here if you're new to LBP
  2. [Wire Format & Framing](framing.md) — **Required reading** — understand message structure before anything else
  3. [Job Control](job-control.md) — Handshake, stop/pause/continue, job structure
  4. [Movement](movement.md) — Absolute/relative moves, homing, jogging, dwell
  5. [Laser Control](laser.md) — Power, frequency, enable/disable, emission
  6. [Machine State](state.md) — Position reporting, state flags, telemetry
  7. [Configuration](configuration.md) — Persistent machine settings with commit pattern
  8. [Job Settings](settings.md) — Per-job speed, boundaries, cut type

- **Optional / Advanced:**
  - [File Transfer](files.md) — Upload and execute cut files
  - [Tool Control](tools.md) — Air assist and future peripherals
  - [Vendor Extensions](vendor-extensions.md) — Define custom commands in the 0xD000 range

- **Protocol version note:** This documentation describes LBP as of [date]. The protocol is actively being finalized — sections marked DRAFT may change.

- [ ] **Step 2: Commit**

```bash
git add docs/index.md
git commit -m "docs: add index page with minimum viable implementation path"
```

---

### Task 13: HTML Preview Page

**Files:**
- Create: `docs/preview.html`

- [ ] **Step 1: Write `docs/preview.html`**

A single self-contained HTML file. Requirements:

- Load marked.js from CDN (`https://cdn.jsdelivr.net/npm/marked/marked.min.js`)
- Load highlight.js from CDN for code block syntax highlighting
- On page load, fetch all `.md` files (hardcoded list matching the file structure) via `fetch()` and render each with `marked.parse()`
- **Sidebar nav** (left, fixed, dark background):
  - Links to each rendered section (anchor links)
  - Sections listed in the minimum viable path order
  - Current section highlighted on scroll
- **Content area** (right, light background):
  - Each markdown file rendered as a section with an `<h1>` separator
  - Code blocks with syntax highlighting
  - Tables styled with borders and padding
  - Responsive — readable on a laptop screen
- **Styling:**
  - Dark sidebar (#1a1a2e or similar), light content (#fafafa)
  - Geist Mono or monospace for code blocks
  - System font stack for body text
  - Max content width ~800px for readability
- **Note at top of HTML:** "This preview requires a local HTTP server to load .md files. Run `python3 -m http.server 8000` from the docs/ directory and open http://localhost:8000/preview.html"
- No build step, no npm, no bundler — pure HTML/CSS/JS with CDN dependencies.

- [ ] **Step 2: Test the preview locally**

```bash
cd docs && python3 -m http.server 8000 &
# Open http://localhost:8000/preview.html in browser
# Verify: sidebar nav works, all sections render, code blocks highlighted, tables formatted
```

- [ ] **Step 3: Commit**

```bash
git add docs/preview.html
git commit -m "docs: add HTML preview page for local documentation rendering"
```

---

### Task 14: Final review pass

- [ ] **Step 1: Cross-reference check**

Open each spec page and verify:
- All command codes match `spec.h` exactly
- All argument types and units match expected payload sizes
- All inter-page links work (e.g., framing.md references from other pages)
- No proprietary LightBurn internals leaked into the docs
- DRAFT markers present on all incomplete sections
- Required badges consistent with the minimum viable path in index.md

- [ ] **Step 2: Preview check**

Load preview.html and read through the entire documentation end-to-end. Check for:
- Consistent formatting across pages
- No broken markdown rendering
- Tables render correctly
- Hex examples are readable and accurate

- [ ] **Step 3: Final commit if any fixes were needed**

```bash
git add docs/
git commit -m "docs: final review pass — fix cross-references and formatting"
```
