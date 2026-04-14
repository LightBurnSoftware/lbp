# LightBurn Protocol (LBP)

## Overview

LBP is a transport-agnostic command-and-response binary protocol for controlling laser cutters and engravers.

LBP is also a work-in-progress. Some functionality has not yet been implemented, and the structure of some messages and workflows is still to be determined. Feedback and commentary is welcome.

**Note:** This document is intended as an overview and companion to the documentation present in the `lbp` headers, particularly `spec.h`.

### Transport-Agnostic

LBP is intended to function over a variety of different transport layers, e.g. serial, TCP, UDP, websocket, etc. This specification assumes that the transport layer will deliver LBP messages quickly and in the correct order.

**Note** Currently, only TCP is implemented in LightBurn and in the simulator.

### Command-and-Response

At a high level, LBP works as a command-and-response protocol:

1. **LightBurn sends commands.** These are compact, structured binary messages — instructions like "move to this position," "set laser power," "start a job," or "report your current state."

2. **The controller executes them and responds.** The manufacturer's firmware receives each packet, interprets the command, carries out the operation on the hardware, and responds to Lightburn with another message. This message always contains the same command code as the request along with optional arguments in the case of a query.

In the case of commands that result in long-running operations, the firmware's response is an **acknowledgement** of the request.
It **begins** the operation and responds to the command **immediately** - **not** at the conclusion of the operation.

## Message Structure

```
+--------------+------+----------+----------+
| Header       | Size | Payload  | Checksum |
| [4B]: "DRGN" | [2B] | [Size B] | [2B]     |
+--------------+------+----------+----------+
```

- **Header (4 bytes):** The 4-byte sequence `0x4452474E`, which is the ASCII encoding of `DRGN` (short for "dragon").
- **Size (2 bytes):** A  integer which encodes the size, in bytes, of the following **Payload**.
- **Payload ("Size" bytes):** Always consists of a 2-byte Command, followed by 0 or more bytes of argument data. (This means that **Size** must always be `>= 2`)
- **Checksum (2 bytes):** The CRC16 checksum of the **Payload**, computed over **only** the Payload data, **not** the Header of Size data. (See `checksum.cpp` for the crc16 algorithm.)

**IMPORTANT** All Size, Payload, and Checksum data is encoded in **Big-Endian Format**. (Helper functions are provided in `lbp/beio.h`)

See the `spec.h` header file for message dimension constants, command code definitions, and argument information.

## Simple Workflows

### Handshake

LBP provides a simple no-op "Handshake" command code. This helps confirm communication between LightBurn and the firmware.

**LightBurn Sends:**

```
+--------------+-------+---------+----------+
| Header       | Size  | Payload | Checksum |
| 44 52 47 4e  | 00 02 | 01 b8   | 5c 2f    |
+--------------+-------+---------+----------+
```

The Handshake is the simplest kind of message in the LBP: The payload is a simple command code with no additional arguments.

**Firmware Responds:**

```
+--------------+-------+---------+----------+
| Header       | Size  | Payload | Checksum |
| 44 52 47 4e  | 00 02 | 01 b8   | 5c 2f    |
+--------------+-------+---------+----------+
```

The firmware responds to the handshake command in the same way it responds to all commands - with another LBP message with the same command code.

### Position Query

Here we use the code command code `cmd_pos_axis_x`, defined in `lbp/spec.h` to ask for the current x position of the laser in micrometers:

**LightBurn Sends:**

```
+--------------+-------+---------+----------+
| Header       | Size  | Payload | Checksum |
| 44 52 47 4e  | 00 02 | 81 01   | da 8b    |
+--------------+-------+---------+----------+
```

**Firmware Responds:**

```
+--------------+-------+-------------------+----------+
| Header       | Size  | Payload           | Checksum |
| 44 52 47 4e  | 00 06 | 81 01 00 00 4e 20 | 36 00    |
+--------------+-------+-------------------+----------+
```

Our laser's x-axis position is 20 mm. So, the firmware responds: The payload consists of the same command,
(`cmd_pos_axis_x`: `81 01`) followed by a 4-byte integer `0x00004e20`, which is 20,000 micrometers.
We see here that the payload size is 6 bytes: 2 bytes for the command code acknowledgement + 4 bytes for the x position argument.

### Absolute Move in XY

Let's ask the firmware to move to the absolute position `(10mm, 20mm)`:

**LightBurn Sends:**

```
+--------------+-------+-------------------------------+----------+
| Header       | Size  | Payload                       | Checksum |
| 44 52 47 4e  | 00 0a | 6a 03 00 00 27 10 00 00 4e 20 | 53 a9    |
+--------------+-------+-------------------------------+----------+
```

Here we see our size is 10 bytes:
- 2 bytes for the command (`cmd_move_abs_xy`).
- 4 bytes for the x position in micrometers.
- 4 bytes for the y position in micrometers.

**Firmware Responds:**
```
+--------------+-------+---------+----------+
| Header       | Size  | Payload | Checksum |
| 44 52 47 4e  | 00 02 | 6a 03   | f9 a5    |
+--------------+-------+---------+----------+
```
The firmware schedules the movement and responds immediately with the same `cmd_move_abs_xy` command. The response is sent
as soon as the command is acknowledged, **not** after the laser has successfully (or unsuccessfully) moved to the target position.

### State Query

LightBurn will periodically query the firmware with `cmd_get_state`. The firmware is expected to reply with a collection of flags.
See `cmd_get_state` in `lbp/spec.h` for details.

## Files
LBP supports the workflow of packaging many individual and complete LBP messages into **Files**.
A **File** is simply a series of concatenated LBP **messages**. **Files** are sent to the firmware
in the form of **File Chunk** messages.

**File Chunk** messages are expected to be large; the LBP library currently sets their size to 512 bytes.
(In contrast, no LBP command messages are expected to be larger than 26 bytes.)

Files are sent to the firmware with the following workflow:

- LightBurn concatenates many messages into a binary file.
- LightBurn breaks the binary file into *N* chunks where *N* is calculated from the length of the file and the maximum message payload capacity.
- LightBurn sends `cmd_begin_file` message.
- LightBurn sends *N* `cmd_file_chunk` messages.
- LightBurn sends `cmd_end_file` message.

If LightBurn then sends a `cmd_execute` message, the firmware is expected to parse the file it has just received and execute all commands as a **Job**.

### TODO:

- Commands for more advanced file operations, such as loading, saving, naming, querying, and deleting files are considered, but not yet designed or implemented.  Streaming jobs, rather than sending bulk files, is a future possibility for low-memory controllers.
- We may add a 32-bit integer argument to the `cmd_file_chunk` message - either the chunk index, or the byte offset of the chunk within the file.

## Jobs

We will here use the term **Job** to refer to a full cutting/engraving task sent from LightBurn to the firmware.

Jobs are encoded with the following series of messages:
- `cmd_job_begin`
  - `cmd_job_header_begin`
    - *job setting message #1*
    - *job setting message #2*
    - ...
	- *final job setting message*
  - `cmd_job_header_end`
  - `cmd_job_body_begin`
    - *job command message #1*
    - *job command message #2*
	- ...
	- *final job command message*
  - `cmd_job_body_end`
- `cmd_job_end`

The `cmd_job_start` and `cmd_job_end` bookends are important because they let the firmware know that a job is being executed.

**Note**
Currently, the Lightburn LBP implementation and the LBP simulator support only the following workflow for jobs:

1. LightBurn sends the entire job to the firmware as a **file**.
2. LightBurn sends the `cmd_execute` command to signal that the firmware should execute its received file.

## Frame Operations

Framing operations are currently planned as bulk operations - similar to **Jobs**.
- `cmd_frame_begin`
  - *frame command message #1*
  - *frame command message #2*
  - ...
  - *final frame command message*
- `cmd_frame_end`

### TODO
The framing operation messages are defined in `lbp/spec.h`, but the operations are not yet implemented in the LightBurn LBP implementation or in the LBP Simulator.

## Configuration
Many of the command codes defined in `lbp/spec.h` begin with the `cfg_` prefix. This designates them as **Configuration** commands.
**Configuration** values represent important physical properties or settings of the machine. A machine's **Configuration** is expected
to persist between power cycles.

### Configuration Query

Configurations are queried by LightBurn one value at a time using the following workflow:

1. LightBurn sends a message with the `cfg_` code as its command and no additional arguments.
2. The firmware responds with a message with that same `cfg_` command, with a single 32-bit argument representing that command's current value.

This 32-value usually represents a single integer value, but in some cases represents a boolean value, or a set of flags.
These are documented in `lbp/spec.h`.

As an example, let's query the focus distance (`cfg_focus_distance`):

**LightBurn Sends:**

```
+--------------+-------+---------+----------+
| Header       | Size  | Payload | Checksum |
| 44 52 47 4e  | 00 02 | c2 11   | 55 f7    |
+--------------+-------+---------+----------+
```

**Firmware Responds:**

```
+--------------+-------+-------------------+----------+
| Header       | Size  | Payload           | Checksum |
| 44 52 47 4e  |00 06  | c2 11 00 00 4e 20 | da ba    |
+--------------+-------+-------------------+----------+
```
We see a response of 20,000 micrometers.

### Configuration Set

To set a configuration, at least two messages are necessary - the new value and a `cmd_commit_cfg` message.
LightBurn may send several new Configuration values in a row before sending a **commit** message.
New configurations are commited (applied) all at once by the firmware upone receiving a `cmd_commit_msg`.

As an example, let's set the User Origin to x=20mm, y=10mm.
First, we set the User Origin X coordinate to 20mm (`cfg_user_origin_x`):

**LightBurn Sends:**

```
+--------------+-------+-------------------+----------+
| Header       | Size  | Payload           | Checksum |
| 44 52 47 4e  | 00 06 | c0 61 00 00 4e 20 | 7f a7    |
+--------------+-------+-------------------+----------+
```

**Firmware Responds:**

```
+--------------+-------+---------+----------+
| Header       | Size  | Payload | Checksum |
| 44 52 47 4e  | 00 02 | c0 61   | 62 b7    |
+--------------+-------+---------+----------+
```

Now we set the User Origin Y coordinate to 10mm (`cfg_user_origin_y`):

**LightBurn Sends:**

```
+--------------+-------+-------------------+----------+
| Header       | Size  | Payload           | Checksum |
| 44 52 47 4e  | 00 06 | c0 62 00 00 27 10 | 7d 39    |
+--------------+-------+-------------------+----------+
```

**Firmware Responds:**

```
+--------------+-------+---------+----------+
| Header       | Size  | Payload | Checksum |
| 44 52 47 4e  | 00 02 | c0 62   | 59 85    |
+--------------+-------+---------+----------+
```

And now the commit message (`cmd_commit_cfg`):

**LightBurn Sends:**

```
+--------------+-------+---------+----------+
| Header       | Size  | Payload | Checksum |
| 44 52 47 4e  | 00 02 | 0c cc   | 87 aa    |
+--------------+-------+---------+----------+
```

**Firmware Responds:**

```
+--------------+-------+---------+----------+
| Header       | Size  | Payload | Checksum |
| 44 52 47 4e  | 00 02 | 0c cc   | 87 aa    |
+--------------+-------+---------+----------+
```

### TODO
Most of the configuration definitions values are WIP (works in progress). Very few have been meaningfully implemented in the simulator,
and units and flag definitions are subject to change. We have started with a best guess of common configuration codes; feedback is welcome.

We have reserved the command space `0xC000` - `0xCFFF` for configuration codes that we expect to be common across many machines.
We expect vendor machines to have their own custom configuration values as well, so we reserve the command space `0xD000` - `0xDFFF`
for vender-specific configuration codes.

## LBP Library (lbp)

A small library, `lbp`, is provided to assist in firmware development. It consists of:

- `spec.h`: defines the specification for LBP messages and all command and configuration codes.
- `payload.h`/`payload.cpp`: defines a the Payload class, a helpful container for the payload section of successfully parsed lbp messages.
- `parser.h`: defines a buffered Parser class, which can parse incoming bytes into lbp Payloads.
- `ringbuffer.h`: defines a Ring Buffer, which provides the buffer space for a Parser.
- `message.h`: defines the Message class, provided to help assemble outgoing lbp messages for transmission.
- `beio.h`/`beio.cpp`: defines helper functions for reading and writing Big-Endian values to byte buffers.
- `queue.h`: a templated FIFO queue. Useful for queueing up movement commands or preparing outgoing messages for transmission.
- `checksum.h`/`checksum.cpp`: provides the crc16 implementation.

This library is intended to be firmware-ready and embedded-friendly, so it requires no heap allocations. All container classes
are templated for maximum capacity with helpful specializations provided in the headers.

These classes are documented in the headers with usage examples, but it's worth discussing **Payload** and **Message**
in particular, since it may seem odd that we `lbp` provides two separate helper classes for messages.

### The Payload class

The **Payload** class contains the payload section of a parsed LBP message. It is designed to be constructed by a Parser.
**Payload** provides helpful methods for reading arguments. Think of it as the **input class** of a LBP message.
The header checksum are not included, as they are not useful after the payload has been parsed.

The **Payload** class is templated by maximum capacity. Two template specializations are provided, which should be all that is necessary:
- `CmdPayload` is large enough to fit all commands that are not `cmd_file_chunk`, as none are expected to require more than four 32-bit integers.
- `MaxPayload` is large enough to accommodate the maximum possible payload, being a `cmd_file_chunk`. The **Parser** outputs one of these at a time.

(See `lbp/spec.h` for the definitions of these dimensions).

### The Message class

The **Message** class assists in the construction of a full LBP message to prepare for transport. It provides
convenient constructors for common message types, along with helper methods for writing arguments.
Additionally, the **Message** class automatically computes the checksum once all expected arguments are written.
Think of it as the **output class** of a LBP message.

Like the **Payload** class, **Message** is also templated by maximum capacity.
- `CmdMsg` is large enough for all commands that are not `cmd_file_chunk`
- `MaxMsg` is large enough to accommodate the largest possible output message.

(See `lbp/spec.h` for the definitions of these dimensions).

## Simulator (lbp-simulator)

The LightBurn Protocol is being developed simultaneously with a simple simulator. The simulator is a Qt Desktop application.
It is provided also as an sample of the `lbp` software library in action.

While the simulator *is* a desktop application and does make use of Qt and the C++ standard library, its architecture
adopts a layered approach. Usage of Qt and aspects of C++ that are not friendly to embedded contexts are limited
to the "upper" layers of the software - the main window, the debug logger, the transportation layer, and configuration storage.

The actual simulation loop written to be embedded-friendly, with no heap allocations and no use of Qt.
(See `firmwaresim.h/cpp`, `movement.h/cpp`). It's not actual firmware, but it is written to be separate and clean enough from the
desktop application concerns to serve as an example for `lbp` library usage.

### Simulator classes

- **MainWindow:** The Qt entrance point for the desktop application software. It displays the global log messages, connection status, and a simulation view.
- **SimView:** A widget displaying a simple view of the current laser position. It preserves cuts. (Press c to clear.)
- **Connection:** A Qt class that provides a TCP server for LightBurn to connect to. Includes a **Parser**.
- **FirmwareSim:** This is where the simulation `loop` is implemented. It parses **Payloads** from **Connection**, processes them, and sends output **Messages** back through the **Connection**.
- **MovementSim:** This is where the physical state of the machine is simulated. It is responsible for all movement and laser commands. It maintains its own **Queue** of **CmdPayloads** which it executes in order.
- **FileSystem:** This class is responsible for receiving, concatenating, and parsing files sent in `cmd_file_chunk`. As more file-related commands are implemented, its capabilites are expected to expand.
- **Configuration:** This class is responsible for processing commands related to `cfg_` messages. It stores configuration data between sessions using `json`.

## Work In Progress
This is very much an early look at the LightBurn Protocol - thank you for joining us! Here are the major items that are still incomplete in specification, simulator, and LightBurn LBP implemention, and represent the immediate priorities for next steps:

1. Pausing and Continuing Jobs
2. Raster engrave operations - these are still to be designed.
3. Framing operations - the messages are defined, but the functionality is not implemented.
4. Job-level settings - designed but not yet implemented.
5. Several design decisions regarding certain specific laser information queries and multiple laser offsets.
6. Design decisions - should gantry movements be separate commands from galvo movements? Should job movements get different commands than user-control movements?
7. More advanced and granular file operations.
8. Solidify common configuration - units, flag definitions, etc.

In addition, much of the implementation of specific configuration items is not yet implemented in the simulator.
(The movement component does not yet consider configuration items like workspace dimensions, default movement speeds, etc.)

Incompletely specified or implemented commands should be marked as such in `lbp/spec.h`, but please reach out if you suspect a mistake or omission.

## Contact

Stephen Edwards
Sr. Software Engineer
stephen.edwards@lightburnsoftware.com
