# SPEC.H

This document is meant to accompany the `spec.h` header and provide a guide for common `lbp` workflows.

## Message Structure

## Command Code Design
LBP command codes are unsigned 16-bit integers. We do not require all 65,536 possible values.
So we take advantage of the large address space for organization and readability.

We assume that most humans who need to read LBP code will do so while programming or debugging and will
thus mostly interact with LBP as hexadecimal codes.

Thus, we break the 16 bits of a command code into four 4-bit "nibbles" and compose command codes out of
readable combinations thereof.

### Most-Significant Nibble Codes
The most significant nibble (**MSN**) represents the broad command category.

The following three nibbles can also be used to narrow down command category,
enabling the construction of commands by bitwise-or operations.
Sometimes flags will be used.

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

or some logical-OR combinations thereof.

### Composition Example:
Consider the command `cmd_move_abs_xy` ("Absolute Move in X and Y"):

`0x6A03` is composed of:
- `0x6000`: MSB for "movement."
- `0x0A00`: Indicates an "absolute" move.
- `0x0003`: Flags for both X and Y axes are set.

Put together, a developer inspecting LBP messages on the wire can read this quite easily.

## Moving the Laser
There are three broad categories of move commands:
- Absolute moves
- Relative moves
- Continuous moves

The firmware responds to all movement commands upon receipt with a message consisting of the
same command and no arguments. For example, the response to a `cmd_move_abs_xy` message is:

| Command             | Arguments | Payload Length |
|---------------------|-----------|----------------|
| `cmd_move_abs_xy`   | None      | 2              |

This response is sent immediately upon receipt of the command, and is sent whether the move
succeeds or fails. The firmware is not required to send any positional update upon move completion.

These movement commands will execute according to the speeds set by the user (using `cmd_speed_*` commands).
If no speed has been set by the user since the last power cycle,
the laser should move at the rate specified by configured defaults.

### Setting movement speed
Setting the movement speed is done using the following command codes:

| Command        | Arguments (speed in micrometers / second) | Payload Length |
|----------------|-------------------------------------------|----------------|
| `cmd_speed_xy` | int32 movement speed in the xy plane      | 6              |
| `cmd_speed_z`  | int32 movement speed along the z axis     | 6              |
| `cmd_speed_u`  | int32 movement speed along the u axis     | 6              |
| `cmd_speed_x`  | int32 movement speed along the x axis*    | 6              |
| `cmd_speed_y`  | int32 movement speed along the y axis*    | 6              |

`cmd_speed_x` and `cmd_speed_y` are provided for machines that cannot move diagonally,
or whose x and y axis movement mechanisms meaningfully differ.

### Absolute Moves
Absolute move commands move the laser to the specified position along the command's indicated axis.
Each command includes one 32-bit integer argument corresponding to the desired axis position in micrometers.

| Command             | Arguments (Axis positions in micrometers) | Payload Length |
|---------------------|-------------------------------------------|----------------|
| `cmd_move_abs_x`    | int32 X (μm)                              | 6              |
| `cmd_move_abs_y`    | int32 Y (μm)                              | 6              |
| `cmd_move_abs_z`    | int32 Z (μm)                              | 6              |
| `cmd_move_abs_u`    | int32 U (μm)                              | 6              |
| `cmd_move_abs_xy`   | int32 X, int32 Y (μm)                     | 10             |
| `cmd_move_abs_xyz`  | int32 X, int32 Y, int32 Z (μm)            | 14             |
| `cmd_move_abs_xyzu` | int32 X, int32 Y, int32 Z, int32 U (μm)   | 18             |

#### Job Cut Origin
In the context of a job, absolute moves in X and Y have the option to be sent relative to a specified origin.

This origin is set with the command `cmd_cut_from` and should be set as part of the **Job Header** (see below).

| Command           | Arguments (code) | Payload Length |
|-------------------|------------------|----------------|
| `cmd_cut_from`    | int8 code        | 3              |

This origin, represented as a single-byte argument, can be either:

- `cut_from_user_origin`: The User Origin, specified using configuration commands (see below).
- `cut_from_current_position`: The laser's XY position at the start of the job.
- `cut_from_absolute`: The laser's machine-specified `(0, 0)`.


### Relative Moves
Relative move commands move the laser by a specified distance (or **delta**) from its previous position at the start of the move.
Each command includes one 32-bit integer argument corresponding to the desired axis delta in micrometers.

| Command             | Arguments (axis distance in micrometers) | Payload Length |
|---------------------|------------------------------------------|----------------|
| `cmd_move_rel_x`    | int32 X (μm)                             | 6              |
| `cmd_move_rel_y`    | int32 Y (μm)                             | 6              |
| `cmd_move_rel_z`    | int32 Z (μm)                             | 6              |
| `cmd_move_rel_u`    | int32 U (μm)                             | 6              |
| `cmd_move_rel_xy`   | int32 X, int32 Y (μm)                    | 10             |
| `cmd_move_rel_xyz`  | int32 X, int32 Y, int32 Z (μm)           | 14             |
| `cmd_move_rel_xyzu` | int32 X, int32 Y, int32 Z, int32 U (μm)  | 18             |

### Continuous Moves (Jog)
Continuous movements are different from Absolute or Relative moves in that they have no distance or positional arguments.
They consist simply of "start" or "stop" commands for movement along a specified axis.

Upon receipt of a "start" command, the firmware is expected to start moving the laser along the axis and direction encoded in the command.
The firmware is expected to continue moving the laser until either the respective "stop" command is received, a higher-priority
or contractictory command is received, or a fault occurs (such as contact with a machine boundary).

| Command               | Description                            | Payload Length |
|-----------------------|----------------------------------------|----------------|
| `cmd_jog_x_pos_start` | Start moving along the positive x axis | 2              |
| `cmd_jog_x_pos_stop`  | Stop moving along the positive x axis  | 2              |
| `cmd_jog_y_pos_start` | Start moving along the positive y axis | 2              |
| `cmd_jog_y_pos_stop`  | Stop moving along the positive y axis  | 2              |
| `cmd_jog_z_pos_start` | Start moving along the positive z axis | 2              |
| `cmd_jog_z_pos_stop`  | Stop moving along the positive z axis  | 2              |
| `cmd_jog_u_pos_start` | Start moving along the positive u axis | 2              |
| `cmd_jog_u_pos_stop`  | Stop moving along the positive u axis  | 2              |

This specification neither forbids nor requires the capability for simultaneous axis jogging -
it simply provides the command definitions.
If the user sends `cmd_jog_x_pos_start` immediately followed by `cmd_jog_y_pos_start`, and
your hardware is capable of said movement, we leave the resulting behaviour to your discretion.
It is reasonable to either cancel the X jog and begin a Y Jog, begin jogging diagonally, or cancel both requests.

#### TODO
How these commands may differ for galvanometer movement is yet to be designed.
We may either define different commands, or specify that the same commands are to be interpreted with different units.

## Controlling the Laser
Your machine may have more than one laser tube. All laser control commands have 1-byte laser index argument
in addition to any respective numerical arguments. This index is `1`-based, with `0` being shorthand for
"use whichever lasers have been enabled."

| Command               | Arguments                                  | Payload Length |
|-----------------------|--------------------------------------------|----------------|
| `cmd_laser_enable`    | int8 laser index                           | 3              |
| `cmd_laser_disable`   | int8 laser index                           | 3              |
| `cmd_laser_power_max` | int8 laser index, int16 max power (%)*     | 5              |
| `cmd_laser_power_min` | int8 laser index, int16 max power (%)*     | 5              |
| `cmd_laser_freq`      | int8 laser index, int32 pwm frequency (Hz) | 7              |

These laser settings are expected to persist until the respective commands are sent again.

### Enabling Laser Tubes
Before embarking on a cut, LB will send `cmd_laser_enable` (and possibly `cmd_laser_disable`) messages
to dictate which lasers are being utilized for a cut.

Unlike other laser commands, a laser index value of `0` is not valid for these commands, since these commands
are those that give the `0` index argument its meaning.

### Laser Power
Laser power for a cut is sent as a percentage of the laser's configured maximum output power.
The minimum and maximum output power for each laser involved in a cut should be sent before turning the laser on and moving.

### \*Note: Percentages in LBP
All percentages in LBP are represented as int16 arguments, where each integer value is 1/16384 of 100%, or `1/163.84 %`.
For example, `100%` is represented as `16384`, `50%` is represented as `8192`, and `1%` is represented as `165`.

### Laser Frequency (PWM)
In addition to power, it is also necessary to set the laser's PWM frequency. This is given in int32 Hz.

### Turning the Laser On

| Command         | Arguments        | Payload Length |
|-----------------|------------------|----------------|
| `cmd_laser_on`  | int8 laser index | 3              |
| `cmd_laser_off` | int8 laser index | 3              |

Once the laser settings have been sent, it is still necessary to turn the enabled lasers on and off.

**Remember:** If respective `cmd_laser_enable` and `cmd_laser_disable` have been sent prior to these commands, it is
appropriate to send a laser index of `0` to apply these commands to the enabled laser tube(s).

## Making a Cut

## Composing a Job

### The Job Header

### The Job Body

## Sending a Job

## Framing

## Configuration
