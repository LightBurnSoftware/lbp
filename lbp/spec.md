# SPEC.H

This document is meant to accompany the `spec.h` header and provide a guide for common `lbp` workflows.

## Overview
LBP is a transport-agnostic command-and-response binary protocol for controlling laser cutters and engravers.

LBP is also a work-in-progress. Some functionality has not yet been implemented, and the structure of some messages and workflows is still to be determined. Feedback and commentary is welcome.

LBP is intended to function over a variety of different transport layers, e.g. serial, TCP, UDP, websocket, etc. This specification assumes that the transport layer will deliver LBP messages quickly and in the correct order.

## Message Structure

```
+--------------+------+----------+----------+
| Header       | Size | Payload  | Checksum |
| [4B]: "DRGN" | [2B] | [Size B] | [2B]     |
+--------------+------+----------+----------+
```

- **Header (4 bytes):** The 4-byte sequence `0x4452474E`, which is the ASCII encoding of `DRGN` (short for "dragon").
- **Size (2 bytes):** A 16-bit integer which encodes the size, in bytes, of the following **Payload**.
- **Payload ("Size" bytes):** Always consists of a 2-byte Command, followed by 0 or more bytes of argument data. (This means that **Size** must always be `>= 2`)
- **Checksum (2 bytes):** The CRC16 checksum of the **Payload**, computed over **only** the Payload data, **not** the Header of Size data. (See `checksum.cpp` for the crc16 algorithm.)

**IMPORTANT** All Size, Payload, and Checksum data is encoded in **Big-Endian Format**. (Helper functions are provided in `lbp/beio.h`)

See the `spec.h` header file for message dimension constants, command code definitions, and argument information.

## Simple Workflows

At a high level, LBP works as a command-and-response protocol:

1. **LightBurn sends commands.** These are compact, structured binary messages — instructions like "move to this position," "set laser power," "start a job," or "report your current state."

2. **The controller executes them and responds.** The manufacturer's firmware receives each packet, interprets the command, carries out the operation on the hardware, and responds to Lightburn with another message. This message always contains the same command code as the request along with optional arguments in the case of a query.

In the case of commands that result in long-running operations, the firmware's response is an **acknowledgement** of the request.
It **begins** the operation and responds to the command **immediately** - **not** at the conclusion of the operation.

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
- `0x0008`: U Axis

or some bitwise-OR combinations thereof.

### Composition Example:
Consider the command `cmd_cut_abs_xy` ("Move to absolute X and Y position while cutting"):

`0x6A03` is composed of:
- `0x6000`: MSB for "movement."
- `0x0A00`: Indicates an "absolute cut" move (within the "movement" category).
- `0x0003`: Flags for both X and Y axes are set.

Put together, a developer inspecting LBP messages on the wire can read this quite easily.

## Moving the Laser
LBP provides many categories of movement commands. This is to give the firmware **context** for the movement.
The first broad categorical distinction is whether the move is an "operator" move or a "programmed" move.

- **Operator** moves originate outside the context of a programmed job. The user is pressing buttons or other controls
in LightBurn to move the laser in real time.
- **Programmed** moves occur in the context of a job.

Within these categories are further distinctions. Both **operator** and **programmed** moves may be either **absolute** or **relative**,
with **operator** moves also having the option of being **continuous**. **Programmed** moves provide additional context of **cut** vs **rapid**,
which may effect how the firmware plans the movement. In addition, the **homing** is considered an **operator** move.
So, all together, we have:

**Operator**
- **home**: move the laser to its **home** position on any or all axes.
- **jog start**: start a continuous movement on a particular axis.
- **jog stop**: stop a continuous movement on a particular axis.
- **jog step**: step the laser by a certain distance from its current position. Think of this as a "relative operator move."
- **goto**: move the laser directly to a certain absolute position. Think of this as an "absolute operator move."

**Programmed**:
- **relative rapid**: move the laser a specified distance from its current position. It is **not expected** that the laser is cutting.
- **absolute rapid**: move the laser to a specific position (relative to the origin set by `cmd_cut_from`). It is **not expected** that the laser is cutting.
- **relative cut**: move the laser a specified distance from its current position. The laser **is expected** to be active and cutting.
- **absolute cut**: move the laser to a specific position (relative to the origin set by `cmd_cut_from`).The laser **is expected** to be active and cutting.
- **dwell**: perform no movement for a specified number of milliseconds.

These distinctions exist to provide the firmware with as much **context** as possible for the command. They also allow for more granular
configuration of default speeds and movement behaviors. The intention is to help make LBP-capable firmware easier to write and maintain.

**Note**: These **programmed movement** commands **do not** automatically turn the laser on or off. The user is expected to program those commands separately,
prior to the movement commands.

#### TODO: Galvanometer Movement
How these movement commands may differ for galvanometer movement is yet to be designed.
We may either define different commands, or specify that the same commands are to be interpreted with different units.

### Movement Speed
These movement commands will execute according to the speeds set by the user (using `cmd_speed_*` commands, see below).
If no speed has been set by the user since the last power cycle, the laser should move at the rate specified by configured defaults.

| Command        | Arguments (speed in micrometers / second) | Payload Length |
|----------------|-------------------------------------------|----------------|
| `cmd_speed_xy` | int32 movement speed in the xy plane      | 6              |
| `cmd_speed_z`  | int32 movement speed along the z axis     | 6              |
| `cmd_speed_u`  | int32 movement speed along the u axis     | 6              |
| `cmd_speed_x`  | int32 movement speed along the x axis*    | 6              |
| `cmd_speed_y`  | int32 movement speed along the y axis*    | 6              |

`cmd_speed_x` and `cmd_speed_y` are provided for machines that cannot move diagonally,
or whose x and y axis movement mechanisms meaningfully differ.

#### TODO: Granular Movement Speed Commands
It may be useful to define different speed commands for the different kinds of movement.
We may do this in the future. For now, `cmd_speed_*` overrides default behavior for all moves,
and most movement commands are expected to be preceeded with a `cmd_speed_*` if they wish to
have different speeds than preceding movements.

### Operator Movements
These commands are expected to originate from a movement panel in the user interface. The user is issuing movement commands to the machine live
and expects the machine to respond in real time.

#### Movement Command Response
The firmware responds to all operator movement commands upon receipt with a message consisting of the
same command and no arguments. For example, the response to a `cmd_goto_xy` message is:

| Command         | Arguments | Payload Length |
|-----------------|-----------|----------------|
| `cmd_goto_xy`   | None      | 2              |

This response is sent immediately upon receipt of the command, and is sent whether the move
succeeds or fails. The firmware is not required to send any positional update upon move completion.

#### Go To (Absolute Operator Move)
Go To commands move the laser to the specified position along the command's indicated axis.
Each command includes one 32-bit integer argument corresponding to the desired axis position in micrometers.

| Command         | Arguments (Axis positions in micrometers) | Payload Length |
|-----------------|-------------------------------------------|----------------|
| `cmd_goto_x`    | int32 X (μm)                              | 6              |
| `cmd_goto_y`    | int32 Y (μm)                              | 6              |
| `cmd_goto_z`    | int32 Z (μm)                              | 6              |
| `cmd_goto_u`    | int32 U (μm)                              | 6              |
| `cmd_goto_xy`   | int32 X, int32 Y (μm)                     | 10             |
| `cmd_goto_xyz`  | int32 X, int32 Y, int32 Z (μm)            | 14             |
| `cmd_goto_xyzu` | int32 X, int32 Y, int32 Z, int32 U (μm)   | 18             |

**Note**: Go To command coordinates are to be interpreted relative to the machine's absolute zero.

#### Jog Step (Relative Operator Move)
Jog Step commands are named such in order to be familiar to machine operators. They are really
just relative operator moves. Thus, the argument is to be one 32-bit argument per indicated axis
representing the distance to move from the current location along that axis.

| Command             | Arguments (Axis positions in micrometers) | Payload Length |
|---------------------|-------------------------------------------|----------------|
| `cmd_jog_step_x`    | int32 X (μm)                              | 6              |
| `cmd_jog_step_y`    | int32 Y (μm)                              | 6              |
| `cmd_jog_step_z`    | int32 Z (μm)                              | 6              |
| `cmd_jog_step_u`    | int32 U (μm)                              | 6              |
| `cmd_jog_step_xy`   | int32 X, int32 Y (μm)                     | 10             |
| `cmd_jog_step_xyz`  | int32 X, int32 Y, int32 Z (μm)            | 14             |
| `cmd_jog_step_xyzu` | int32 X, int32 Y, int32 Z, int32 U (μm)   | 18             |

#### Jog Start/Stop (Continuous Operator Move)
Continuous movements are different from Absolute (Goto) or Relative (Jog Step) moves in that they have no distance or positional arguments.
In fact, they have **no arguments at all**. They consist simply of "start" or "stop" commands for movement along a specified axis.

Upon receipt of a "start" command, the firmware is expected to start moving the laser along the axis and direction encoded in the command.
The firmware is expected to continue moving the laser until either the respective "stop" command is received, a higher-priority
or contradictory command is received, or a fault occurs (such as contact with a machine boundary).

| Command               | Payload Length | Description                            |
|-----------------------|----------------|----------------------------------------|
| `cmd_jog_start_pos_x` | 2              | Start moving along the positive x axis |
| `cmd_jog_stop_pos_x`  | 2              | Stop moving along the positive x axis  |
| `cmd_jog_start_neg_x` | 2              | Start moving along the negative x axis |
| `cmd_jog_stop_neg_x`  | 2              | Stop moving along the negative x axis  |
| `cmd_jog_start_pos_y` | 2              | Start moving along the positive y axis |
| `cmd_jog_stop_pos_y`  | 2              | Stop moving along the positive y axis  |
| `cmd_jog_start_neg_y` | 2              | Start moving along the negative y axis |
| `cmd_jog_stop_neg_y`  | 2              | Stop moving along the negative y axis  |
| `cmd_jog_start_pos_z` | 2              | Start moving along the positive z axis |
| `cmd_jog_stop_pos_z`  | 2              | Stop moving along the positive z axis  |
| `cmd_jog_start_neg_z` | 2              | Start moving along the negative z axis |
| `cmd_jog_stop_neg_z`  | 2              | Stop moving along the negative z axis  |
| `cmd_jog_start_pos_u` | 2              | Start moving along the positive u axis |
| `cmd_jog_stop_pos_u`  | 2              | Stop moving along the positive u axis  |
| `cmd_jog_start_neg_u` | 2              | Start moving along the negative u axis |
| `cmd_jog_stop_neg_u`  | 2              | Stop moving along the negative u axis  |

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
Cut commands can be either relative or absolute.

#### Absolute Cut
| Command               | Arguments (Axis positions in micrometers) | Payload Length |
|-----------------------|-------------------------------------------|----------------|
| `cmd_cut_abs_x`       | int32 X (μm)                              | 6              |
| `cmd_cut_abs_y`       | int32 Y (μm)                              | 6              |
| `cmd_cut_abs_z`       | int32 Z (μm)                              | 6              |
| `cmd_cut_abs_u`       | int32 U (μm)                              | 6              |
| `cmd_cut_abs_xy`      | int32 X, int32 Y (μm)                     | 10             |
| `cmd_cut_abs_xyz`     | int32 X, int32 Y, int32 Z (μm)            | 14             |
| `cmd_cut_abs_xyzu`    | int32 X, int32 Y, int32 Z, int32 U (μm)   | 18             |

Absolute moves in X and Y have the option to be sent relative to a specified origin.
This origin is set with the command `cmd_cut_from` and should be set as part of the **Job Header** (see below).

#### Relative Cut
Just like Jog Step, Relative Cut move commands move the laser by a specified distance (or **delta**) from its previous position at the start of the move.
Each command includes one 32-bit integer argument corresponding to the desired axis delta in micrometers.

| Command               | Arguments (axis distance in micrometers) | Payload Length |
|-----------------------|------------------------------------------|----------------|
| `cmd_cut_rel_x`       | int32 X (μm)                             | 6              |
| `cmd_cut_rel_y`       | int32 Y (μm)                             | 6              |
| `cmd_cut_rel_z`       | int32 Z (μm)                             | 6              |
| `cmd_cut_rel_u`       | int32 U (μm)                             | 6              |
| `cmd_cut_rel_xy`      | int32 X, int32 Y (μm)                    | 10             |
| `cmd_cut_rel_xyz`     | int32 X, int32 Y, int32 Z (μm)           | 14             |
| `cmd_cut_rel_xyzu`    | int32 X, int32 Y, int32 Z, int32 U (μm)  | 18             |

#### Rapid
**Rapid** Commands are programmed moves that signal to the firmware that the laser is **not expected** to be
on and cutting during the move. Rapid moves are expected to be faster than cuts.

#### Absolute Rapid
Just like Absolute Cuts, Absolute Rapid arguments are axis positions **relative to the jog origin**.
Job Origin is set in the Job Header using the `cmd_cut_from` message.

| Command               | Arguments (Axis positions in micrometers) | Payload Length |
|-----------------------|-------------------------------------------|----------------|
| `cmd_rapid_abs_x`    | int32 X (μm)                              | 6              |
| `cmd_rapid_abs_y`    | int32 Y (μm)                              | 6              |
| `cmd_rapid_abs_z`    | int32 Z (μm)                              | 6              |
| `cmd_rapid_abs_u`    | int32 U (μm)                              | 6              |
| `cmd_rapid_abs_xy`   | int32 X, int32 Y (μm)                     | 10             |
| `cmd_rapid_abs_xyz`  | int32 X, int32 Y, int32 Z (μm)            | 14             |
| `cmd_rapid_abs_xyzu` | int32 X, int32 Y, int32 Z, int32 U (μm)   | 18             |

#### Relative Rapid
| Command               | Arguments (axis distance in micrometers) | Payload Length |
|-----------------------|------------------------------------------|----------------|
| `cmd_rapid_rel_x`    | int32 X (μm)                             | 6              |
| `cmd_rapid_rel_y`    | int32 Y (μm)                             | 6              |
| `cmd_rapid_rel_z`    | int32 Z (μm)                             | 6              |
| `cmd_rapid_rel_u`    | int32 U (μm)                             | 6              |
| `cmd_rapid_rel_xy`   | int32 X, int32 Y (μm)                    | 10             |
| `cmd_rapid_rel_xyz`  | int32 X, int32 Y, int32 Z (μm)           | 14             |
| `cmd_rapid_rel_xyzu` | int32 X, int32 Y, int32 Z, int32 U (μm)  | 18             |

#### Dwell
While not technically a "movement", "dwell" commands the laser to remain in place for a specified number of milliseconds.
This can occur whether the laser is on or off.

| Command     | Arguments (code)   | Payload Length |
|-------------|--------------------|----------------|
| `cmd_dwell` | int32 milliseconds | 6              |

## Controlling the Laser
Your machine may have more than one laser tube. All laser control commands have 1-byte laser index argument
in addition to any respective numerical arguments. This index is `0`-based, with index `0` corresponding to the first laser,
`1` to the second, etc.

| Command               | Arguments                                  | Payload Length |
|-----------------------|--------------------------------------------|----------------|
| `cmd_laser_enable`    | int8 laser index                           | 3              |
| `cmd_laser_disable`   | int8 laser index                           | 3              |
| `cmd_laser_power_max` | int8 laser index, int16 max power (%)*     | 5              |
| `cmd_laser_power_min` | int8 laser index, int16 max power (%)*     | 5              |
| `cmd_laser_freq`      | int8 laser index, int32 PWM frequency (Hz) | 7              |

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

| Command         | Arguments        | Payload Length |
|-----------------|------------------|----------------|
| `cmd_laser_on`  | int8 laser index | 3              |
| `cmd_laser_off` | int8 laser index | 3              |

### Cut Type
This command is a work-in-progress, intended to give the firmware information about the upcoming cuts that might be helpful in motion planning.

| Command         | Arguments    | Payload Length |
|-----------------|--------------|----------------|
| `cmd_cut_type`  | int16 value  | 4              |

Where value may be:
- `cut_type_normal`: a normal cut, no additional information encoded.
- `cut_type_scan_uni_x`: a unidirectional scan along the x axis.
- `cut_type_scan_uni_y`: a unidirectional scan along the y axis.
- `cut_type_scan_bi_x`: a bidirectional scan along the x axis.
- `cut_type_scan_bi_y`: a bidirectional scan along the y axis.

## Making a Cut
Generally, cuts or engravings will be programmed in the following pattern:
1. Settings for the cut, if they differ from those for the previous cut, are sent with respective commands - lasers are enabled or disabled, laser power and frequency are set, movement speed is set, etc.
2. If the laser is not already positioned at the start of the cut, a `cmd_laser_off` is sent, followed by and `cmd_rapid_xy`.
3. `cmd_laser_on` and `cmd_cut_xy` are sent, turning on the laser(s) and performing the cut.

## Raster Engraving
To aid in motion planning, LBP offers a special command for encoding raster lines.

| Command             | Arguments                      | Payload Length |
|---------------------|--------------------------------|----------------|
| `cmd_raster_power`  | 1 <= N <= 8 int16 power values | 6 - 18         |

If the firmware receives this command, the next `cmd_cut_x`, `cmd_cut_y`, or `cmd_cut_xy` should be assumed to be a raster line.
This `cmd_cut` will engrave N "pixels" of equal length using the power settings received in the `cmd_raster_power` message.
The laser is to perform the movement at the set speed, changing output power as it moves to engrave all N pixel values in one pass.

## Jobs
We use the term "job" to refer to a series of programmed commands that encode a complete cutting/engraving task for the laser.
LBP uses commands with `_begin` and `_end` as bookend-style tags to indicate the context of these messages.

| Command                | Payload Length | Description                                                     |
|------------------------|----------------|-----------------------------------------------------------------|
| `cmd_job_begin`        | 2              | Marks the start of job commands.                                |
| `cmd_job_header_begin` | 2              | Marks the start of commands in the job header.                  |
| `cmd_job_header_end`   | 2              | Marks the end of the job header.                                |
| `cmd_job_body_begin`   | 2              | Marks the start of commands that make up the action of the job. |
| `cmd_job_body_end`     | 2              | Marks the end of commands that make up the action of the job.   |
| `cmd_job_end`          | 2              | Marks the end of job commands.                                  |


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

| Command           | Arguments (code) | Payload Length |
|-------------------|------------------|----------------|
| `cmd_cut_from`    | int8 code        | 3              |

This origin, represented as a single-byte argument, can be either:

- `cut_from_user_origin`: The User Origin, specified using configuration commands (see below).
- `cut_from_current_position`: The laser's XY position at the start of the job.
- `cut_from_absolute`: The laser's machine-specified `(0, 0)`.

#### Job Bounds
LBP provides the following commands to inform the firmware of the boundaries of the job in absolute coordinates calculated from the previously-specified job origin.

| Command               | Arguments (μm)           | Payload Length |
|-----------------------|--------------------------|----------------|
| `cmd_bounds_min_x`    | int32 minimum x position | 6              |
| `cmd_bounds_max_x`    | int32 maximum x position | 6              |
| `cmd_bounds_min_y`    | int32 minimum y position | 6              |
| `cmd_bounds_max_y`    | int32 maximum y position | 6              |
| `cmd_bounds_min_z`    | int32 minimum z position | 6              |
| `cmd_bounds_max_z`    | int32 maximum z position | 6              |
| `cmd_bounds_min_u`    | int32 minimum u position | 6              |
| `cmd_bounds_max_u`    | int32 maximum u position | 6              |

### The Job Body
This is where the action of the job is programmed. This will mostly consist of:
- commands for applying laser settings
- commands for applying cut settings
- commands for moving and cutting

### Files
A LBP **file** is simply a concatenated list of valid and complete LBP messages.
(Including the message header, size, and checksum fields.)
Thus, the bytes in a file can be parsed into payloads using the same algorithms as bytes coming in off the wire.

### Receiving a File
A **file** is sent with the following commands:

| Command          | Arguments                | Payload Length        |
|------------------|--------------------------|-----------------------|
| `cmd_file_begin` | int32 file size in bytes | 6                     |
| `cmd_file_chunk` | Slice of file content    | 2 ~ `size_file_chunk` |
| `cmd_file_end`   | None                     | 2                     |

Files are sent in messages of the maximum permitted size.
Firmware is expected to use the argument of `cmd_file_begin` to prepare storage for the incoming file.
It will then concatenate the arguments of all `cmd_file_chunk` messages in the order they are received.

### TODO: File chunk ordering
It may prove necessary to add a "sequence number" or "file offset" argument to each `cmd_file_chunk` message.

### TODO: More filesystem commands
More filesystem commands are planned but their designs are not finalized.
Possible functionalities include:
- Saving files to the machine
- Loading saved files
- Listing all files saved on the machine
- Deleting files saved on the machine

### Executing a Job
Lightburn will concatenate all the commands for a job into a **file** and send using the commands listed above.
Once a file has been sent, Lightburn can send `cmd_execute` to begin execution of the currently loaded file.

| Command        | Payload Length | Description                                    |
|----------------|----------------|------------------------------------------------|
| `cmd_execute`  | 2              | Execute the file currently loaded as a job.    |
| `cmd_pause`    | 2              | Pause the job that is currently executing.     |
| `cmd_continue` | 2              | Continue a paused job.                         |
| `cmd_stop`     | 2              | Stop any job and/or cancel all queued actions. |

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

| Command            | Payload Length | Description                                                                     |
|--------------------|----------------|---------------------------------------------------------------------------------|
| `cmd_frame_begin`  | 2              | Indicates that all following movement commands are part of a framing operation. |
| `cmd_frame_end`    | 2              | Marks the end of a framing operation.                                           |


## Queries
The responses to all commands discussed above have been quite simple - an acknowledgment consisting of the same command code and no arguments.
We now introduce commands with non-trivial responses. These query commands do not have arguments, but their responses do.

Let's start with the most straightforward queries: what is the current laser position?

### Current Position
The following queries are received from LightBurn with no arguments. The following table describes the responses.

| Command (Query)  | Response Arguments (μm)                  | Payload Length |
|------------------|------------------------------------------|----------------|
| `cmd_pos_x`      | int32 X (μm)                             | 6              |
| `cmd_pos_y`      | int32 Y (μm)                             | 6              |
| `cmd_pos_z`      | int32 Z (μm)                             | 6              |
| `cmd_pos_u`      | int32 U (μm)                             | 6              |
| `cmd_pos_xy`     | int32 X, int32 Y (μm)                    | 10             |
| `cmd_pos_xyz`    | int32 X, int32 Y, int32 Z (μm)           | 14             |
| `cmd_pos_xyzu`   | int32 X, int32 Y, int32 Z, int32 U (μm)  | 18             |

These positions are expected to be reported in machine coordinates, relative to absolute machine zero.

### Machine State
"State" can mean a lot of things, but in the case of the query `cmd_get_state`, it refers to a set of flags which broadly describes what the machine is
doing at the time of the query.

| Command (Query)  | Response Arguments    | Payload Length |
|------------------|-----------------------|----------------|
| `cmd_get_state`  | int32 state flags     | 6              |

The response argument is a bitwise-OR composition of one or more of the following flags:
- `state_idle`: The machine is on, but not moving or executing any programmed commands.
- `state_moving`: The machine is moving, either from operator or programmed commands.
- `state_executing_job`: The machine is executing a job.
- `state_executing_frame`: The machine is executing framing commands.
- `state_paused`: The machine is paused mid-job.
- `state_receiving`: The machine is currently receiving a file from LightBurn.
- `state_file_loaded`: A file is loaded and ready to execute.
- `state_computing`: The machine is performing a non-trivial calculation.

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

| Command (Query)     | Response Arguments | Payload Length |
|---------------------|--------------------|----------------|
| `cfg_x_home_offset` | int32 X (μm)       | 6              |

The firmware responds with the `int32` X homing offset in micrometers.

### Configuration Assignment (Set)

**Setting** a configuration value actually requires two commands.
LightBurn will send a message with the configuration code and a 32-bit argument.
The firmware acknowledges this request with the same command and no arguments.
LightBurn may then send many more configuration set messages. These are all responded to in turn.

The firmware does not **latch** these new values, however, until it receives a message with the command code
`cmd_commit_cfg`. Upon receipt of this message, all previous configuration set commands are completed.

**Note**: Configuration queries return the last value for that configuration code that **has been committed**.
