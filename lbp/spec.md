# SPEC.H

This document is meant to accompany the `spec.h` header and provide a guide for common `lbp` workflows.

## Overview
LBP is a transport-agnostic command-and-response binary protocol for controlling laser cutters and engravers.

LBP is also a work-in-progress. Some functionality has not yet been implemented, and the structure of some messages and workflows is still to be determined. Feedback and commentary is welcome.

LBP is intended to function over a variety of different transport layers, e.g. serial, TCP, UDP, websocket, etc. This specification assumes that the transport layer will deliver LBP messages quickly and in the correct order.

## Message Structure

```
+--------------+----------------+---------+------+-----------+----------+
| Magic        | Transaction ID | Command | Size | Arguments | Checksum |
| [4B]: "DRGN" | [2B]           | [2B]    | [2B] | [Size B]  | [2B]     |
+--------------+----------------+---------+------+-----------+----------+
 \____________ header (10 bytes) _______________/
```

- **Magic (4 bytes):** The 4-byte sequence `0x4452474E`, which is the ASCII encoding of `DRGN` (short for "dragon").
- **Transaction ID (2 bytes):** A 16-bit unsigned integer that pairs a request with its response. `0` means "no transaction ID". See [Transactions](#transactions).
- **Command (2 bytes):** The 16-bit command code. See `spec.h`.
- **Size (2 bytes):** A 16-bit unsigned integer encoding the size, in bytes, of the **Arguments** that immediately follow it. It may be `0`, and must not exceed `size_file_args`.
- **Arguments ("Size" bytes):** The command's argument data, if any.
- **Checksum (2 bytes):** A CRC-16 computed over the **Transaction ID, Command, Size, and Arguments** fields, in that order (everything after the Magic). See [Checksum](#checksum).

**IMPORTANT** All multi-byte fields are encoded in **Big-Endian Format**, except the Checksum (see below). (Helper functions are provided in `lbp/beio.h`)

### Checksum
The checksum is **CRC-16/X-25** (also known as CRC-16/IBM-SDLC):

| Parameter        | Value                                   |
|------------------|-----------------------------------------|
| Width            | 16 bits                                 |
| Polynomial       | `0x1021` (`0x8408` in reflected form)   |
| Initial value    | `0xFFFF`                                |
| Reflect in / out | yes / yes                               |
| Final XOR        | `0xFFFF`                                |
| Check value      | `0x906E` (the CRC of ASCII `123456789`) |

The checksum is sent **low byte first**: a CRC of `0x906E` appears on the wire as `6e 90`. This is the one exception to big-endian
byte order in a message. (Under review: it may change to big-endian, consistent with every other field.)
The reference implementation is `lbp/src/checksum.cpp`.

A message without arguments is therefore 12 bytes long. The largest permitted message is `size_file_msg` (512 bytes), which leaves room for 500 bytes of arguments.

See the `spec.h` header file for message dimension constants, command code definitions, and argument information.

## Simple Workflows

At a high level, LBP works as a command-and-response protocol:

1. **LightBurn sends commands.** These are compact, structured binary messages — instructions like "move to this position," "set laser power," "start a job," or "report your current state."

2. **The controller executes them and responds.** The manufacturer's firmware receives each message, interprets the command, carries out the operation on the hardware, and responds to LightBurn with another message. This response always contains the same command code **and the same Transaction ID** as the request, along with optional arguments in the case of a query.

In the case of commands that result in long-running operations, the firmware's response is an **acknowledgement** of the request.
It **begins** the operation and responds to the command **immediately** - **not** at the conclusion of the operation.

### Handshake

The Handshake confirms communication between LightBurn and the firmware, and exchanges the protocol version each side speaks.

**LightBurn Sends:**

```
+--------------+-------+-------+-------+-----------+----------+
| Magic        | TxID  | Cmd   | Size  | Arguments | Checksum |
| 44 52 47 4e  | 00 01 | 01 b8 | 00 02 | 00 0a     | cb 06    |
+--------------+-------+-------+-------+-----------+----------+
```

The command is `cmd_handshake` (`01 b8`), and its 2-byte argument is LightBurn's protocol version, `0x000A` (0.10). LightBurn has assigned this request Transaction ID `1`.

**Firmware Responds:**

```
+--------------+-------+-------+-------+-----------+----------+
| Magic        | TxID  | Cmd   | Size  | Arguments | Checksum |
| 44 52 47 4e  | 00 01 | 01 b8 | 00 02 | 00 0a     | cb 06    |
+--------------+-------+-------+-------+-----------+----------+
```

The firmware responds with the same command code and Transaction ID, followed by the protocol version **the firmware** speaks. Here both sides speak 0.10, so the response happens to be identical to the request.

#### Protocol Version

| Command          | Request Arguments            | Response Arguments            | Argument Size |
|------------------|------------------------------|-------------------------------|---------------|
| `cmd_handshake`  | uint16 LightBurn's version   | uint16 firmware's version     | 2             |

- The version is a 16-bit value: the **upper byte is the major** version and the **lower byte is the minor** version. `0x000A` is version 0.10; `0x0102` would be 1.2. The current version is `protocol_version` in `spec.h`.
- The firmware must **always** respond to a handshake with its own version, even if LightBurn's version differs. LightBurn decides whether the two are compatible, so a mismatch can be reported to the user clearly instead of appearing as a timeout.
- Compatibility (see `lbp/version.h`): major versions must match. Before 1.0, minor versions must match too, since any 0.x release may contain breaking changes. From 1.0 on, minor versions only add functionality, and LightBurn will not send commands that the firmware's minor version does not support.
- The handshake has no side effects, and may be sent at any time.

### Position Query

Here we use the command code `cmd_pos_x`, defined in `lbp/spec.h`, to ask for the current x position of the laser in micrometers. This request has Transaction ID `42` (`0x002a`):

**LightBurn Sends:**

```
+--------------+-------+-------+-------+----------+
| Magic        | TxID  | Cmd   | Size  | Checksum |
| 44 52 47 4e  | 00 2a | 81 01 | 00 00 | bf b0    |
+--------------+-------+-------+-------+----------+
```

**Firmware Responds:**

```
+--------------+-------+-------+-------+-------------+----------+
| Magic        | TxID  | Cmd   | Size  | Arguments   | Checksum |
| 44 52 47 4e  | 00 2a | 81 01 | 00 04 | 00 00 4e 20 | 9d 75    |
+--------------+-------+-------+-------+-------------+----------+
```

Our laser's x-axis position is 20 mm. The response carries the same command (`cmd_pos_x`: `81 01`) and Transaction ID as the request, and a single 4-byte argument `0x00004e20`, which is 20,000 micrometers. Size is therefore 4.

## Transactions

**Note**: Transaction IDs are new as of version 0.9.

The Transaction ID lets LightBurn match each response to the request that caused it, and lets the firmware recognize a request that LightBurn has retried.

### Rules for LightBurn
- LightBurn assigns Transaction IDs. They are opaque to the firmware: it must not interpret them beyond the rules below.
- IDs are assigned when a request is created, incrementing by one for each new request and wrapping from `0xFFFF` back to `1`. `0` is never assigned.
- When LightBurn **retries** a request (for example, after a timeout), it re-sends the request with the **same** Transaction ID.
- Consecutive distinct requests therefore always have distinct Transaction IDs.
- The first request LightBurn sends on every new connection is `cmd_handshake`. Because the handshake has no side effects, it is harmless if the firmware mistakes it for a retry of a request from a previous connection that happened to have the same ID.
- LightBurn never sends Transaction ID `0` over a connection. Only messages inside job files carry Transaction ID `0`.
- Messages written into job files always have Transaction ID `0`.

### Rules for the Firmware
- **Responses:** When responding to a request, the firmware must copy the request's Transaction ID into the response unchanged, including `0`.
- **Behavior does not depend on the ID:** Whether and how the firmware responds to a command depends only on the command. A request with Transaction ID `0` receives the same response as any other; the response simply carries ID `0`.
- **Unsolicited messages:** Messages the firmware originates on its own, rather than in response to a request, must have Transaction ID `0`.
- **Job files:** Transaction IDs of messages read from a job file must be ignored. Job file commands are executed, not responded to.
- **Duplicate requests:**: The firmware should use the Transaction ID to identify duplicate requests. Sequential, identical Transaction IDs indicate that Lightburn is retrying a request. The firmware should:
  - respond again, with the same command code and Transaction ID, and
  - **not** repeat the command's side effects. A retried relative move must not move the laser a second time, and a retried file chunk must not be written twice.

Queries have no side effects, so the firmware may simply answer a retried query again with current values.

This applies only to requests the firmware **executed**. A request the firmware rejected with `cmd_nack` had no side effects, so a retry of it must be evaluated again from scratch. The machine's state may have changed in the meantime (for example, a full queue may have drained). Consequently, the firmware must only send `cmd_nack` for a request it did not act on at all.

**Note**
message.h contains the helper function `respondTo(...)` to assist firmware authors in adopting this change.

### Unsolicited Messages
The firmware may send messages that are not responses to any request, at any time. These always carry Transaction ID `0`, and LightBurn never responds to them.
LightBurn must ignore any unsolicited message whose command it does not recognize. This allows future versions of LBP to add new notifications without breaking existing LightBurn versions.

Currently, the firmware sends `cmd_job_end` unsolicited when a job finishes, and `cmd_event` (see [Events](#events)).

## Events
The firmware reports things that *happen* with `cmd_event`, sent unsolicited (Transaction ID `0`).

| Command     | Arguments                                   | Arg Size |
|-------------|---------------------------------------------|----------|
| `cmd_event` | uint8 kind, uint16 code, uint32 detail       | 7        |

| Kind            | Value  | Meaning | `code` | `detail` |
|-----------------|--------|---------|--------|----------|
| `event_fault`   | `0x01` | The machine can't operate until the fault is cleared. `state_fault` is set while it lasts. | `err_*` reason | Context (e.g. which axis), or 0 |
| `event_warning` | `0x02` | Something is wrong, but the machine is still operable (e.g. a source temperature warning). | Warning code | Context, or 0 |
| `event_input`   | `0x03` | An operator input happened. | `input_start`, `input_frame`, or `input_stop` | 0 |
| `event_result`  | `0x04` | A long-running request has finished. | `0` on success, otherwise an `err_*` code (`err_canceled` if it was stopped) | Transaction ID of the original request |

`0` is not a valid kind. LightBurn ignores events of a kind it does not recognize, and handles unrecognized codes within a known kind
generically (for example, by showing an unknown fault to the user).

**Events are notifications, not the source of truth.** They are never acknowledged or retried, so one may be lost.
Anything LightBurn must not miss is also reflected in the state flags (see [Machine State](#machine-state)), and if an event and the
state flags ever disagree, the state flags win.

**Conditions vs. happenings.** A condition that is true or false right now (the door is open, the pedal is held) is a state flag.
Something that happens at a moment (the pedal was pressed, homing finished) is an event. A pedal tap between two state queries
would be missed, so LightBurn acts on `event_input`, not on `state_pedal_down`.

**Results.** The firmware sends `event_result` only after it has sent the response to the original request, and only for long-running
requests (such as homing, focusing, and executing a job), never for each move within a job. Requests with Transaction ID `0` (including
commands in a job file) cannot be matched to a result.

The firmware should debounce inputs and limit repeated events, so that a flapping switch cannot flood the connection.

## Errors

When the firmware cannot carry out a request, it responds with `cmd_nack` **instead of** the normal response.

| Command    | Arguments                                                | Arg Size |
|------------|----------------------------------------------------------|----------|
| `cmd_nack` | uint16 rejected command, uint16 error code, uint32 detail | 8        |

- The `cmd_nack` carries the **Transaction ID of the rejected request**, so LightBurn can match it like any other response.
- The **rejected command** is the command code of the request being rejected.
- The **error code** is one of the `err_*` values in `spec.h`.
- The **detail** is defined per error code (see below), and is `0` if unused.
- A `cmd_nack` means the firmware did **not** act on the request at all. See [Duplicate requests](#rules-for-the-firmware): a retry of a rejected request is evaluated again from scratch.

For example, a `cmd_jog_to_xy` request with Transaction ID `7` that carries only 4 bytes of arguments (instead of the expected 8) is rejected with `err_invalid_length`, with the expected size as detail:

```
+--------------+-------+-------+-------+-------------------------------+----------+
| Magic        | TxID  | Cmd   | Size  | Arguments                     | Checksum |
| 44 52 47 4e  | 00 07 | 0b ad | 00 08 | 67 03  01 02  00 00 00 08     | 94 fd    |
+--------------+-------+-------+-------+-------------------------------+----------+
```

### Messages that can't be rejected
A `cmd_nack` can only be sent for a message that arrived intact, because it must carry that message's Transaction ID.
A message with a bad checksum or an invalid size is **dropped silently**: its Transaction ID may be the corrupted part.
LightBurn will time out and retry the request with the same Transaction ID.

Commands read from a job file are not responded to, so they cannot be rejected with `cmd_nack` either.

### Error Codes
Codes are loosely grouped by their upper byte: `0x01xx` for problems with the request itself, `0x02xx` for conditions on the machine, and `0x04xx` for files.
`0xD0xx` is reserved for vendor-specific codes.

| Code | Name | Meaning | Detail |
|------|------|---------|--------|
| `0x0101` | `err_invalid_cmd`    | The command code is not recognized or not implemented by this firmware. Retrying will not help. | 0 |
| `0x0102` | `err_invalid_length` | The argument size is wrong for this command. | Expected argument size |
| `0x0103` | `err_invalid_arg`    | An argument value is invalid or out of range. | Index of the bad argument (0-based) |
| `0x0201` | `err_queue_full`     | The command buffer is full; retry shortly. | 0 |
| `0x0202` | `err_wrong_state`    | A recognized command that is not allowed in the current state (e.g. `cmd_continue` when not paused, `cmd_execute` with no file loaded). | `state_*` flags |
| `0x0203` | `err_fault`          | Refused because a fault is active (see `state_fault`). | 0 |
| `0x0204` | `err_canceled`       | Stopped (by `cmd_stop` or an operator stop) before it finished. Used in `event_result`. | 0 |
| `0x0401` | `err_storage_full`   | Not enough space for the file. | 0 |
| `0x0402` | `err_transfer`       | `cmd_file_chunk` or `cmd_file_end` without a `cmd_file_begin`, or out of sequence. | 0 |
| `0x0403` | `err_invalid_file`   | The received file failed validation: its checksum doesn't match, its length doesn't match its header, or its contents are malformed. | 0 |
| `0x0404` | `err_invalid_header` | The received file's header is not recognized or not supported: wrong magic, unsupported version, or values this machine can never accept. | Byte offset of the offending header field |
| `0x0405` | `err_file_mismatch`  | A valid stored file doesn't fit the current configuration, such as a correction table whose field size differs from `cfg_field_size_x/y`. Sent when the file is applied (`cmd_galvo_correction`), not when it is received. | Byte offset of the header field that doesn't match |
| `0xFFFF` | `err_unknown`        | Catch-all. | 0 |

**Note**
message.h contains the helper function `nackTo(...)` to assist firmware authors in adopting this change.

## Command Code Design
LBP command codes are unsigned 16-bit integers. We do not require all 65,536 possible values,
so we take advantage of the large address space for organization and readability.

We assume that most humans who need to read LBP code will do so while programming or debugging and will
thus mostly interact with LBP as hexadecimal codes.

Thus, we break the 16 bits of a command code into four 4-bit "nibbles" and compose command codes from
readable combinations thereof.

### Most-Significant Nibble Codes
The most significant nibble (**MSN**) represents the broad command category.

The following three nibbles can also be used to narrow down command category,
enabling the construction of commands by bitwise-OR operations.

The purpose of this compositional approach is two-fold:
1. To aid in readability when visually examining LBP data in a binary or hex viewer.
2. To make the protocol easier and more fun to design.

#### MSN Categories
- `0x0000`: Fundamentals - beginnings, endings, basics.
- `0x1000`: Laser commands (1 = "L", for "Laser")
- `0x2000`: Unused
- `0x3000`: Unused
- `0x4000`: Files and File System
- `0x5000`: Non-persistent Settings (5 = "S", for "Settings")
- `0x6000`: Movement commands (6 = "G", for "Go")
- `0x7000`: Tool commands (7 = "T", for "Tool")
- `0x8000`: Observable State (8 rhymes with "State")
- `0x9000`: Unused
- `0xA000`: Unused
- `0xB000`: Unused
- `0xC000`: Common Configuration (C for "Configuration")
- `0xD000`: Reserved for Vender-specific Configuration
- `0xE000`: Reserved for Extended Future Capabilities
- `0xF000`: Unused

### Axis flags
Classic flag-based composition isn't always appropriate, but in the cases of positional axes it makes a lot of sense for readability.
When appropriate, the LSB (Least Significant Nibble) of certain commands may consist of:
- `0x0001`: X Axis
- `0x0002`: Y Axis
- `0x0004`: Z Axis
- `0x0008`: A Axis
- `0x0010`: B Axis
- `0x0020`: C Axis
- `0x0040`: U Axis
- `0x0080`: V Axis

or some bitwise-OR combinations thereof.

#### Notes on Axes
Most laser engravers will have at maximum three or four degrees of freedom.
However, some fancier machines may have "flying galvonometer" operations, or specialized rotary or feed mechanisms.
Thus we reserve 8 axis flags, enabling terse yet expressive commands for simultaneous movements via bitwise flag
combinations, with reasonable headroom for the future.
Generally speaking, we expect most machines to use X,Y,Z, and possibly either A or U:
XYZ for primary positional control, and either A for rotary or U for feed.

We expect XYZABC configurations to be utilized for true 6-axis positioning, such as flying galvonometer systems.

That said, we plan to allow full configuration of all axes. Each will be configurable to accept values of either:

- `unit_micrometers`, for linear systems.
- `unit_millidegrees`, for rotational systems.
- `unit_steps`, for stepper motors.

These units will inform the meaning of arguments in movement commands, position queries, and configurations for that axis.
(See below for configuration)

For now, all XYZ and UV are assumed to use micrometers, and ABC are assumed to use millidegrees.

### Composition Example:
Consider the command `cmd_cut_abs_xy` ("Move to absolute X and Y position while cutting"):

`0x6B03` is composed of:
- `0x6000`: MSN for "movement."
- `0x0B00`: Indicates an "absolute cut" move (within the "movement" category).
- `0x0003`: Flags for both X and Y axes are set.

Put together, a developer inspecting LBP messages on the wire can read this quite easily.

## Moving the Laser
LBP provides many categories of movement commands. This is to give the firmware **context** for the movement.
The first broad categorical distinction is whether the move is an "operator" move or a "programmed" move.

- **Operator** moves originate outside the context of a programmed job. The user is pressing buttons or other controls
in LightBurn to move the laser in real time.
- **Programmed** moves occur in the context of a job.

Within these categories are further distinctions. Both **operator** and **programmed** moves may be either **absolute** or **relative**,
with **operator** moves also having the option of being **continuous**. **Programmed** moves provide additional context of **cut** vs **travel**,
which may effect how the firmware plans the movement. In addition, the **homing** is considered an **operator** move.
So, all together, we have:

**Operator**
- **home**: move the laser to its **home** position on any or all axes.
- **jog start**: start a continuous movement on a particular axis.
- **jog stop**: stop a continuous movement on a particular axis.
- **jog step**: step the laser by a certain distance from its current position. Think of this as a "relative operator move."
- **jog_to**: move the laser directly to a certain absolute position. Think of this as an "absolute operator move."

**Programmed**:
- **relative travel**: move the laser a specified distance from its current position. It is **not expected** that the laser is cutting.
- **absolute travel**: move the laser to a specific position (relative to the origin set by `cmd_cut_from`). It is **not expected** that the laser is cutting.
- **relative cut**: move the laser a specified distance from its current position. The laser **is expected** to be active and cutting.
- **absolute cut**: move the laser to a specific position (relative to the origin set by `cmd_cut_from`).The laser **is expected** to be active and cutting.
- **dwell**: perform no movement for a specified number of microseconds.

These distinctions exist to provide the firmware with as much **context** as possible for the command. They also allow for more granular
configuration of default speeds and movement behaviors. The intention is to help make LBP-capable firmware easier to write and maintain.

**Note**: These **programmed movement** commands **do not** automatically turn the laser on or off. The user is expected to program those commands separately,
prior to the movement commands.

### Movement Speed
There are three families of speed command, one for each kind of move:

| Family       | Commands               | Governs                                                    | Galvo meaning |
|--------------|------------------------|------------------------------------------------------------|---------------|
| Cut speed    | `cmd_cut_speed_*`      | programmed cut moves (`cmd_cut_*`)                         | mark speed    |
| Travel speed | `cmd_travel_speed_*`   | programmed travel moves (`cmd_travel_*`)                   | jump speed    |
| Jog speed    | `cmd_jog_speed_*`      | operator moves (`cmd_jog_start_*`, `cmd_jog_step_*`, `cmd_jog_to_*`) | —   |

Each family's command code mirrors the code of the moves it governs: cut speeds are `0x5Axx` (cut moves are `0x6Axx`/`0x6Bxx`),
travel speeds are `0x58xx` (travel moves are `0x68xx`/`0x69xx`), and jog speeds are `0x56xx` (jog moves are `0x66xx`/`0x67xx`).
The low byte carries axis flags, as for movement commands.

Every speed command takes a single int32 argument: the speed in axis units per second (micrometers/second for linear axes),
whatever axis flags the command carries.

- Each speed is **sticky**: it applies to every following move of its kind until it is changed. Setting one family never
  changes another, so a jog never disturbs the speed of a job's cuts, and a travel speed never changes the cut speed.
- If a speed has not been set since the last power cycle, the firmware uses its configured default for that kind of move.
- **Homing** ignores all speed commands. Each axis homes at its configured home speed (`cfg_*_home_speed`).

The table below lists the cut-speed commands. The travel and jog families define the same set of axis combinations,
with `travel` or `jog` in place of `cut`.

| Command           | Arguments (speed in axis units / second)  | Arg Size       |
|-------------------|-------------------------------------------|----------------|
| `cmd_cut_speed_xy`| int32 movement speed in the xy plane      | 4              |
| `cmd_cut_speed_x` | int32 movement speed along the x axis*    | 4              |
| `cmd_cut_speed_y` | int32 movement speed along the y axis*    | 4              |
| `cmd_cut_speed_z` | int32 movement speed along the z axis     | 4              |
| `cmd_cut_speed_a` | int32 movement speed along the a axis     | 4              |
| `cmd_cut_speed_b` | int32 movement speed along the b axis     | 4              |
| `cmd_cut_speed_c` | int32 movement speed along the c axis     | 4              |
| `cmd_cut_speed_u` | int32 movement speed along the u axis     | 4              |
| `cmd_cut_speed_v` | int32 movement speed along the v axis     | 4              |
| `cmd_cut_speed_ab`| int32 movement speed in the ab plane      | 4              |

\* The single-axis x and y commands are provided for machines that cannot move diagonally,
or whose x and y axis movement mechanisms meaningfully differ.

### Movement Commands
Movement commands are composed of valid movement categories in the MSB and bitwise combinations of axis flags in the LSB.
Common commands will be listed in the tables below, but any such combination is a valid command according to the specification.

A movement command will have one 32-bit integer argument for each axis flag that is set in the command lsb, in the following order:
`x`, `y`, `z`, `a`, `b`, `c`, `u`, `v`.

For example the command `0x6B41` would have two 32-bit arguments - X position followed by U position.
In practice, such unusual commands would be vanishingly unlikely to originate from Lightburn, but are valid for the purposes of this specification.

See the tables below for examples of typical movement commands. The tables are not exhaustive, but should cover common cases.

#### TODO: Axis unit configuration
A, B, C are conventionally rotational axes. A design decision must be made about what units to send along.

### Operator Movements
These commands are expected to originate from a movement panel in the user interface. The user is issuing movement commands to the machine live
and expects the machine to respond in real time.

#### Movement Command Response
The firmware responds to all operator movement commands upon receipt with a message consisting of the
same command and no arguments. For example, the response to a `cmd_jog_to_xy` message is:

| Command           | Arguments | Arg Size       |
|-------------------|-----------|----------------|
| `cmd_jog_to_xy`   | None      | 0              |

This response is sent immediately upon receipt of the command, before the move completes.
If the firmware cannot perform the move at all (for example, because a fault is active),
it responds with `cmd_nack` instead. See [Errors](#errors).
The firmware is not required to send any positional update upon move completion.

#### Go To (Absolute Operator Move)
Go To commands move the laser to the specified position along the command's indicated axis.
Each command includes one 32-bit integer argument corresponding to the desired axis position in micrometers.

| Command           | Arguments (positions in axis units)  | Arg Size       |
|-------------------|--------------------------------------|----------------|
| `cmd_jog_to_x`    | int32 X                              | 4              |
| `cmd_jog_to_y`    | int32 Y                              | 4              |
| `cmd_jog_to_z`    | int32 Z                              | 4              |
| `cmd_jog_to_a`    | int32 A                              | 4              |
| `cmd_jog_to_b`    | int32 B                              | 4              |
| `cmd_jog_to_c`    | int32 C                              | 4              |
| `cmd_jog_to_u`    | int32 U                              | 4              |
| `cmd_jog_to_v`    | int32 V                              | 4              |
| `cmd_jog_to_xy`   | int32 X, int32 Y                     | 8              |
| `cmd_jog_to_xyz`  | int32 X, int32 Y, int32 Z            | 12             |
| `cmd_jog_to_xyza` | int32 X, int32 Y, int32 Z, int32 A   | 16             |
| `cmd_jog_to_abc`  | int32 A, int32 B, int32 C            | 12             |

**Note**: Go To command coordinates are to be interpreted relative to the machine's absolute zero.

#### Jog Step (Relative Operator Move)
Jog Step commands are named such in order to be familiar to machine operators. They are really
just relative operator moves. Thus, the argument is to be one 32-bit argument per indicated axis
representing the distance to move from the current location along that axis.
Usually the operator will jog only one axis at a time, but if your device allows planar motion then
Lightburn will expose those commands.

| Command          | Arguments (distances in axis units) | Arg Size       |
|------------------|-------------------------------------|----------------|
| `cmd_jog_step_x` | int32 X                             | 4              |
| `cmd_jog_step_y` | int32 Y                             | 4              |
| `cmd_jog_step_z` | int32 Z                             | 4              |
| `cmd_jog_step_a` | int32 A                             | 4              |
| `cmd_jog_step_b` | int32 B                             | 4              |
| `cmd_jog_step_c` | int32 C                             | 4              |
| `cmd_jog_step_u` | int32 U                             | 4              |
| `cmd_jog_step_v` | int32 V                             | 4              |
| `cmd_jog_step_xy`| int32 X, int32 Y                    | 8              |
| `cmd_jog_step_ab`| int32 A, int32 B                    | 8              |

#### Jog Start/Stop (Continuous Operator Move)
Continuous movements are different from Absolute (Goto) or Relative (Jog Step) moves in that they have no distance or positional arguments.
In fact, they have **no arguments at all**. They consist simply of "start" or "stop" commands for movement along a specified axis.

Upon receipt of a "start" command, the firmware is expected to start moving the laser along the axis and direction encoded in the command.
The firmware is expected to continue moving the laser until either the respective "stop" command is received, a higher-priority
or contradictory command is received, or a fault occurs (such as contact with a machine boundary).

| Command               | Arg Size       | Description                            |
|-----------------------|----------------|----------------------------------------|
| `cmd_jog_start_pos_x` | 0              | Start moving along the positive X axis |
| `cmd_jog_stop_pos_x`  | 0              | Stop moving along the positive X axis  |
| `cmd_jog_start_neg_x` | 0              | Start moving along the negative X axis |
| `cmd_jog_stop_neg_x`  | 0              | Stop moving along the negative X axis  |
| `cmd_jog_start_pos_y` | 0              | Start moving along the positive Y axis |
| `cmd_jog_stop_pos_y`  | 0              | Stop moving along the positive Y axis  |
| `cmd_jog_start_neg_y` | 0              | Start moving along the negative Y axis |
| `cmd_jog_stop_neg_y`  | 0              | Stop moving along the negative Y axis  |
| `cmd_jog_start_pos_z` | 0              | Start moving along the positive Z axis |
| `cmd_jog_stop_pos_z`  | 0              | Stop moving along the positive Z axis  |
| `cmd_jog_start_neg_z` | 0              | Start moving along the negative Z axis |
| `cmd_jog_stop_neg_z`  | 0              | Stop moving along the negative Z axis  |
| `cmd_jog_start_pos_a` | 0              | Start moving along the positive A axis |
| `cmd_jog_stop_pos_a`  | 0              | Stop moving along the positive A axis  |
| `cmd_jog_start_neg_a` | 0              | Start moving along the negative A axis |
| `cmd_jog_stop_neg_a`  | 0              | Stop moving along the negative A axis  |
| `cmd_jog_start_pos_b` | 0              | Start moving along the positive B axis |
| `cmd_jog_stop_pos_b`  | 0              | Stop moving along the positive B axis  |
| `cmd_jog_start_neg_b` | 0              | Start moving along the negative B axis |
| `cmd_jog_stop_neg_b`  | 0              | Stop moving along the negative B axis  |
| `cmd_jog_start_pos_c` | 0              | Start moving along the positive C axis |
| `cmd_jog_stop_pos_c`  | 0              | Stop moving along the positive C axis  |
| `cmd_jog_start_neg_c` | 0              | Start moving along the negative C axis |
| `cmd_jog_stop_neg_c`  | 0              | Stop moving along the negative C axis  |
| `cmd_jog_start_pos_u` | 0              | Start moving along the positive U axis |
| `cmd_jog_stop_pos_u`  | 0              | Stop moving along the positive U axis  |
| `cmd_jog_start_neg_u` | 0              | Start moving along the negative U axis |
| `cmd_jog_stop_neg_u`  | 0              | Stop moving along the negative U axis  |
| `cmd_jog_start_pos_v` | 0              | Start moving along the positive V axis |
| `cmd_jog_stop_pos_v`  | 0              | Stop moving along the positive V axis  |
| `cmd_jog_start_neg_v` | 0              | Start moving along the negative V axis |
| `cmd_jog_stop_neg_v`  | 0              | Stop moving along the negative V axis  |

This specification neither forbids nor requires the capability for simultaneous axis jogging -
it simply provides the command definitions.
If the user sends `cmd_jog_x_pos_start` immediately followed by `cmd_jog_y_pos_start`, and
your hardware is capable of said movement, we leave the resulting behaviour to your discretion.
It is reasonable to either cancel the X jog and begin a Y Jog, begin jogging diagonally, or cancel both requests.

### Programmed Movements
These commands are expected to appear in the context of an cutting/engraving job.

#### Cut
**Cut** commands are programmed moves that signal to the firmware that the laser is expected to be on and cutting during the movement.
Crucially though, these commands **do not turn on the laser**. They simply tell the firmware to move as if the laser is cutting.
Cut commands can be either relative or absolute. The following tables list the most probably examples of cut commands.

#### Absolute Cut
| Command               | Arguments (positions in axis units) | Arg Size       |
|-----------------------|-------------------------------------|----------------|
| `cmd_cut_abs_x`       | int32 X                             | 4              |
| `cmd_cut_abs_y`       | int32 Y                             | 4              |
| `cmd_cut_abs_a`       | int32 A                             | 4              |
| `cmd_cut_abs_b`       | int32 B                             | 4              |
| `cmd_cut_abs_xy`      | int32 X, int32 Y                    | 8              |
| `cmd_cut_abs_ab`      | int32 A, int32 B                    | 8              |

Absolute moves in X and Y have the option to be sent relative to a specified origin.
This origin is set with the command `cmd_cut_from` and should be set as part of the **Job Header** (see below).

#### Relative Cut
Just like Jog Step, Relative Cut move commands move the laser by a specified distance (or **delta**) from its previous position at the start of the move.
Each command includes one 32-bit integer argument corresponding to the desired axis delta in micrometers.

| Command               | Arguments (distances in axis units ) | Arg Size       |
|-----------------------|--------------------------------------|----------------|
| `cmd_cut_rel_x`       | int32 X                              | 4              |
| `cmd_cut_rel_y`       | int32 Y                              | 4              |
| `cmd_cut_rel_a`       | int32 A                              | 4              |
| `cmd_cut_rel_b`       | int32 B                              | 4              |
| `cmd_cut_rel_xy`      | int32 X, int32 Y                     | 8              |
| `cmd_cut_rel_ab`      | int32 A, int32 B                     | 8              |

#### Rapid
**Rapid** Commands are programmed moves that signal to the firmware that the laser is **not expected** to be
on and cutting during the move. Rapid moves are expected to be faster than cuts.

#### Absolute Rapid
Just like Absolute Cuts, Absolute Rapid arguments are axis positions **relative to the jog origin**.
Job Origin is set in the Job Header using the `cmd_cut_from` message.

| Command                 | Arguments (positions in axis units)                   | Arg Size       |
|-------------------------|-------------------------------------------------------|----------------|
| `cmd_travel_abs_x`      | int32 X                                               | 4              |
| `cmd_travel_abs_y`      | int32 Y                                               | 4              |
| `cmd_travel_abs_z`      | int32 Z                                               | 4              |
| `cmd_travel_abs_a`      | int32 A                                               | 4              |
| `cmd_travel_abs_b`      | int32 B                                               | 4              |
| `cmd_travel_abs_c`      | int32 C                                               | 4              |
| `cmd_travel_abs_u`      | int32 U                                               | 4              |
| `cmd_travel_abs_v`      | int32 V                                               | 4              |
| `cmd_travel_abs_xy`     | int32 X, int32 Y                                      | 8              |
| `cmd_travel_abs_xyz`    | int32 X, int32 Y, int32 Z                             | 12             |
| `cmd_travel_abs_xyza`   | int32 X, int32 Y, int32 Z, int32 A                    | 16             |
| `cmd_travel_abs_xyzu`   | int32 X, int32 Y, int32 Z, int32 U                    | 16             |
| `cmd_travel_abs_xyzabc` | int32 X, int32 Y, int32 Z, int32 A, int32 B, int32 C  | 24             |

#### Relative Rapid
| Command                 | Arguments (distnaces in axis units)                   | Arg Size       |
|-------------------------|-------------------------------------------------------|----------------|
| `cmd_travel_rel_x`      | int32 X                                               | 4              |
| `cmd_travel_rel_y`      | int32 Y                                               | 4              |
| `cmd_travel_rel_z`      | int32 Z                                               | 4              |
| `cmd_travel_rel_a`      | int32 A                                               | 4              |
| `cmd_travel_rel_b`      | int32 B                                               | 4              |
| `cmd_travel_rel_c`      | int32 C                                               | 4              |
| `cmd_travel_rel_u`      | int32 U                                               | 4              |
| `cmd_travel_rel_v`      | int32 V                                               | 4              |
| `cmd_travel_rel_xy`     | int32 X, int32 Y                                      | 8              |
| `cmd_travel_rel_xyz`    | int32 X, int32 Y, int32 Z                             | 12             |
| `cmd_travel_rel_xyza`   | int32 X, int32 Y, int32 Z, int32 A                    | 16             |
| `cmd_travel_rel_xyzu`   | int32 X, int32 Y, int32 Z, int32 U                    | 16             |
| `cmd_travel_rel_xyzabc` | int32 X, int32 Y, int32 Z, int32 A, int32 B, int32 C  | 24             |

#### Dwell
While not technically a "movement", "dwell" commands the laser to remain in place for a specified number of microseconds.
This can occur whether the laser is on or off.

| Command     | Arguments (code)   | Arg Size       |
|-------------|--------------------|----------------|
| `cmd_dwell` | int32 microseconds | 4              |

## Controlling the Laser
Your machine may have more than one laser tube. All laser control commands have 1-byte laser index argument
in addition to any respective numerical arguments. This index is `0`-based, with index `0` corresponding to the first laser,
`1` to the second, etc.

| Command               | Arguments                                  | Arg Size       |
|-----------------------|--------------------------------------------|----------------|
| `cmd_laser_enable`    | uint8 laser index                          | 1              |
| `cmd_laser_disable`   | uint8 laser index                          | 1              |
| `cmd_laser_power_max` | uint8 laser index, int16 max power (%)*    | 3              |
| `cmd_laser_power_min` | uint8 laser index, int16 min power (%)*    | 3              |
| `cmd_laser_freq`      | uint8 laser index, int32 PWM frequency (Hz)| 5              |

These laser settings are expected to persist until the respective commands are sent again.

### \*Note: Percentages in LBP
All percentages in LBP are represented as unsigned 16-bit integer arguments, where each integer value is 1/65535 of 100%, or 1/655.35 %.
For example, 100% is represented as `65535`, 50% as `32768`, and 1% as `655`.

### Enabling Laser Tubes
Before embarking on a cut, LB will send `cmd_laser_enable` (and possibly `cmd_laser_disable`) messages
to indicate to the firmware that these lasers are intended to be used soon.

### Laser Power
Laser power for a cut is sent as a percentage of the laser's configured maximum output power.
The minimum and maximum output power for each laser involved in a cut should be sent before turning the laser on and moving.

### Laser Frequency (PWM)
In addition to power, it is also necessary to set the laser's PWM frequency. This is given in int32 Hz.

### Turning the Laser On
Once the laser settings have been sent, it is still necessary to turn the enabled lasers on and off.

| Command         | Arguments        | Arg Size       |
|-----------------|------------------|----------------|
| `cmd_laser_on`  | uint8 laser index | 1              |
| `cmd_laser_off` | uint8 laser index | 1              |

### Guide and Focus Lights
Many machines have low-power pointers in addition to their lasers. These are **not** lasers in the LBP sense:
they have no power or frequency, are never enabled with `cmd_laser_enable`, and are typically safe to use with the lid open.

| Command               | Arguments        | Arg Size |
|-----------------------|------------------|----------|
| `cmd_guide_light_on`  | uint8 laser index | 1        |
| `cmd_guide_light_off` | uint8 laser index | 1        |
| `cmd_focus_light_on`  | none             | 0        |
| `cmd_focus_light_off` | none             | 0        |

**Guide light:** an aiming beam that shows where a laser source's beam will land, such as a galvo's red dot
(which travels through the mirrors), or a gantry's pointer. On fiber sources it is often built into the source itself,
so the laser index selects *which source's* aiming beam.

If your machine has a single aiming beam shared by all sources (an external red diode, for example),
implement index `0` only, and reply to any other index with `cmd_nack` and `err_invalid_arg` (detail `0`).
LightBurn will then use index `0` regardless of which source is selected.

**Focus light:** a separate pointer (often two, angled to meet at the focal distance) used to set the focus height.
It is never steered, so it takes no arguments.

Firmware without one of these lights should reply to its commands with `cmd_nack` and `err_invalid_cmd`.

### Cut Type
This command is a work-in-progress, intended to give the firmware information about the upcoming cuts that might be helpful in motion planning.

| Command         | Arguments    | Arg Size       |
|-----------------|--------------|----------------|
| `cmd_cut_type`  | int16 value  | 2              |

Where value may be:
- `cut_type_normal`: a normal cut, no additional information encoded.
- `cut_type_scan_uni_x`: a unidirectional scan along the x axis.
- `cut_type_scan_uni_y`: a unidirectional scan along the y axis.
- `cut_type_scan_bi_x`: a bidirectional scan along the x axis.
- `cut_type_scan_bi_y`: a bidirectional scan along the y axis.

## Making a Cut
Generally, cuts or engravings will be programmed in the following pattern:
1. Settings for the cut, if they differ from those for the previous cut, are sent with respective commands - lasers are enabled or disabled, laser power and frequency are set, movement speed is set, etc.
2. If the laser is not already positioned at the start of the cut, a `cmd_laser_off` is sent, followed by and `cmd_travel_xy`.
3. `cmd_laser_on` and `cmd_cut_xy` are sent, turning on the laser(s) and performing the cut.

## Raster Engraving
To aid in motion planning, LBP offers a special command for encoding raster lines.

| Command             | Arguments                                         | Arg Size     |
|---------------------|---------------------------------------------------|--------------|
| `cmd_raster_power`  | uint8 laser index, 0 <= N <= 15 uint16 power values| 1 + 2N       |

If the firmware receives this command, the next `cmd_cut_x`, `cmd_cut_y`, or `cmd_cut_xy` should be assumed to be a raster line.
This `cmd_cut` will engrave N "pixels" of equal length using the power settings received in the `cmd_raster_power` message.
The laser is to perform the movement at the set speed, changing output power as it moves to engrave all N pixel values in one pass.

## Jobs
We use the term "job" to refer to a series of programmed commands that encode a complete cutting/engraving task for the laser.
LBP uses commands with `_begin` and `_end` as bookend-style tags to indicate the context of these messages.

| Command                | Arg Size       | Description                                                     |
|------------------------|----------------|-----------------------------------------------------------------|
| `cmd_job_begin`        | 0              | Marks the start of job commands.                                |
| `cmd_job_header_begin` | 0              | Marks the start of commands in the job header.                  |
| `cmd_job_header_end`   | 0              | Marks the end of the job header.                                |
| `cmd_job_body_begin`   | 0              | Marks the start of commands that make up the action of the job. |
| `cmd_job_body_end`     | 0              | Marks the end of commands that make up the action of the job.   |
| `cmd_job_end`          | 0              | Marks the end of job commands.                                  |


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

### The Job Header

The Job header is a place to put settings that apply to the entire job.
So far, this consists of:

#### Job Cut Origin
In the context of a job, absolute moves in X and Y have the option to be sent relative to a specified origin.

| Command           | Arguments (code) | Arg Size       |
|-------------------|------------------|----------------|
| `cmd_cut_from`    | int8 code        | 1              |

This origin, represented as a single-byte argument, can be either:

- `cut_from_user_origin`: The User Origin, specified using configuration commands (see below).
- `cut_from_current_position`: The laser's XY position at the start of the job.
- `cut_from_absolute`: The laser's machine-specified `(0, 0)`.

#### Job Bounds
LBP provides the following commands to inform the firmware of the boundaries of the job in absolute coordinates calculated from the previously-specified job origin.

| Command               | Arguments (μm)           | Arg Size       |
|-----------------------|--------------------------|----------------|
| `cmd_bounds_min_x`    | int32 minimum x position | 4              |
| `cmd_bounds_max_x`    | int32 maximum x position | 4              |
| `cmd_bounds_min_y`    | int32 minimum y position | 4              |
| `cmd_bounds_max_y`    | int32 maximum y position | 4              |
| `cmd_bounds_min_z`    | int32 minimum z position | 4              |
| `cmd_bounds_max_z`    | int32 maximum z position | 4              |
| `cmd_bounds_min_u`    | int32 minimum u position | 4              |
| `cmd_bounds_max_u`    | int32 maximum u position | 4              |

### The Job Body
This is where the action of the job is programmed. This will mostly consist of:
- commands for applying laser settings
- commands for applying cut settings
- commands for moving and cutting

### Files
A LBP **file** is a block of bytes transferred between LightBurn and the firmware. Every file has a **type**,
which says what its contents are:

| File Type              | Value  | Contents                                                        |
|------------------------|--------|-----------------------------------------------------------------|
| `file_type_job`        | `0x01` | A job: a list of LBP messages (see below). `cmd_execute` runs it. |
| `file_type_correction` | `0x02` | A galvo lens correction table: an `.lbpcor` file (see [Lens Correction Files](#lens-correction-files)). |

`0` is not a valid file type. Firmware that does not support a file type rejects it with `cmd_nack` and
`err_invalid_arg` (detail `0`, the file type argument).

A **job file** is simply a concatenated list of valid and complete LBP messages.
(Including the magic, Transaction ID, command, size, and checksum fields.)
Thus, the bytes in a job file can be parsed using the same algorithms as bytes coming in off the wire.
Messages in a job file always have Transaction ID `0`.

### Receiving a File
A **file** is sent with the following commands:

| Command          | Arguments                                     | Arg Size              |
|------------------|-----------------------------------------------|-----------------------|
| `cmd_file_begin` | uint8 file type, int32 file size in bytes     | 5                     |
| `cmd_file_chunk` | Slice of file content                         | 0 - `size_file_chunk` |
| `cmd_file_end`   | None                                          | 0                     |

Files are sent in messages of the maximum permitted size.
Firmware is expected to use the arguments of `cmd_file_begin` to prepare storage for the incoming file.
It will then concatenate the arguments of all `cmd_file_chunk` messages in the order they are received.
Only one file transfer may be in progress at a time.

### TODO: File chunk ordering
It may prove necessary to add a "sequence number" or "file offset" argument to each `cmd_file_chunk` message.

### TODO: More filesystem commands
More filesystem commands are planned but their designs are not finalized.
Possible functionalities include:
- Saving files to the machine
- Loading saved files
- Listing all files saved on the machine
- Deleting files saved on the machine

### Lens Correction Files
A galvo's lens and mirrors bend straight lines (bulge, skew, trapezoid) and stretch the field (scale).
The firmware removes that distortion using a **correction table**, uploaded by LightBurn as a file of type
`file_type_correction`. The table is an `.lbpcor` file. All multi-byte fields are big-endian.

| Part    | Contents                                                                                  |
|---------|-------------------------------------------------------------------------------------------|
| Header  | `header_size` bytes, described below.                                                     |
| Grid    | `grid_x * grid_y` nodes. Each node is an int32 `dx` then an int32 `dy`, in micrometers.   |
| Trailer | uint32 CRC-32 (zlib / ISO-HDLC) of every byte before it.                                  |

**Header** (version 1, 24 bytes):

| Field          | Type   | Value                                                                  |
|----------------|--------|------------------------------------------------------------------------|
| `magic`        | uint32 | `0x4C425043`, ASCII `LBPC` (`lbpcor_magic`)                            |
| `version`      | uint16 | `1` (`lbpcor_version`)                                                 |
| `header_size`  | uint16 | Size of the header in bytes; `24` for version 1                        |
| `grid_x`       | uint16 | Nodes per row: `65`                                                    |
| `grid_y`       | uint16 | Rows: `65`                                                             |
| `layers`       | uint16 | `1`                                                                    |
| `flags`        | uint16 | Linear corrections baked into the table (below)                        |
| `field_size_x` | int32  | Field span covered by the grid, edge to edge (μm)                      |
| `field_size_y` | int32  | Field span covered by the grid, edge to edge (μm)                      |

A reader skips any header bytes past the fields it knows, using `header_size`, so later versions can add fields.

**Grid.** Nodes are stored row by row, with x varying fastest. Node (`col`, `row`) sits at
`x = -field_size_x/2 + col * field_size_x / (grid_x - 1)` and
`y = -field_size_y/2 + row * field_size_y / (grid_y - 1)`, so row 0 is the most negative y.
Coordinates are field-centre coordinates, the same as `cut_from_absolute` positions on the wire.

**Applying the table.** Each node stores the field's **distortion** at that point: where the beam actually lands,
minus where it was commanded. To correct a target point `p`, the firmware interpolates the grid bilinearly at `p` and
**subtracts** the result: it commands `p - grid(p)`. (This is the EZCAD3 convention.) An all-zero table applies no correction.

**Flags.** The table can also carry linear corrections: whatever linear error was present when the grid was measured.
`flags` records which ones are baked in, so the same correction is not also applied by LightBurn:

| Bit    | Name                    | Meaning                          |
|--------|-------------------------|----------------------------------|
| `0x01` | `lbpcor_flag_scale`     | Field scale is in the table.     |
| `0x02` | `lbpcor_flag_rotation`  | Field rotation is in the table.  |
| `0x04` | `lbpcor_flag_offset`    | Field offset is in the table.    |

Bits 3–15 are reserved and must be `0`. The firmware ignores `flags` and applies the table as written.
LightBurn reads them before uploading, and uses identity (scale 1, rotation 0, offset 0) for each part of its
own field transform whose bit is set. Bulge, skew and trapezoid are always the table's job.

**Receiving the file.** At `cmd_file_end` the firmware checks the file **on its own**: a bad CRC or a length that
does not match the header is rejected with `err_invalid_file`; an unrecognized magic, an unsupported version, or a
header value this machine can never accept is rejected with `err_invalid_header`, with the byte offset of the offending
field as the detail. A file that passes is **stored, but not applied**: it replaces any previously stored table,
and correction is off until it is applied with `cmd_galvo_correction`. Whether the table fits the machine's current
configuration is checked when it is applied, not when it is received.

**Lifetime.** LightBurn keeps the correction file and uploads it on every connection. Firmware may keep the
table across power cycles, but is not required to, and LightBurn never reads a table back.

#### Applying Correction

| Command                | Arguments                 | Arg Size |
|------------------------|---------------------------|----------|
| `cmd_galvo_correction` | uint8 enable (`0` or `1`) | 1        |

`cmd_galvo_correction` with `1` **applies** the stored table. The firmware first checks the table against its
current configuration, and refuses the request if it does not fit:

| Problem                                                           | Error               | Detail                                   |
|-------------------------------------------------------------------|---------------------|------------------------------------------|
| No valid table is stored                                          | `err_wrong_state`   | state flags                              |
| `field_size_x` or `field_size_y` differs from `cfg_field_size_x/y` | `err_file_mismatch` | byte offset of the mismatched header field (`16` or `20`) |

A table made for a different field almost always means a stale table from another lens, or a mistyped field size,
so the firmware refuses it rather than guessing. LightBurn reports the mismatch to the user.

`cmd_galvo_correction` with `0` stops applying the table; the table stays stored. LightBurn does this while its
correction wizard marks a test pattern, so it measures the lens's raw distortion, and applies the table again when
the wizard closes.

Correction is **off** until applied, and goes back to off when:
- a new table is received;
- `cfg_field_size_x` or `cfg_field_size_y` changes, since the table may no longer fit.

Otherwise it keeps its last state; in particular, a handshake does not change it (the handshake has no side effects).
LightBurn uploads and applies the table on every connection, so a host that disconnects mid-calibration leaves
correction off only until it reconnects.

On connecting, LightBurn sends the configuration, uploads the table, then applies it. Firmware that keeps the table
across power cycles may also apply it at power-on, for operation without a host, provided it makes the same check.

### Executing a Job
Lightburn will concatenate all the commands for a job into a **file** and send using the commands listed above.
Once a file has been sent, Lightburn can send `cmd_execute` to begin execution of the currently loaded file.

| Command        | Arg Size       | Description                                    |
|----------------|----------------|------------------------------------------------|
| `cmd_execute`  | 0              | Execute the file currently loaded as a job.    |
| `cmd_pause`    | 0              | Pause the job that is currently executing.     |
| `cmd_continue` | 0              | Continue a paused job.                         |
| `cmd_stop`     | 0              | Stop any job and/or cancel all queued actions. |

One advantage of sending a job as a file is that a job can be sent once and then executed multiple times, enabling workflows
such as manual moving of the material or changing the **user origin** between executions.

### TODO: Streaming vs. Files
We intend to implement commands to allow LightBurn to ask the firmware what method of job delivery is preferred:
command streaming or bulk delivery (in a file). These commands and workflows have not yet been designed.

## Framing
**Framing** is a useful operation that Lightburn provides as a sanity check for its users.
The laser is moved in such a way as to demonstrate the bounds of the cutting job.
It can do this either as a simple bounding rectangle or a tight outline.

There are no frame-specific movement commands, but LBP does provide bookend commands to let the firmware know that
the contained movement commands are part of a framing operation.

| Command            | Arg Size       | Description                                                                     |
|--------------------|----------------|---------------------------------------------------------------------------------|
| `cmd_frame_begin`  | 0              | Indicates that all following movement commands are part of a framing operation. |
| `cmd_frame_end`    | 0              | Marks the end of a framing operation.                                           |

A frame uses the ordinary movement and emission commands. Cut and travel moves say how to move;
`cmd_laser_on/off` or `cmd_guide_light_on/off` say what, if anything, is lit. A gantry may frame with
the laser at low power, while a galvo (or a gantry with a pointer) frames with its guide light.

### Example: Guide Light Frame
Two shapes, framed with the guide light, with the guide light blanked while moving between them:

```
cmd_frame_begin
cmd_cut_speed_xy           frame speed
cmd_travel_speed_xy        frame speed
cmd_galvo_polygon_delay    0          (galvo: no marking delays while framing)
cmd_guide_light_on         0
cmd_cut_abs_xy  ...                   outline of the first shape, lit
cmd_guide_light_off        0          blank before the dark move
cmd_travel_abs_xy                     to the second shape
cmd_dwell                  settle time (us), so the light comes on where the mirrors have stopped
cmd_guide_light_on         0
cmd_cut_abs_xy  ...                   outline of the second shape, lit
cmd_guide_light_off        0
cmd_frame_end
```

The laser on/off delays apply only to lasers, never to guide lights, so the firmware toggles the guide light
exactly where it appears in the sequence. If the firmware applies `cmd_galvo_jump_delay`, that delay also follows
the travel move above; LightBurn sends either the jump delay or the `cmd_dwell`, never both.


## Queries
The responses to all commands discussed above have been quite simple - an acknowledgment consisting of the same command code and no arguments.
We now introduce commands with non-trivial responses. These query commands do not have arguments, but their responses do.

Let's start with the most straightforward queries: what is the current laser position?

### Current Position
The following queries are received from LightBurn with no arguments. Much like movement commands,
any commands composed of the `cmd_pos` MSB and a bitwise combination of `axis_*` values
constitutes a valid position query, and responses values should be sent as `int32` values in the following order,
for each axis present:
`x`, `y`, `z`, `a`, `b`, `c`, `u`, `v`.

| Command (Query)  | Response Arguments (positions in axis units  | Arg Size       |
|------------------|----------------------------------------------|----------------|
| `cmd_pos_x`      | int32 X                                      | 4              |
| `cmd_pos_y`      | int32 Y                                      | 4              |
| `cmd_pos_z`      | int32 Z                                      | 4              |
| `cmd_pos_a`      | int32 A                                      | 4              |
| `cmd_pos_b`      | int32 B                                      | 4              |
| `cmd_pos_c`      | int32 C                                      | 4              |
| `cmd_pos_u`      | int32 U                                      | 4              |
| `cmd_pos_v`      | int32 V                                      | 4              |
| `cmd_pos_xy`     | int32 X, int32 Y                             | 8              |
| `cmd_pos_xyz`    | int32 X, int32 Y, int32 Z                    | 12             |
| `cmd_pos_abc`    | int32 A, int32 B, int32 C                    | 12             |
| `cmd_pos_xyza`   | int32 X, int32 Y, int32 Z, int32 A           | 16             |
| `cmd_pos_xyzu`   | int32 X, int32 Y, int32 Z, int32 U           | 16             |
| `cmd_pos_xyzabc` | int32 X, int32 Y, int32 Z, int32 A, int32 B, int32 C | 24             |

These positions are expected to be reported in machine coordinates, relative to absolute machine zero.

### Machine State
"State" can mean a lot of things, but in the case of the query `cmd_get_state`, it refers to a set of flags which broadly describes what the machine is
doing at the time of the query.

| Command (Query)  | Response Arguments    | Arg Size       |
|------------------|-----------------------|----------------|
| `cmd_get_state`  | int32 state flags     | 4              |

The response argument is a bitwise-OR composition of one or more of the following flags:
- `state_idle`: The machine is on, but not moving or executing any programmed commands.
- `state_moving`: The machine is moving, either from operator or programmed commands.
- `state_executing_job`: The machine is executing a job.
- `state_executing_frame`: The machine is executing framing commands.
- `state_paused`: The machine is paused mid-job.
- `state_receiving`: The machine is currently receiving a file from LightBurn.
- `state_file_loaded`: A file is loaded and ready to execute.
- `state_computing`: The machine is performing a non-trivial calculation.
- `state_fault`: A fault prevents operation (e.g. an open interlock, e-stop, or hardware fault). Set for as long as the fault persists. While it is set, the firmware should refuse commands that would start motion, with `cmd_nack` and `err_fault`.
- `state_pedal_down`: The foot pedal (start input) is currently pressed. Presses themselves are reported by `event_input`.
- `state_door_open`: The enclosure door or lid is open.
- `state_interlock_open`: The safety interlock circuit is open, so the laser cannot fire. A firmware that treats this as a fault also sets `state_fault`.

Many simulatenous flags are possible - for example it is quite reasonable to be moving during a job.

**Note:** More state flags may prove necessary as development continues.

## Configuration
The machine's configuration represent characteristics or settings of the machine that are expected to persist between power settings.
These may include default behaviors such as movement speeds, more granular characteristics such as acceleration values, or physical properties such as rotary dimensions.
Configuration commands can either represent **get** or **set** operations, depending on whether the message includes arguments.

Configuration commands can be easily identified because their symbol names begin with the `cfg_`.

**Note:** The current set of configuration codes represents an early best-guess and is subject to heavy change as development progresses.

### Configuration Query (Get)
Configuration Query (Get) messages use the Configuration code as their command code and have no arguments.
The firmware is to respond to these messages with the same code and a 4-byte argument.
Most of the time this is just a 32-bit integer, but flags are also possible.

As an example, the firmware may receive a message containing `cfg_x_home_offset` and no arguments.
This indicates a query for that configured value.

| Command (Query)     | Response Arguments | Arg Size       |
|---------------------|--------------------|----------------|
| `cfg_x_home_offset` | int32 X (μm)       | 4              |

The firmware responds with the `int32` X homing offset in micrometers.

### Configuration Assignment (Set)

**Setting** a configuration value actually requires two commands.
LightBurn will send a message with the configuration code and a 32-bit argument.
The firmware acknowledges this request with the same command and no arguments.
LightBurn may then send many more configuration set messages. These are all responded to in turn.

The firmware does not **latch** these new values, however, until it receives a message with the command code
`cmd_commit_cfg`. Upon receipt of this message, all previous configuration set commands are completed.

**Note**: Configuration queries return the last value for that configuration code that **has been committed**.
