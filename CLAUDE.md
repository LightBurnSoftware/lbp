# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Toothless is a C++ project implementing the **LightBurn Protocol (LBP)** — a binary protocol for communicating with laser cutting hardware — plus a Qt6-based firmware simulator GUI. The project name "Toothless" and protocol header `0x4452474E` ('DRGN') share a dragon theme.

## Build Commands

Both components use CMake. The LBP library requires C++17 and CMake 3.15+. The GUI requires Qt6 6.5+ (Core, Widgets, Network) and CMake 3.19+.

```bash
# Build the LBP library
cd lbp && mkdir -p build && cd build
cmake .. && make

# Build the GUI (must build lbp first — GUI links against it)
# Qt6 is installed via Qt Creator at ~/Qt — set CMAKE_PREFIX_PATH accordingly.
cd lbp-simulator && mkdir -p build && cd build
cmake .. -DCMAKE_PREFIX_PATH=~/Qt/6.5.7/macos && make

# Run the (minimal) LBP test
cd lbp/build && ./TestLBP
```

The GUI's CMakeLists.txt references the LBP library via `${CMAKE_CURRENT_SOURCE_DIR}/../lbp`, so the two directories must remain siblings. Qt 6.10.1 is also available at `~/Qt/6.10.1/macos`.

## Architecture

### LBP Library (`lbp/`)

A zero-dependency, stack-allocated static library implementing the binary protocol. Headers live in `include/lbp/`, sources in `src/`.

- **Protocol spec** (`spec.h`): All command codes, constants, flags, MSN categories, and size definitions. Commands are categorized by MSN (Most Significant Nibble) — e.g., `0x1000` = laser, `0x6000` = movement, `0xC000` = configuration. Wire format: `[4-byte DRGN header][2-byte size][payload][2-byte CRC16]`.
- **Parser** (`parser.h`): Header-only incremental state machine (HeaderSync → Size → Data → Checksum) that reads from a ring buffer.
- **RingBuffer** (`ringbuffer.h`): Header-only power-of-2 ring buffer for single-producer/single-consumer use. Stack-allocated to avoid heap fragmentation.
- **Message** (`message.h`): Header-only template-based builder — construct, append args, checksum auto-calculated. Two specializations: `CmdMsg` (small) and `MaxMsg` (file chunks).
- **Payload** (`payload.h`, `src/payload.cpp`): Read-cursor container for extracting typed arguments from received packets. Two specializations: `CmdPayload` and `MaxPayload`.
- **Checksum** (`checksum.h`, `src/checksum.cpp`): CRC16-CCITT (polynomial 0x1021, reversed 0x8408, init 0xFFFF).
- **BEIO** (`beio.h`, `src/beio.cpp`): Big-endian input/output helpers for reading/writing values to network-byte-order data buffers.
- **Queue** (`queue.h`): Header-only FIFO queue template for command and message queuing.

All integers are big-endian (network byte order). Payload sizes: command code = 2 bytes, max command payload = 18 bytes (2 cmd + 16 args), file chunk = 502 bytes (within a 512-byte message).

### GUI Simulator (`lbp-simulator/`)

A Qt6 firmware simulator that accepts LBP commands over TCP (port 6666) and visualizes machine state:

- **FirmwareSim** orchestrates processing via a **component chain** (SimComponent interface): FileSystem → Configuration → Movement. Order matters — filesystem is processed first. Unhandled commands fall through to FirmwareSim itself.
- **Connection** wraps QTcpServer (single client at a time), feeds bytes into an LBP RingBuffer.
- **MovementSim** is a state machine (Idle → Jogging → Moving → Dwelling) simulating 4-axis motion (X/Y/Z/U in micrometers) with velocity normalization and elapsed-time physics.
- **Configuration** uses a set-and-commit pattern: staged changes are only persisted (to `~/Documents/config.json` via nlohmann/json) on explicit `cmd_commit_cfg`.
- **FileSystem** buffers incoming file chunks (up to 65KB) during upload sequences.
- **SimView** renders a 2D thermal visualization of laser activity on the work surface.
- **MainWindow** runs a 10ms timer loop driving the simulation update cycle.

## Current Work: Vendor-Facing Documentation

**Goal:** Create public documentation for laser manufacturers who ask "How can I get my laser to be supported by LightBurn?" The docs describe the LBP spec so vendors can program their controllers to work with LightBurn.

**Key context:**
- LBP (`lbp/`) is both the protocol spec definition AND a reference library for parsing/building packets
- `lbp-simulator/` is a firmware simulator AND a partial example firmware implementation (still WIP)
- The protocol is transport-agnostic — works over TCP, serial/UART, and USB
- All integers are big-endian (network byte order)
- The protocol is NOT yet complete — docs will be updated over the next couple weeks as implementation is finalized
- Docs are public/external-facing — must NOT include proprietary LightBurn internals, only what vendors need to implement the spec
- Final docs will live on Zendesk Knowledge site; markdown copies maintained in `docs/` directory

**Documentation status:**
All planned doc pages have been written and are in `docs/`. The design spec (`docs/spec-design.md`) and implementation plan (`docs/implementation-plan.md`) are complete. An HTML preview page (`docs/preview.html`) is available for local review.

**Completed pages:**
- [x] Introduction page (`introduction.md`)
- [x] Wire format / framing (`framing.md`)
- [x] Job control (`job-control.md`)
- [x] Movement (`movement.md`)
- [x] Laser control (`laser.md`)
- [x] Configuration (`configuration.md`)
- [x] Job settings (`settings.md`)
- [x] File transfer (`files.md`)
- [x] Machine state & telemetry (`state.md`)
- [x] Tool control (`tools.md`)
- [x] Vendor extensions (`vendor-extensions.md`)
- [x] Index / landing page (`index.md`)
- [x] HTML preview page (`preview.html`)

**Known doc issues requiring attention:**
- Multiple DRAFT sections still need finalization (response/ACK mechanism, file execution args, telemetry responses)

**Source code observations (not bugs in docs, but worth noting):**
- `spec.h:61` has "Vender" typo (should be "Vendor")

## Agent Instructions
1. First think through the problem, read the codebase for the relevant files or resources.
2. Before you make any major changes check in with me and I will verify the plan.
3. Give a high level explanation of the changes you make every step of the way.
4. Make every task and code change as simple as possible. Avoid making massive or complex changes. Every change should impact as little code as possible. Maintain simplicity everywhere possible.
5. Maintain a documentation file that describes how the architecture of the app works inside and out.
6. Never speculate about code or files you have not opened. If the user references a specific file, you MUST read the file before answering. Make sure to investigate and read relevant files BEFORE answering or making changes to the codebase. Never make any claims about the code before investigating unless you are certain of the correct answer. Give grounded and hallucination-free answers.
7. Cite sources of information. This project is a compilation of knowledge, so it needs to be accurate and able to be verified. The information should come from official sources of truth and not random forum posts. 