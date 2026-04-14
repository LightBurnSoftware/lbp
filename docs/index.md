# LightBurn Protocol (LBP) Documentation

This documentation describes the LightBurn Protocol — a binary communication protocol for laser cutting and engraving hardware. It is written for laser manufacturers who want to build LBP support into their controller firmware to achieve compatibility with LightBurn software.

If you're new to LBP, start with the [Introduction](introduction.md) for a high-level overview.

---

## Minimum Viable Implementation

To achieve basic LightBurn compatibility, implement the following in order. Each page marks individual commands as **Required** or optional.

| Step | Page | What You'll Implement |
|------|------|-----------------------|
| 1 | [Wire Format & Framing](framing.md) | **Required reading.** Understand the binary message structure before anything else. |
| 2 | [Job Control](job-control.md) | Handshake, stop/pause/continue, job header/body structure. |
| 3 | [Movement](movement.md) | Absolute and relative XY moves, XY homing, jogging, dwell. |
| 4 | [Laser Control](laser.md) | Power, frequency, enable/disable, emission on/off. |
| 5 | [Machine State](state.md) | Position reporting and machine state flags. |
| 6 | [Configuration](configuration.md) | Persistent machine settings with the set-and-commit pattern. |
| 7 | [Job Settings](settings.md) | Per-job speed, boundaries, and cut type. |

Once these are implemented, your controller will support cutting, engraving, and manual positioning through LightBurn.

---

## Optional / Advanced

| Page | Description |
|------|-------------|
| [File Transfer](files.md) | Upload cut files to the controller for offline execution. |
| [Tool Control](tools.md) | Air assist and future auxiliary peripherals. |
| [Vendor Extensions](vendor-extensions.md) | Define custom commands in the `0xD000` range for hardware-specific features. |

---

## Quick Reference

| Property | Value |
|----------|-------|
| Header | `0x4452474E` ("DRGN") |
| Byte order | Big-endian (network byte order) |
| Checksum | CRC16-CCITT (poly 0x1021, init 0xFFFF) |
| Min message | 10 bytes |
| Max command message | 26 bytes |
| File chunk message | 512 bytes |
| Transport | Any byte stream (serial, USB, TCP) |
| Position units | Micrometers (um) |
| Speed units | Micrometers/sec |
| Acceleration units | Micrometers/sec^2 |

---

*This specification is a living document. Sections marked **DRAFT** are not yet finalized and may change. Last updated: 2026-03-31.*
