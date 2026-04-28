// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#ifndef LBP_SPEC_H
#define LBP_SPEC_H

#include <cstdint>

// Specification for the LightBurn Protocol.

namespace lbp {

//sizes of message components
constexpr uint8_t size_start = 4; // 4 bytes for start header
constexpr uint8_t size_len = 2; // 2 bytes to store payload size
constexpr uint8_t size_crc = 2; // 2 bytes to store crc16 at end of message
constexpr uint8_t size_cmd = 2; // 2 bytes for the lbp command code.
constexpr uint8_t size_min_payload = size_cmd; // smallest is just a 16 bit command
constexpr uint8_t size_cfg_payload = size_cmd + 4; // all cfg payloads are just the command plus a 4-byte value.
constexpr uint8_t size_header_footer = size_start + size_len + size_crc; // size of everything *except* the payload.
constexpr uint8_t size_min_msg = size_header_footer + size_min_payload; // size of the smallest permitted message.
constexpr uint8_t size_payload_offset = size_start + size_len; // index of the first byte of the payload
constexpr uint8_t size_arg_offset = size_payload_offset + size_min_payload; // index of the first byte of payload "arguments"

// sizes of common msgs and payloads (defined to facilitate static allocations).
constexpr int size_max_cmd_args = 16; // no commands require more than four 4-byte integers.
constexpr int size_max_cmd_payload = size_max_cmd_args + size_cmd; // largest expected command payload.
constexpr int size_max_cmd_msg = size_header_footer + size_max_cmd_payload; // largest expected command message.
constexpr int size_file_msg = 512; // total size of a file message.
constexpr int size_file_payload = size_file_msg - size_header_footer; // size of a file payload.
constexpr int size_file_chunk = size_file_msg - size_header_footer - size_cmd; // length of the actual file segment sent in a cmd_file_chunk message.

//header of every message
constexpr uint32_t cmd_start = 0x4452474E; //'DRGN' in ascii, short for "dragon".

// LBP command codes are broken into 16 headings.
// The most significant nibble (four bits) represents the broad command category.

// The other three nibbles can also be used to narrow down command category,
// enabling the construction of commands by bitwise-or operations.
// Sometimes flags will be used.

// The purpose of this compositional approach is two-fold:
// 1: to aid in readability when visually examining LBP data in a binary or hex viewer.
// 2: to make the protocol more fun to design.

// ---- MSN (Most Significant Nibble) Categories ----
// 0x0000 - Fundamentals - beginnings, endings, basics.
// 0x1000 - Laser commands (1 = "L", for "Laser")
// 0x2000 - Unused
// 0x3000 - Unused
// 0x4000 - Files and File System
// 0x5000 - Non-persistent Settings (5 = "S", for "Settings")
// 0x6000 - Movement commands (6 = "G", for "Go")
// 0x7000 - Tool commands (7 = "T", for "Tool")
// 0x8000 - Observable State (8 rhymes with "State")
// 0x9000 - Unused
// 0xA000 - Unused
// 0xB000 - Unused
// 0xC000 - Common Configuration (C for "Configuration")
// 0xD000 - Reserved: Vender-specific Configuration
// 0xE000 - Reserved: Extended Future Capabilities
// 0xF000 - Unused

// ---- LSN Flags (Least Significant Nibble) ----

// Axis Flags
constexpr uint16_t flag_x = 0x0001;
constexpr uint16_t flag_y = 0x0002;
constexpr uint16_t flag_z = 0x0004;
constexpr uint16_t flag_u = 0x0008;
// Reserve 0x00F0 for more axes. (6-axis controllers?)

// ------------------------------------------
// -------BEGIN COMMAND DEFINITIONS----------
// ------------------------------------------

// ----------------------------------------------------------------------------
// 0x0000: Fundamentals -------------------------------------------------------
// ----------------------------------------------------------------------------
constexpr uint16_t cmd_handshake = 0x01B8; // Verify communication

constexpr uint16_t cmd_job_begin 		= 0x070B; // Marks the beginning of bulk commands relating to a job
constexpr uint16_t cmd_job_end	 		= 0x070E; // Marks the end of bulk commands relating to a job
constexpr uint16_t cmd_job_header_begin	= 0x078B; // Marks the beginning job-wide settings.
constexpr uint16_t cmd_job_header_end 	= 0x078E; // Marks the end of job-wide settings.
constexpr uint16_t cmd_job_body_begin	= 0x07BB; // Marks the beginning of job commands. (movements, cuts, local settings, etc)
constexpr uint16_t cmd_job_body_end 	= 0x07BE; // Marks the end of job commands.

constexpr uint16_t cmd_execute	= 0x0C66;	// Execute loaded file.
constexpr uint16_t cmd_pause	= 0x0C77;	// Pause job in such a way as to be continued.
constexpr uint16_t cmd_continue = 0x0C88;	// Continue a job that has been paused.
constexpr uint16_t cmd_stop 	= 0x0CFF;	// Stop job and/or any movement and cutting.

constexpr uint16_t cmd_frame_begin = 0x0FAB; // Marks beginning of a frame commands.
constexpr uint16_t cmd_frame_end = 0x0FAE; // Marks the end of frame commands.

constexpr uint16_t cmd_commit_cfg = 0x0CCC; // commit changes sent with cfg_ commands.

// ----------------------------------------------------------------------------
// 0x0001: Laser Commands -----------------------------------------------------
// ----------------------------------------------------------------------------
constexpr uint16_t flag_laser = 0x1000; // (1 = "L", for "Laser")

constexpr uint16_t cmd_laser_power_min	= flag_laser | 0x05A0; // int8 laser index, int16 percent of max power
constexpr uint16_t cmd_laser_power_max	= flag_laser | 0x05A1; // int8 laser index, int16 percent of max power
constexpr uint16_t cmd_laser_freq 		= flag_laser | 0x05F0; // int8 laser index, int32 frequency (TODO unit)
constexpr uint16_t cmd_laser_enable 	= flag_laser | 0x05E1; // int8 laser index (choose which laser(s) is the cut default.)
constexpr uint16_t cmd_laser_disable 	= flag_laser | 0x05E2; // int8 laser index, (choose which lasers(s) is the cut default.)
constexpr uint16_t cmd_laser_offset_xy	= flag_laser | 0x05B0 | flag_x | flag_y; // int8 laser index, int32 x, y offsets (um)

constexpr uint16_t cmd_laser_off 		= flag_laser | 0x05C1; // int8 laser index (0 for the lasers enabled with cmd_laser_enable)
constexpr uint16_t cmd_laser_on 		= flag_laser | 0x05C2; // int8 laser index (0 for the lasers enabled with cmd_laser_enable)

constexpr uint16_t cmd_focus_z 			= flag_laser | 0x0F00 | flag_z; // arguments TODO

// ----------------------------------------------------------------------------
// 0x0004: Files --------------------------------------------------------------
// ----------------------------------------------------------------------------
constexpr uint16_t flag_files = 0x4000; // (4 starts with "F", for "Files")

constexpr uint16_t cmd_get_filename		= flag_files | 0x0401; // TODO: int16 file index
constexpr uint16_t cmd_set_filename		= flag_files | 0x0402; // TODO: int16 short file index
constexpr uint16_t cmd_delete_file		= flag_files | 0x0403; // TODO: int16 file index
constexpr uint16_t cmd_begin_file		= flag_files | 0x0404; // int32 file size
constexpr uint16_t cmd_end_file			= flag_files | 0x0405; // Marks the end of a sent file.
constexpr uint16_t cmd_new_file			= flag_files | 0x0406; // TODO
constexpr uint16_t cmd_file_count		= flag_files | 0x0407; // TODO
constexpr uint16_t cmd_file_time		= flag_files | 0x0408; // TODO
constexpr uint16_t cmd_calc_file_time 	= flag_files | 0x0409; // TODO
constexpr uint16_t cmd_file_chunk		= flag_files | 0x04FC; // Mark a file chunk.

// Filesystem queries:
constexpr uint16_t cmd_flash_available	= flag_files | 0x05FA; // TODO: return int64 bytes
constexpr uint16_t cmd_mainboard_version = flag_files | 0x05B0; // TODO

// ----------------------------------------------------------------------------
// 0x5000: Settings -----------------------------------------------------------
// ----------------------------------------------------------------------------
constexpr uint16_t flag_settings = 0x5000; // (5 = "S", for "Settings")

// Speed commands
constexpr uint16_t flag_speed = flag_settings | 0x0100;

constexpr uint16_t cmd_speed_xy	= flag_speed | flag_x | flag_y;
constexpr uint16_t cmd_speed_z	= flag_speed | flag_z;
constexpr uint16_t cmd_speed_u	= flag_speed | flag_u;

// Boundary commands
constexpr uint16_t flag_bounds_min = flag_settings | 0x0200;
constexpr uint16_t flag_bounds_max = flag_settings | 0x0300;

// Boundary commands: Composed of flags. Argument: one signed int32 (micrometers) per indicated axis.
constexpr uint16_t cmd_bounds_min_x = flag_bounds_min | flag_x;
constexpr uint16_t cmd_bounds_max_x = flag_bounds_max | flag_x;
constexpr uint16_t cmd_bounds_min_y = flag_bounds_min | flag_y;
constexpr uint16_t cmd_bounds_max_y = flag_bounds_max | flag_y;
constexpr uint16_t cmd_bounds_min_z = flag_bounds_min | flag_z;
constexpr uint16_t cmd_bounds_max_z = flag_bounds_max | flag_z;
constexpr uint16_t cmd_bounds_min_u = flag_bounds_min | flag_u;
constexpr uint16_t cmd_bounds_max_u = flag_bounds_max | flag_u;

// When this appears in a job header, all following absolute xy move commands
// should be interpreted as relative to the following options.
constexpr uint16_t cmd_cut_from = flag_settings | 0x0CCF; // int8 value, listed below.
// options for cmd_cut_from
constexpr uint8_t cut_from_current_position = 0x0; // The current position at the start of the job
constexpr uint8_t cut_from_user_origin = 0x1; // The user origin (set by cfg_user_origin_x and cfg_user_origin_y) at the start of the job.
constexpr uint8_t cut_from_absolute = 0x2; // Absolute machine coordinates.

// Cut Type
constexpr uint16_t cmd_cut_type = flag_settings | 0x0CC7;
constexpr uint16_t flag_cut_type_scan_uni = 0x0100;
constexpr uint16_t flag_cut_type_scan_bi = 0x0200;

// Possible values for cmd_cut_type
constexpr uint16_t cut_type_normal = 0x0000;
constexpr uint16_t cut_type_scan_uni_x = flag_cut_type_scan_uni | flag_x;
constexpr uint16_t cut_type_scan_uni_y = flag_cut_type_scan_uni | flag_y;
constexpr uint16_t cut_type_scan_bi_x = flag_cut_type_scan_bi | flag_x;
constexpr uint16_t cut_type_scan_bi_y = flag_cut_type_scan_bi | flag_y;

// ----------------------------------------------------------------------------
// 0x6000: Movement -----------------------------------------------------------
// ----------------------------------------------------------------------------
constexpr uint16_t flag_move = 0x6000; // (6 = "G", for "Go")

constexpr uint16_t flag_move_abs = flag_move | 0x0A00;
constexpr uint16_t flag_move_rel = flag_move | 0x0100;

// Discrete Movement Commands: Argument: one signed int32 (micrometers) per indicated axis.
constexpr uint16_t cmd_move_rel_x = flag_move_rel | flag_x;
constexpr uint16_t cmd_move_rel_y = flag_move_rel | flag_y;
constexpr uint16_t cmd_move_rel_z = flag_move_rel | flag_z;
constexpr uint16_t cmd_move_rel_u = flag_move_rel | flag_u;

constexpr uint16_t cmd_move_rel_xy = flag_move_rel | flag_x | flag_y;
constexpr uint16_t cmd_move_rel_xyz = flag_move_rel | flag_x | flag_y | flag_z;
constexpr uint16_t cmd_move_rel_xyzu = flag_move_rel | flag_x | flag_y | flag_z | flag_u;

constexpr uint16_t cmd_move_abs_x =	flag_move_abs | flag_x;
constexpr uint16_t cmd_move_abs_y =	flag_move_abs | flag_y;
constexpr uint16_t cmd_move_abs_z =	flag_move_abs | flag_z;
constexpr uint16_t cmd_move_abs_u =	flag_move_abs | flag_u;

constexpr uint16_t cmd_move_abs_xy = flag_move_abs | flag_x | flag_y;
constexpr uint16_t cmd_move_abs_xyz = flag_move_abs | flag_x | flag_y | flag_z;
constexpr uint16_t cmd_move_abs_xyzu = flag_move_abs | flag_x | flag_y | flag_z | flag_u;

// Continuous jog moves
constexpr uint16_t flag_jog_pos_stop	= flag_move | 0x0200;
constexpr uint16_t flag_jog_pos_start	= flag_move | 0x0300;
constexpr uint16_t flag_jog_neg_stop	= flag_move | 0x0400;
constexpr uint16_t flag_jog_neg_start	= flag_move | 0x0500;

constexpr uint16_t cmd_jog_x_pos_start	= flag_jog_pos_start | flag_x;
constexpr uint16_t cmd_jog_x_pos_stop 	= flag_jog_pos_stop | flag_x;
constexpr uint16_t cmd_jog_x_neg_start 	= flag_jog_neg_start | flag_x;
constexpr uint16_t cmd_jog_x_neg_stop 	= flag_jog_neg_stop | flag_x;

constexpr uint16_t cmd_jog_y_pos_start	= flag_jog_pos_start | flag_y;
constexpr uint16_t cmd_jog_y_pos_stop	= flag_jog_pos_stop | flag_y;
constexpr uint16_t cmd_jog_y_neg_start	= flag_jog_neg_start | flag_y;
constexpr uint16_t cmd_jog_y_neg_stop	= flag_jog_neg_stop | flag_y;

constexpr uint16_t cmd_jog_z_pos_start	= flag_jog_pos_start | flag_z;
constexpr uint16_t cmd_jog_z_pos_stop	= flag_jog_pos_stop | flag_z;
constexpr uint16_t cmd_jog_z_neg_start	= flag_jog_neg_start | flag_z;
constexpr uint16_t cmd_jog_z_neg_stop 	= flag_jog_neg_stop | flag_z;

constexpr uint16_t cmd_jog_u_pos_start	= flag_jog_pos_start | flag_u;
constexpr uint16_t cmd_jog_u_pos_stop	= flag_jog_pos_stop | flag_u;
constexpr uint16_t cmd_jog_u_neg_start	= flag_jog_neg_start | flag_u;
constexpr uint16_t cmd_jog_u_neg_stop 	= flag_jog_neg_stop | flag_u;

// Dwell
constexpr uint16_t cmd_dwell = flag_move | 0x02B; // int32 duration (microseconds)

constexpr int32_t max_dwell = 60000000; // maximum dwell time (microseconds)

// Home
constexpr int16_t flag_home = flag_move | 0x0800;

constexpr int16_t cmd_home_x = flag_home | flag_x;
constexpr int16_t cmd_home_y = flag_home | flag_y;
constexpr int16_t cmd_home_z = flag_home | flag_z;
constexpr int16_t cmd_home_u = flag_home | flag_u;
constexpr int16_t cmd_home_xy = flag_home | flag_x | flag_y;
constexpr int16_t cmd_home_xyz = flag_home | flag_x | flag_y | flag_z;
constexpr int16_t cmd_home_xyzu = flag_home | flag_x | flag_y | flag_z | flag_u;

// ----------------------------------------------------------------------------
// 0x7000: Tool Controls ------------------------------------------------------
// ----------------------------------------------------------------------------
constexpr uint16_t flag_tool = 0x7000; // (7 = "T", for "Tool")

// Air commands: (Air sounds like "FF")
constexpr uint16_t cmd_air_off = flag_tool | 0x0FF0;
constexpr uint16_t cmd_air_on = flag_tool | 0x0FF1;

// ----------------------------------------------------------------------------
// 0x8000: Observable State ---------------------------------------------------
// ----------------------------------------------------------------------------
constexpr uint16_t flag_state = 0x8000; // "eight" rhymes with "state"

// The Get State command will return a 32-bit integer composed of state_* flags.
constexpr uint16_t cmd_get_state = flag_state | 0x057A;

constexpr uint32_t state_idle = 0x0000;				// machine is on but not moving or executing a job
constexpr uint32_t state_moving = 0x0001;			// machine is moving, either by job or user control
constexpr uint32_t state_executing_job = 0x0002;	// machine is executing a job
constexpr uint32_t state_executing_frame = 0x0004;	// machine is executing framing commands.
constexpr uint32_t state_paused = 0x0008;			// machine is paused mid-job.
constexpr uint32_t state_receiving = 0x0010;		// machine is receiving a file from LB
constexpr uint32_t state_file_loaded = 0x0020;		// a file is loaded and ready to execute.
constexpr uint32_t state_computing = 0x0040;		// machine is performing a non-trivial computation.

// Position Queries
constexpr uint16_t flag_pos = flag_state | 0x0100;

constexpr uint16_t cmd_pos_x = flag_pos | flag_x; // return int32 micrometers
constexpr uint16_t cmd_pos_y = flag_pos | flag_y; // return int32 micrometers
constexpr uint16_t cmd_pos_z = flag_pos | flag_z; // return int32 micrometers
constexpr uint16_t cmd_pos_u = flag_pos | flag_u; // return int32 micrometers

constexpr uint16_t cmd_pos_xy = flag_pos | flag_x | flag_y; // return int32 micrometers (x, y)
constexpr uint16_t cmd_pos_xyz = flag_pos | flag_x | flag_y | flag_z; // return int32 micrometers (x, y, z)
constexpr uint16_t cmd_pos_xyzu = flag_pos | flag_x | flag_y | flag_z | flag_u; // return int32 micrometers (x, y, z, u)

// TODO: machine lifespan queries
constexpr uint16_t flag_total_time = flag_state | 0x0800;
constexpr uint16_t cmd_total_on_time = flag_total_time | 0x0001; // TODO: return uint32 seconds
constexpr uint16_t cmd_total_processing_time = flag_total_time | 0x0002; // TODO: return uint32 seconds
constexpr uint16_t cmd_total_laser_on_time = flag_total_time | 0x0003; // TODO: Arg: 1 byte laser intex. TODO: return uint32 seconds

constexpr uint16_t flag_total_travel = flag_state | 0x0900;
constexpr uint16_t cmd_total_travel_x = flag_total_travel | flag_x; // TODO: return uint32 meters
constexpr uint16_t cmd_total_travel_y = flag_total_travel | flag_y; // TODO: return uint32 meters
constexpr uint16_t cmd_total_travel_z = flag_total_travel | flag_z; // TODO: return uint32 meters
constexpr uint16_t cmd_total_travel_u = flag_total_travel | flag_u; // TODO: return uint32 meters

// ----------------------------------------------------------------------------
// 0xC000: Configuration ------------------------------------------------------
// ----------------------------------------------------------------------------
constexpr uint16_t flag_cfg = 0xC000; // "C" for "Config"

// TODO: very few configurations are meaningfully implemented in the simulator.
// units and flag definitions are subject to change.

constexpr uint16_t cfg_head_dist = flag_cfg | 0x001E; // nanometers

constexpr uint16_t cfg_laser1_freq			= flag_cfg | 0x0111; // hz (20000)
constexpr uint16_t cfg_laser1_min_power 	= flag_cfg | 0x0121; // percent
constexpr uint16_t cfg_laser1_max_power 	= flag_cfg | 0x0131; // percent
constexpr uint16_t cfg_laser1_preig_freq	= flag_cfg | 0x0141; // hz (20000)
constexpr uint16_t cfg_laser1_preig_pct		= flag_cfg | 0x0151; // percent * 10
// constexpr uint16_t cfg_laser1_type 		= flag_cfg | 0x0161; // TODO
// glass / RF / RF+preignition ?  (0x8000=multi-tube, enable 1=0x2000, enable 2=0x4000, glass=0, rf No=1, rf/Wpre=2, 0x400=Special Mode)

constexpr uint16_t cfg_laser2_freq 		= flag_cfg | 0x0112;  // hz (20000)
constexpr uint16_t cfg_laser2_min_power	= flag_cfg | 0x0122;  // percent
constexpr uint16_t cfg_laser2_max_power	= flag_cfg | 0x0132;  // percent
constexpr uint16_t cfg_laser2_preig_freq = flag_cfg | 0x0142;  // hz (20000)
constexpr uint16_t cfg_laser2_preig_pct	= flag_cfg | 0x0152;  // percent * 10
// constexpr uint16_t cfg_laser2_type 		= flag_cfg | 0x0161; // TODO

constexpr uint16_t cfg_x_settings			= flag_cfg | flag_x | 0x0A10; // TODO binary flags
constexpr uint16_t cfg_x_step_length 		= flag_cfg | flag_x | 0x0A20; // nanometers?  (micrometers * 1M)
constexpr uint16_t cfg_x_max_speed 			= flag_cfg | flag_x | 0x0A30; // micrometers/sec
constexpr uint16_t cfg_x_jumpoff_speed 		= flag_cfg | flag_x | 0x0A40; // micrometers/sec^2
constexpr uint16_t cfg_x_max_accel 			= flag_cfg | flag_x | 0x0A50; // micrometers/sec^2
constexpr uint16_t cfg_x_breadth 			= flag_cfg | flag_x | 0x0A60; // width in nanometers
constexpr uint16_t cfg_x_key_jumpoff_speed	= flag_cfg | flag_x | 0x0A70; // micrometers/sec
constexpr uint16_t cfg_x_key_accel			= flag_cfg | flag_x | 0x0A80; // micrometers/sec^2
constexpr uint16_t cfg_x_estop_accel		= flag_cfg | flag_x | 0x0A90; // micrometers/sec^2
constexpr uint16_t cfg_x_home_offset		= flag_cfg | flag_x | 0x0AA0; // micrometers
constexpr uint16_t cfg_x_backlash			= flag_cfg | flag_x | 0x0AB0; // micrometers

constexpr uint16_t cfg_y_settings			= flag_cfg | flag_y | 0x0A10; // TODO binary flags
constexpr uint16_t cfg_y_step_length 		= flag_cfg | flag_y | 0x0A20; // nanometers?  (micrometers * 1M)
constexpr uint16_t cfg_y_max_speed 			= flag_cfg | flag_y | 0x0A30; // micrometers/sec
constexpr uint16_t cfg_y_jumpoff_speed 		= flag_cfg | flag_y | 0x0A40; // micrometers/sec^2
constexpr uint16_t cfg_y_max_accel 			= flag_cfg | flag_y | 0x0A50; // micrometers/sec^2
constexpr uint16_t cfg_y_breadth 			= flag_cfg | flag_y | 0x0A60; // width in nanometers
constexpr uint16_t cfg_y_key_jumpoff_speed	= flag_cfg | flag_y | 0x0A70; // micrometers/sec
constexpr uint16_t cfg_y_key_accel			= flag_cfg | flag_y | 0x0A80; // micrometers/sec^2
constexpr uint16_t cfg_y_estop_accel		= flag_cfg | flag_y | 0x0A90; // micrometers/sec^2
constexpr uint16_t cfg_y_home_offset		= flag_cfg | flag_y | 0x0AA0; // micrometers
constexpr uint16_t cfg_y_backlash			= flag_cfg | flag_y | 0x0AB0; // micrometers

constexpr uint16_t cfg_z_settings			= flag_cfg | flag_z | 0x0A10; // TODO binary flags
constexpr uint16_t cfg_z_step_length 		= flag_cfg | flag_z | 0x0A20; // nanometers?  (micrometers * 1M)
constexpr uint16_t cfg_z_max_speed 			= flag_cfg | flag_z | 0x0A30; // micrometers/sec
constexpr uint16_t cfg_z_jumpoff_speed 		= flag_cfg | flag_z | 0x0A40; // micrometers/sec^2
constexpr uint16_t cfg_z_max_accel 			= flag_cfg | flag_z | 0x0A50; // micrometers/sec^2
constexpr uint16_t cfg_z_breadth 			= flag_cfg | flag_z | 0x0A60; // width in nanometers
constexpr uint16_t cfg_z_key_jumpoff_speed	= flag_cfg | flag_z | 0x0A70; // micrometers/sec
constexpr uint16_t cfg_z_key_accel			= flag_cfg | flag_z | 0x0A80; // micrometers/sec^2
constexpr uint16_t cfg_z_estop_accel		= flag_cfg | flag_z | 0x0A90; // micrometers/sec^2
constexpr uint16_t cfg_z_home_offset		= flag_cfg | flag_z | 0x0AA0; // micrometers
constexpr uint16_t cfg_z_backlash			= flag_cfg | flag_z | 0x0AB0; // micrometers

constexpr uint16_t cfg_u_settings			= flag_cfg | flag_u | 0x0A10; // binary flags
constexpr uint16_t cfg_u_step_length 		= flag_cfg | flag_u | 0x0A20; // nanometers?  (micrometers * 1M)
constexpr uint16_t cfg_u_max_speed 			= flag_cfg | flag_u | 0x0A30; // micrometers/sec
constexpr uint16_t cfg_u_jumpoff_speed 		= flag_cfg | flag_u | 0x0A40; // micrometers/sec^2
constexpr uint16_t cfg_u_max_accel 			= flag_cfg | flag_u | 0x0A50; // micrometers/sec^2
constexpr uint16_t cfg_u_breadth 			= flag_cfg | flag_u | 0x0A60; // width in nanometers
constexpr uint16_t cfg_u_key_jumpoff_speed	= flag_cfg | flag_u | 0x0A70; // micrometers/sec
constexpr uint16_t cfg_u_key_accel			= flag_cfg | flag_u | 0x0A80; // micrometers/sec^2
constexpr uint16_t cfg_u_estop_accel		= flag_cfg | flag_u | 0x0A90; // micrometers/sec^2
constexpr uint16_t cfg_u_home_offset		= flag_cfg | flag_u | 0x0AA0; // micrometers
constexpr uint16_t cfg_u_backlash			= flag_cfg | flag_u | 0x0AB0; // micrometers

// Configurables - cut:
constexpr uint16_t cfg_idle_speed		= flag_cfg | 0x0201; // micrometers/sec
constexpr uint16_t cfg_idle_acc			= flag_cfg | 0x0202; // micrometers/sec^2
constexpr uint16_t cfg_idle_delay		= flag_cfg | 0x0203; // ??	// microseconds?
constexpr uint16_t cfg_start_speed		= flag_cfg | 0x0204; // micrometers/sec
constexpr uint16_t cfg_min_acc			= flag_cfg | 0x0205; // micrometers/sec^2
constexpr uint16_t cfg_max_acc			= flag_cfg | 0x0206; // ??	// micrometers/sec^2
constexpr uint16_t cfg_acc_factor_pct	= flag_cfg | 0x0207; // percent
constexpr uint16_t cfg_G0_acc_factor_pct = flag_cfg | 0x0208; // percent
constexpr uint16_t cfg_speed_factor_pct = flag_cfg | 0x0209; // percent

constexpr uint16_t cfg_docking_position_x = flag_cfg | 0x0D00 | flag_x; // micrometers
constexpr uint16_t cfg_docking_position_y = flag_cfg | 0x0D00 | flag_y; // micrometers
constexpr uint16_t cfg_docking_position_z = flag_cfg | 0x0D00 | flag_z; // micrometers

// Configurables - engrave:
constexpr uint16_t cfg_engrave_x_start_speed = flag_cfg | 0x0E50 | flag_x; // micrometers/sec
constexpr uint16_t cfg_engrave_y_start_speed = flag_cfg | 0x0E50 | flag_y; // micrometers/sec

constexpr uint16_t cfg_engrave_x_acc		= flag_cfg | 0x0EA0 | flag_x; // micrometers/sec^2
constexpr uint16_t cfg_engrave_y_acc		= flag_cfg | 0x0EA0 | flag_y; // micrometers/sec^2
constexpr uint16_t cfg_line_shift_speed		= flag_cfg | 0x0E01; // micrometers/sec
constexpr uint16_t cfg_facula_size_pct		= flag_cfg | 0x0E02; // precent*10.0
constexpr uint16_t cfg_engrave_factor_pct	= flag_cfg | 0x0E03; // percent

// Configurables - Homing
constexpr uint16_t cfg_xy_home_speed = flag_cfg | 0x0AC0 | flag_x | flag_y; // micrometers/sec
constexpr uint16_t cfg_z_home_speed = flag_cfg | 0x0AC0 | flag_z; // micrometers/sec
constexpr uint16_t cfg_z_work_speed = flag_cfg | 0x0AD0 | flag_z; // micrometers/sec
constexpr uint16_t cfg_u_home_speed = flag_cfg | 0x0AC0 | flag_u; // micrometers/sec
constexpr uint16_t cfg_u_work_speed = flag_cfg | 0x0AD0 | flag_u; // micrometers/sec

// Configurables - Material
constexpr uint16_t cfg_material_thick = flag_cfg | 0x0301; // micrometers

// Configurables - Focus & misc
constexpr uint16_t cfg_focus_distance	= flag_cfg | 0x0211; // micrometers
constexpr uint16_t cfg_return_location	= flag_cfg | 0x0212; // flags (0 == origin, 0x8000 = absolute origin, 0x4000 = no return)

constexpr uint16_t cfg_reset_delay		= flag_cfg | 0x0213; // milliseconds
constexpr uint16_t cfg_status_on_delay	= flag_cfg | 0x0214; // milliseconds
constexpr uint16_t cfg_status_off_delay	= flag_cfg | 0x0215; // milliseconds
constexpr uint16_t cfg_finish_delay 	= flag_cfg | 0x0216; // milliseconds

// Configuration - Feeder
// flags (0x8000 ? 0x2000 = Last Feeding off, 0x0800 = Progressive feed on)
constexpr uint16_t cfg_feed_flags		= flag_cfg | 0x0217;
constexpr uint16_t cfg_feed_pre_delay	= flag_cfg | 0x0218; // milliseconds
constexpr uint16_t cfg_feed_post_delay	= flag_cfg | 0x0219; // milliseconds
constexpr uint16_t cfg_feed_backlash	= flag_cfg | 0x021A; // micrometers

// Configurables - Rotary
constexpr uint16_t cfg_rotary_enable = flag_cfg | 0x0221; // rotary enable = 0 or 1
constexpr uint16_t cfg_rotary_pulses_per_rotation = flag_cfg | 0x0222; // steps * 1000
constexpr uint16_t cfg_rotary_diameter = flag_cfg | 0x0223; // micrometers

constexpr uint16_t cfg_wireless_panel_fast = flag_cfg | 0x0224; // micrometers/sec
constexpr uint16_t cfg_wireless_panel_slow = flag_cfg | 0x0225; // micrometers/sec

// Configurables - Axis and Autolayout
constexpr uint16_t cfg_autolayout = flag_cfg | 0x0401;
constexpr uint16_t cfg_axis_auto_home = flag_cfg | 0x0402;

// User Origin
constexpr uint16_t cfg_user_origin_x = flag_cfg | 0x0060 | flag_x;
constexpr uint16_t cfg_user_origin_y = flag_cfg | 0x0060 | flag_y;

} // namespace lbp

#endif // LBP_SPEC_H
