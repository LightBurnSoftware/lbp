# LightBurn Protocol (LBP) — Introduction

## What is the LightBurn Protocol?

The LightBurn Protocol (LBP) is a standardized binary communication protocol that enables laser cutting and engraving hardware to communicate with LightBurn software. It defines a structured, well-documented interface between the controller firmware running on a laser machine and the LightBurn application that drives it.

LBP is the answer to the question every laser manufacturer eventually asks: **"How do I get my hardware supported by LightBurn?"**

## Why does LBP exist?

Historically, integrating a new laser controller with LightBurn required custom engineering effort on both sides — reverse-engineering communication patterns, building one-off adapters, and maintaining fragile compatibility layers. This was slow, error-prone, and difficult to scale.

LBP replaces that approach with a single, published specification. Manufacturers no longer need to coordinate bespoke integrations. Instead, they implement one well-documented protocol, and their hardware works with LightBurn.

The result is a clear, repeatable path to compatibility — faster for manufacturers, more reliable for end users, and easier for everyone to maintain.

## What does LBP enable?

Any laser hardware manufacturer can build LBP support directly into their controller firmware. Once a controller speaks LBP, it is compatible with LightBurn — no proprietary adapters, no middleware, no special arrangements.

This means:

- **Manufacturers** gain access to the LightBurn ecosystem by implementing a single specification.
- **Firmware engineers** have a concrete, byte-level reference for every command the controller needs to handle.
- **End users** get reliable, first-class support for their hardware in LightBurn without waiting for custom integration work.

## How does it work?

At a high level, LBP works as a command-and-response protocol:

1. **LightBurn sends commands.** These are compact, structured binary packets — instructions like "move to this position," "set laser power," "start a job," or "report your current state."

2. **The controller executes them.** The manufacturer's firmware receives each packet, interprets the command, carries out the operation on the hardware, and reports status back to LightBurn.

3. **Communication is continuous.** During a job, LightBurn streams a sequence of commands that coordinate movement, laser firing, and machine configuration. The controller processes them in order and keeps LightBurn informed of its state.

Every message follows the same predictable structure: a fixed header for synchronization, a payload containing the command and its parameters, and a checksum for integrity verification. The protocol is compact and efficient, designed for the real-time demands of laser control.

## Transport flexibility

LBP is transport-agnostic. It operates over any reliable byte-stream connection — serial (UART), USB, TCP/IP, or any other transport the manufacturer prefers. The protocol itself does not depend on the physical or link layer; it only requires that bytes arrive in order and intact.

This means manufacturers can choose the connection method that best fits their hardware platform without any changes to their LBP implementation.

## Extensibility

LBP includes a reserved command range specifically for vendor extensions. Manufacturers can define custom commands within this range to support hardware-specific features — proprietary calibration routines, specialized sensor queries, or any capability unique to their platform.

Vendor extensions coexist with the standard command set without conflict, so manufacturers can differentiate their products while maintaining full LightBurn compatibility.

## What's in the rest of this documentation

The remaining pages in this documentation set are written for firmware engineers and provide the full technical specification needed to implement LBP on a controller. Here is a brief guide:

| Page | Description |
|------|-------------|
| **Wire Format & Framing** | The binary message structure — header, size field, payload, checksum, and byte order. Start here for any technical implementation. |
| **Job Control** | Handshake, connection management, and job lifecycle commands (start, stop, pause, continue). |
| **Movement** | Absolute and relative motion commands, jogging, homing, and dwell operations. |
| **Laser** | Laser power, frequency, enable/disable, on/off control, and focus commands. |
| **Settings** | Per-job runtime parameters such as speed, boundaries, and cut type. |
| **State & Telemetry** | Position queries, machine state flags, and status reporting. |
| **Configuration** | Persistent machine setup — axis parameters, laser configuration, and the set-and-commit pattern. |
| **File Transfer** | Uploading files to the controller in chunks, with begin/end sequencing. |
| **Tool Control** | Air assist and other auxiliary tool commands. |
| **Vendor Extensions** | How to define and use custom commands in the reserved range for hardware-specific features. |

For manufacturers beginning a new integration, we recommend reading the Wire Format & Framing page first to understand the message structure, then proceeding through Job Control, Movement, and Laser — which together form the minimum viable implementation for LightBurn compatibility.

---

*This specification is a living document and will be updated as the protocol is finalized. Sections marked as "Draft" in the technical pages indicate commands or formats that are not yet stable.*
