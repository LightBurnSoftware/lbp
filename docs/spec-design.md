# LBP Vendor Documentation — Design Spec

**Date:** 2026-03-31
**Status:** Approved
**Author:** Joe Spanier / Claude Code

---

## Purpose

Create public-facing documentation for laser manufacturers who want to integrate their controllers with LightBurn software. When a manufacturer asks "How can I get my laser supported by LightBurn?", these docs are the answer.

The documentation describes the LightBurn Protocol (LBP) — a binary communication protocol — in enough detail for a firmware engineer to implement support on their controller hardware.

## Constraints

- **Public and external-facing.** No proprietary LightBurn internals.
- **Protocol is incomplete.** Several commands are still being finalized. Incomplete sections must be clearly marked as "Draft."
- **Living documents.** Will be updated over the next several weeks as the protocol is finalized.
- **Dual-home.** Markdown source of truth lives in `docs/` in this repo. Final versions will be published to the Zendesk Knowledge site.

## Audience

- **Introduction page:** Sales, management, and business development — people who need to understand what LBP is and why it exists, without byte-level details.
- **Spec pages:** Firmware engineers at laser manufacturers — people who will read hex dumps and implement the protocol on embedded controllers.

## File Structure

```
docs/
  index.md              — Landing page: what LBP is, minimum viable path, page map
  introduction.md       — Business-level overview (non-technical audience)
  framing.md            — Wire format: DRGN header, size field, payload, CRC16, byte order
  job-control.md        — 0x0000: handshake, stop/pause/continue, job header/body, frame
  laser.md              — 0x1000: power, frequency, enable/disable, on/off, focus
  files.md              — 0x4000: file upload chunking, begin/end, execute
  settings.md           — 0x5000: speed, boundaries, cut type
  movement.md           — 0x6000: absolute/relative moves, jogging, homing, dwell
  tools.md              — 0x7000: air assist on/off (and future tool commands)
  state.md              — 0x8000: position queries, machine state flags, telemetry
  configuration.md      — 0xC000: axis config, laser config, cut/engrave settings, commit pattern
  vendor-extensions.md  — 0xD000: how vendors define custom commands in the reserved range
  preview.html          — Self-contained HTML page that renders all .md files with sidebar nav
```

### Rationale

Pages map 1:1 to MSN (Most Significant Nibble) command categories in the protocol. This means:
- Each page can be updated independently as that section of the protocol is finalized.
- Vendors can bookmark exactly the section they're working on.
- The structure mirrors how the protocol itself is organized in `spec.h`.

Note: `0x5000` (settings) and `0xC000` (configuration) are separate pages because settings are per-job runtime values while configuration is persistent machine setup with a commit pattern.

## Page Conventions

### Command tables

Every spec page opens with a summary table:

| Command | Code | Arguments | Types & Units | Required |
|---------|------|-----------|---------------|----------|
| `move_abs_xy` | `0x6A03` | x, y | int32 micrometers, int32 micrometers | Yes |

### Minimum viable markers

Commands required for minimum LightBurn support are marked with a **Required** badge. Optional/advanced commands are unmarked. The index page lists the minimum viable set explicitly.

### Byte-level examples

Each command includes at least one hex-annotated example showing the full wire format:

```
44 52 47 4E   — DRGN header
00 0A         — payload size (10 bytes)
6A 03         — command: move_abs_xy
00 00 27 10   — x: 10000 um (10 mm)
00 00 2E E0   — y: 12000 um (12 mm)
XX XX         — CRC16
```

### Units

All units are explicit in every table and example. Standard units in LBP:
- Position: micrometers (um)
- Speed: micrometers/sec
- Acceleration: micrometers/sec^2
- Time: microseconds (dwell), milliseconds (delays)
- Power: percent (0-100), or percent * 10 where noted
- Frequency: Hz
- Distance (breadth, step length): nanometers where noted

### Draft markers

Incomplete sections use a visible callout:

> **DRAFT** — This section describes a command that is not yet finalized. The command code and argument format may change.

## Minimum Viable Implementation Path

The `index.md` page will lay out the recommended implementation order for a vendor starting from scratch:

1. **Framing** (`framing.md`) — Understand the wire format. Required reading before anything else.
2. **Job Control** (`job-control.md`) — Implement handshake to establish communication. Implement stop/pause/continue for safety.
3. **Movement** (`movement.md`) — At minimum: absolute XY moves, homing XY. Jogging is strongly recommended.
4. **Laser** (`laser.md`) — Power control, enable/disable, on/off.
5. **State** (`state.md`) — Respond to position queries and machine state requests.
6. **Configuration** (`configuration.md`) — Accept and persist machine configuration from LightBurn.
7. **Settings** (`settings.md`) — Accept per-job speed and boundary settings.
8. _Optional:_ Files (`files.md`), Tools (`tools.md`), Vendor Extensions (`vendor-extensions.md`).

## Open Questions

These are tracked as "Draft" callouts in the relevant doc pages:

- **Response/ACK mechanism:** How does the controller respond to query commands (e.g., `cmd_get_state`, `cmd_pos_axis_x`)? Does it send back an LBP packet with the same command code plus data? Is there an ACK for write commands? _Waiting on engineer input._
- **Several commands in `spec.h` are marked TODO** — argument formats and return types are not yet defined. These will be documented as Draft until finalized.
- **0xE000 range** — Reserved for "Extended Future Capabilities." Not documented yet.

## HTML Preview Page

A single self-contained `preview.html` file that:
- Uses marked.js (loaded from CDN) to render markdown
- Fetches all `.md` files and renders them in a single scrollable view
- Provides a sidebar nav with links to each section
- Requires no build step — open the file in a browser and it works
- Styled for readability: dark sidebar, light content area, code blocks with syntax highlighting

## Protocol Quick Reference (for spec authors)

These details from the codebase inform the documentation:

- **Header:** `0x4452474E` ("DRGN") — 4 bytes
- **Size field:** 2 bytes, big-endian — size of the payload only (not including header, size field, or CRC)
- **Payload:** Command code (2 bytes) + arguments (variable)
- **CRC:** CRC16-CCITT (polynomial 0x1021, reversed 0x8408, init 0xFFFF) — 2 bytes at end
- **Byte order:** All multi-byte integers are big-endian (network byte order)
- **Max command payload:** 18 bytes (2-byte cmd + up to 16 bytes of args)
- **Max file chunk payload:** 498 bytes (within a 512-byte message)
- **Minimum message size:** 10 bytes (4 header + 2 size + 2 cmd + 2 CRC)
- **MSN categories:** Command codes use the most significant nibble for broad category, with bitwise composition for axis flags and sub-categories
- **Axis flags (LSN):** x=0x0001, y=0x0002, z=0x0004, u=0x0008
- **Transport:** Protocol is transport-agnostic — works over TCP, serial/UART, and USB
