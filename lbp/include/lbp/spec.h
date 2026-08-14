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
constexpr int size_max_cmd_args = 32; // no commands require more than eight 4-byte integers.
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
// 0x8000 - State Queries (8 rhymes with "State")
// 0x9000 - Unused
// 0xA000 - Unused
// 0xB000 - Unused
// 0xC000 - Common Configuration (C for "Configuration")
// 0xD000 - Reserved: Vender-specific Configuration
// 0xE000 - Reserved: Extended Future Capabilities
// 0xF000 - Unused
//

constexpr uint16_t mask_class = 0xF000; // Mask for the MSN, indicating command class/category

// ---- LSN Flags (Least Significant Nibble) ----

// Axis Flags
constexpr uint16_t mask_axis = 0x00FF; // entire LSB reserved for axis flags in certain movement commands.
constexpr uint16_t axis_x = 0x0001;
constexpr uint16_t axis_y = 0x0002;
constexpr uint16_t axis_z = 0x0004;
constexpr uint16_t axis_a = 0x0008;
constexpr uint16_t axis_b = 0x0010;
constexpr uint16_t axis_c = 0x0020;
constexpr uint16_t axis_u = 0x0040;
constexpr uint16_t axis_v = 0x0080;

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
constexpr uint16_t cls_laser = 0x1000; // (1 = "L", for "Laser")

constexpr uint16_t cmd_laser_power_min	= cls_laser | 0x05A0; // int8 laser index, int16 percent of max configured power
constexpr uint16_t cmd_laser_power_max	= cls_laser | 0x05A1; // int8 laser index, int16 percent of max configured power
constexpr uint16_t cmd_laser_freq 		= cls_laser | 0x05F0; // int8 laser index, int32 frequency (Hz)
constexpr uint16_t cmd_laser_enable 	= cls_laser | 0x05E1; // int8 laser index
constexpr uint16_t cmd_laser_disable 	= cls_laser | 0x05E2; // int8 laser index

constexpr uint16_t cmd_laser_off 		= cls_laser | 0x05C1; // int8 laser index
constexpr uint16_t cmd_laser_on 		= cls_laser | 0x05C2; // int8 laser index

constexpr uint16_t cmd_focus_z 			= cls_laser | 0x0F00 | axis_z; // arguments TODO
constexpr uint16_t cmd_laser_offset_xy	= cls_laser | 0x0100 | axis_x | axis_y; // int8 laser index, int32 x, y offsets (um) // TODO

constexpr uint16_t cmd_raster_power = cls_laser | 0x05AA; // int8 laser index, up to fifteen 16-bit power % arguments

// ----------------------------------------------------------------------------
// 0x0004: Files --------------------------------------------------------------
// ----------------------------------------------------------------------------
constexpr uint16_t cls_files = 0x4000; // (4 starts with "F", for "Files")

constexpr uint16_t cmd_file_begin		= cls_files | 0x0401; // Arg: int32 file size
constexpr uint16_t cmd_file_end			= cls_files | 0x0402; // Marks the end of a sent file.
constexpr uint16_t cmd_file_chunk		= cls_files | 0x04FC; // Mark a file chunk.

// Filesystem queries:
constexpr uint16_t cmd_flash_available	= cls_files | 0x05FA; // TODO: return int64 bytes
constexpr uint16_t cmd_mainboard_version = cls_files | 0x05B0; // TODO

// ----------------------------------------------------------------------------
// 0x5000: Settings -----------------------------------------------------------
// ----------------------------------------------------------------------------
constexpr uint16_t cls_settings = 0x5000; // (5 = "S", for "Settings")

// Speed commands
constexpr uint16_t setting_speed = cls_settings | 0x0100;

constexpr uint16_t cmd_speed_xy = setting_speed | axis_x | axis_y;
constexpr uint16_t cmd_speed_ab = setting_speed | axis_a | axis_b;
constexpr uint16_t cmd_speed_x = setting_speed | axis_x;
constexpr uint16_t cmd_speed_y = setting_speed | axis_y;
constexpr uint16_t cmd_speed_z = setting_speed | axis_z;
constexpr uint16_t cmd_speed_a = setting_speed | axis_a;
constexpr uint16_t cmd_speed_b = setting_speed | axis_b;
constexpr uint16_t cmd_speed_c = setting_speed | axis_c;
constexpr uint16_t cmd_speed_u = setting_speed | axis_u;
constexpr uint16_t cmd_speed_v = setting_speed | axis_v;

// Boundary commands
constexpr uint16_t setting_bounds_min = cls_settings | 0x0200;
constexpr uint16_t setting_bounds_max = cls_settings | 0x0300;

// Boundary commands: Composed of flags. Argument: one signed int32 (micrometers) per indicated axis.
constexpr uint16_t cmd_bounds_min_x = setting_bounds_min | axis_x;
constexpr uint16_t cmd_bounds_max_x = setting_bounds_max | axis_x;
constexpr uint16_t cmd_bounds_min_y = setting_bounds_min | axis_y;
constexpr uint16_t cmd_bounds_max_y = setting_bounds_max | axis_y;
constexpr uint16_t cmd_bounds_min_z = setting_bounds_min | axis_z;
constexpr uint16_t cmd_bounds_max_z = setting_bounds_max | axis_z;
constexpr uint16_t cmd_bounds_min_a = setting_bounds_min | axis_a;
constexpr uint16_t cmd_bounds_max_a = setting_bounds_max | axis_a;
constexpr uint16_t cmd_bounds_min_b = setting_bounds_min | axis_b;
constexpr uint16_t cmd_bounds_max_b = setting_bounds_max | axis_b;
constexpr uint16_t cmd_bounds_min_c = setting_bounds_min | axis_c;
constexpr uint16_t cmd_bounds_max_c = setting_bounds_max | axis_c;
constexpr uint16_t cmd_bounds_min_u = setting_bounds_min | axis_u;
constexpr uint16_t cmd_bounds_max_u = setting_bounds_max | axis_u;
constexpr uint16_t cmd_bounds_min_v = setting_bounds_min | axis_v;
constexpr uint16_t cmd_bounds_max_v = setting_bounds_max | axis_v;

// When this appears in a job header, all following absolute xy move commands
// should be interpreted as relative to the following options.
constexpr uint16_t cmd_cut_from = cls_settings | 0x0CCF; // arg: int8 value, listed below.
// options for cmd_cut_from
constexpr uint8_t cut_from_current_position = 0x0;
constexpr uint8_t cut_from_user_origin = 0x1;
constexpr uint8_t cut_from_absolute = 0x2;

// Cut Type
constexpr uint16_t cmd_cut_type = cls_settings | 0x0CC7;
constexpr uint16_t scan_uni = 0x0100;
constexpr uint16_t scan_bi = 0x0200;

// Possible values for cmd_cut_type
constexpr uint16_t cut_type_normal = 0x0000;
constexpr uint16_t cut_type_scan_uni_x = scan_uni | axis_x;
constexpr uint16_t cut_type_scan_uni_y = scan_uni | axis_y;
constexpr uint16_t cut_type_scan_bi_x = scan_bi | axis_x;
constexpr uint16_t cut_type_scan_bi_y = scan_bi | axis_y;

// ----------------------------------------------------------------------------
// 0x6000: Movement -----------------------------------------------------------
// ----------------------------------------------------------------------------
constexpr uint16_t cls_move = 0x6000; // (6 = "G", for "Go")
// Home
constexpr int16_t move_home = cls_move | 0x0100;

constexpr int16_t cmd_home_x = move_home | axis_x;
constexpr int16_t cmd_home_y = move_home | axis_y;
constexpr int16_t cmd_home_z = move_home | axis_z;
constexpr int16_t cmd_home_a = move_home | axis_a;
constexpr int16_t cmd_home_b = move_home | axis_b;
constexpr int16_t cmd_home_c = move_home | axis_c;
constexpr int16_t cmd_home_u = move_home | axis_u;
constexpr int16_t cmd_home_v = move_home | axis_v;
constexpr int16_t cmd_home_xy = move_home | axis_x | axis_y;
constexpr int16_t cmd_home_xyz = move_home | axis_x | axis_y | axis_z;
constexpr int16_t cmd_home_xyzu = move_home | axis_x | axis_y | axis_z | axis_u;
constexpr int16_t cmd_home_abc = move_home | axis_a | axis_b | axis_c;
constexpr int16_t cmd_home_xyzabc = move_home | axis_x | axis_y | axis_z | axis_a | axis_b | axis_c;

// Operator Moves: Continuous Jogging
constexpr uint16_t move_jog_start_pos = cls_move | 0x0200;
constexpr uint16_t move_jog_stop_pos = cls_move | 0x0300;
constexpr uint16_t move_jog_start_neg = cls_move | 0x0400;
constexpr uint16_t move_jog_stop_neg = cls_move | 0x0500;

constexpr uint16_t cmd_jog_start_pos_x = move_jog_start_pos | axis_x;
constexpr uint16_t cmd_jog_stop_pos_x = move_jog_stop_pos | axis_x;
constexpr uint16_t cmd_jog_start_neg_x = move_jog_start_neg | axis_x;
constexpr uint16_t cmd_jog_stop_neg_x = move_jog_stop_neg | axis_x;

constexpr uint16_t cmd_jog_start_pos_y = move_jog_start_pos | axis_y;
constexpr uint16_t cmd_jog_stop_pos_y = move_jog_stop_pos | axis_y;
constexpr uint16_t cmd_jog_start_neg_y = move_jog_start_neg | axis_y;
constexpr uint16_t cmd_jog_stop_neg_y = move_jog_stop_neg | axis_y;

constexpr uint16_t cmd_jog_start_pos_z = move_jog_start_pos | axis_z;
constexpr uint16_t cmd_jog_stop_pos_z = move_jog_stop_pos | axis_z;
constexpr uint16_t cmd_jog_start_neg_z = move_jog_start_neg | axis_z;
constexpr uint16_t cmd_jog_stop_neg_z = move_jog_stop_neg | axis_z;

constexpr uint16_t cmd_jog_start_pos_a = move_jog_start_pos | axis_a;
constexpr uint16_t cmd_jog_stop_pos_a = move_jog_stop_pos | axis_a;
constexpr uint16_t cmd_jog_start_neg_a = move_jog_start_neg | axis_a;
constexpr uint16_t cmd_jog_stop_neg_a = move_jog_stop_neg | axis_a;

constexpr uint16_t cmd_jog_start_pos_b = move_jog_start_pos | axis_b;
constexpr uint16_t cmd_jog_stop_pos_b = move_jog_stop_pos | axis_b;
constexpr uint16_t cmd_jog_start_neg_b = move_jog_start_neg | axis_b;
constexpr uint16_t cmd_jog_stop_neg_b = move_jog_stop_neg | axis_b;

constexpr uint16_t cmd_jog_start_pos_c = move_jog_start_pos | axis_c;
constexpr uint16_t cmd_jog_stop_pos_c = move_jog_stop_pos | axis_c;
constexpr uint16_t cmd_jog_start_neg_c = move_jog_start_neg | axis_c;
constexpr uint16_t cmd_jog_stop_neg_c = move_jog_stop_neg | axis_c;

constexpr uint16_t cmd_jog_start_pos_u = move_jog_start_pos | axis_u;
constexpr uint16_t cmd_jog_stop_pos_u = move_jog_stop_pos | axis_u;
constexpr uint16_t cmd_jog_start_neg_u = move_jog_start_neg | axis_u;
constexpr uint16_t cmd_jog_stop_neg_u = move_jog_stop_neg | axis_u;

constexpr uint16_t cmd_jog_start_pos_v = move_jog_start_pos | axis_v;
constexpr uint16_t cmd_jog_stop_pos_v = move_jog_stop_pos | axis_v;
constexpr uint16_t cmd_jog_start_neg_v = move_jog_start_neg | axis_v;
constexpr uint16_t cmd_jog_stop_neg_v = move_jog_stop_neg | axis_v;

// For the next few movement types, we can use this flag for composing absolute or relative moves.
constexpr uint16_t flag_abs = 0x0100;

// Operator Moves: Jog Step (relative) 
constexpr uint16_t move_jog = cls_move | 0x0600;
constexpr uint16_t move_jog_step = move_jog; // relative jog

constexpr uint16_t cmd_jog_step_x = move_jog_step | axis_x;
constexpr uint16_t cmd_jog_step_y = move_jog_step | axis_y;
constexpr uint16_t cmd_jog_step_z = move_jog_step | axis_z;
constexpr uint16_t cmd_jog_step_a = move_jog_step | axis_a;
constexpr uint16_t cmd_jog_step_b = move_jog_step | axis_b;
constexpr uint16_t cmd_jog_step_c = move_jog_step | axis_c;
constexpr uint16_t cmd_jog_step_u = move_jog_step | axis_u;
constexpr uint16_t cmd_jog_step_v = move_jog_step | axis_v;

constexpr uint16_t cmd_jog_step_xy = move_jog_step | axis_x | axis_y;
constexpr uint16_t cmd_jog_step_xyz = move_jog_step | axis_x | axis_y | axis_z;
constexpr uint16_t cmd_jog_step_xyzu = move_jog_step | axis_x | axis_y | axis_z | axis_u;
constexpr uint16_t cmd_jog_step_abc = move_jog_step | axis_a | axis_b | axis_c;
constexpr uint16_t cmd_jog_step_xyzabc = move_jog_step | axis_x | axis_y | axis_z | axis_a | axis_b | axis_c;

// Operator Moves: Jog To (absolute)
constexpr uint16_t move_jog_to = move_jog | flag_abs;

constexpr uint16_t cmd_jog_to_x = move_jog_to | axis_x;
constexpr uint16_t cmd_jog_to_y = move_jog_to | axis_y;
constexpr uint16_t cmd_jog_to_z = move_jog_to | axis_z;
constexpr uint16_t cmd_jog_to_a = move_jog_to | axis_a;
constexpr uint16_t cmd_jog_to_b = move_jog_to | axis_b;
constexpr uint16_t cmd_jog_to_c = move_jog_to | axis_c;
constexpr uint16_t cmd_jog_to_u = move_jog_to | axis_u;
constexpr uint16_t cmd_jog_to_v = move_jog_to | axis_v;

constexpr uint16_t cmd_jog_to_xy = move_jog_to | axis_x | axis_y;
constexpr uint16_t cmd_jog_to_xyz = move_jog_to | axis_x | axis_y | axis_z;
constexpr uint16_t cmd_jog_to_xyzu = move_jog_to | axis_x | axis_y | axis_z | axis_u;
constexpr uint16_t cmd_jog_to_abc = move_jog_to | axis_a | axis_b | axis_c;
constexpr uint16_t cmd_jog_to_xyzabc = move_jog_to | axis_x | axis_y | axis_z | axis_a | axis_b | axis_c;

// Programmed Rapid Commands: move directly without expectation to cut.
constexpr uint16_t move_travel = cls_move | 0x0800;
constexpr uint16_t move_travel_rel = move_travel;
constexpr uint16_t move_travel_abs = move_travel | flag_abs;

// Travel Commands: Argument: one signed int32 (micrometers) per indicated axis.
constexpr uint16_t cmd_travel_rel_x = move_travel_rel | axis_x;
constexpr uint16_t cmd_travel_rel_y = move_travel_rel | axis_y;
constexpr uint16_t cmd_travel_rel_z = move_travel_rel | axis_z;
constexpr uint16_t cmd_travel_rel_a = move_travel_rel | axis_a;
constexpr uint16_t cmd_travel_rel_b = move_travel_rel | axis_b;
constexpr uint16_t cmd_travel_rel_c = move_travel_rel | axis_c;
constexpr uint16_t cmd_travel_rel_u = move_travel_rel | axis_u;
constexpr uint16_t cmd_travel_rel_v = move_travel_rel | axis_v;

constexpr uint16_t cmd_travel_rel_xy = move_travel_rel | axis_x | axis_y;
constexpr uint16_t cmd_travel_rel_xyz = move_travel_rel | axis_x | axis_y | axis_z;
constexpr uint16_t cmd_travel_rel_xyzu = move_travel_rel | axis_x | axis_y | axis_z | axis_u;
constexpr uint16_t cmd_travel_rel_abc = move_travel_rel | axis_a | axis_b | axis_c;
constexpr uint16_t cmd_travel_rel_xyzabc = move_travel_rel | axis_x | axis_y | axis_z | axis_a | axis_b | axis_c;

constexpr uint16_t cmd_travel_abs_x = move_travel_abs | axis_x;
constexpr uint16_t cmd_travel_abs_y = move_travel_abs | axis_y;
constexpr uint16_t cmd_travel_abs_z = move_travel_abs | axis_z;
constexpr uint16_t cmd_travel_abs_a = move_travel_abs | axis_a;
constexpr uint16_t cmd_travel_abs_b = move_travel_abs | axis_b;
constexpr uint16_t cmd_travel_abs_c = move_travel_abs | axis_c;
constexpr uint16_t cmd_travel_abs_u = move_travel_abs | axis_u;
constexpr uint16_t cmd_travel_abs_v = move_travel_abs | axis_v;

constexpr uint16_t cmd_travel_abs_xy = move_travel_abs | axis_x | axis_y;
constexpr uint16_t cmd_travel_abs_xyz = move_travel_abs | axis_x | axis_y | axis_z;
constexpr uint16_t cmd_travel_abs_xyzu = move_travel_abs | axis_x | axis_y | axis_z | axis_u;
constexpr uint16_t cmd_travel_abs_abc = move_travel_abs | axis_a | axis_b | axis_c;
constexpr uint16_t cmd_travel_abs_xyzabc = move_travel_abs | axis_x | axis_y | axis_z | axis_a | axis_b | axis_c;

// Programmed Cut Commands: move while cutting.
constexpr uint16_t move_cut = cls_move | 0x0A00;
constexpr uint16_t move_cut_rel = move_cut;
constexpr uint16_t move_cut_abs = move_cut | flag_abs;

// Cut Commands: Argument: one signed int32 (micrometers) per indicated axis.
constexpr uint16_t cmd_cut_rel_x = move_cut_rel | axis_x;
constexpr uint16_t cmd_cut_rel_y = move_cut_rel | axis_y;
constexpr uint16_t cmd_cut_rel_z = move_cut_rel | axis_z;
constexpr uint16_t cmd_cut_rel_a = move_cut_rel | axis_a;
constexpr uint16_t cmd_cut_rel_b = move_cut_rel | axis_b;
constexpr uint16_t cmd_cut_rel_c = move_cut_rel | axis_c;
constexpr uint16_t cmd_cut_rel_u = move_cut_rel | axis_u;
constexpr uint16_t cmd_cut_rel_v = move_cut_rel | axis_v;

constexpr uint16_t cmd_cut_rel_xy = move_cut_rel | axis_x | axis_y;
constexpr uint16_t cmd_cut_rel_xyz = move_cut_rel | axis_x | axis_y | axis_z;
constexpr uint16_t cmd_cut_rel_xyzu = move_cut_rel | axis_x | axis_y | axis_z | axis_u;
constexpr uint16_t cmd_cut_rel_abc = move_cut_rel | axis_a | axis_b | axis_c;
constexpr uint16_t cmd_cut_rel_xyzabc = move_cut_rel | axis_x | axis_y | axis_z | axis_a | axis_b | axis_c;

constexpr uint16_t cmd_cut_abs_x = move_cut_abs | axis_x;
constexpr uint16_t cmd_cut_abs_y = move_cut_abs | axis_y;
constexpr uint16_t cmd_cut_abs_z = move_cut_abs | axis_z;
constexpr uint16_t cmd_cut_abs_a = move_cut_abs | axis_a;
constexpr uint16_t cmd_cut_abs_b = move_cut_abs | axis_b;
constexpr uint16_t cmd_cut_abs_c = move_cut_abs | axis_c;
constexpr uint16_t cmd_cut_abs_u = move_cut_abs | axis_u;
constexpr uint16_t cmd_cut_abs_v = move_cut_abs | axis_v;

constexpr uint16_t cmd_cut_abs_xy = move_cut_abs | axis_x | axis_y;
constexpr uint16_t cmd_cut_abs_xyz = move_cut_abs | axis_x | axis_y | axis_z;
constexpr uint16_t cmd_cut_abs_xyzu = move_cut_abs | axis_x | axis_y | axis_z | axis_u;
constexpr uint16_t cmd_cut_abs_abc = move_cut_abs | axis_a | axis_b | axis_c;
constexpr uint16_t cmd_cut_abs_xyzabc = move_cut_abs | axis_x | axis_y | axis_z | axis_a | axis_b | axis_c;

// ----------------------------------------------------------------------------
// 0x7000: Tool Controls ------------------------------------------------------
// ----------------------------------------------------------------------------
constexpr uint16_t cls_tool = 0x7000; // (7 = "T", for "Tool")

// Air commands: (Air sounds like "FF")
constexpr uint16_t cmd_air_off = cls_tool | 0x0FF0;
constexpr uint16_t cmd_air_on = cls_tool | 0x0FF1;

// Dwell
constexpr uint16_t cmd_dwell = cls_tool | 0x0D31; // int32 duration (microseconds)
constexpr int32_t max_dwell = 60000000; // maximum dwell time (microseconds)

// ----------------------------------------------------------------------------
// 0x8000: State Queries ------------------------------------------------------
// ----------------------------------------------------------------------------
constexpr uint16_t cls_state_query = 0x8000; // "eight" rhymes with "state"

// The Get State command will return a 32-bit integer composed of state_* flags.
constexpr uint16_t cmd_get_state = cls_state_query | 0x057A;

constexpr uint32_t state_idle = 0x0000;				// machine is on but not moving or executing a job
constexpr uint32_t state_moving = 0x0001;			// machine is moving, either by job or user control
constexpr uint32_t state_executing_job = 0x0002;	// machine is executing a job
constexpr uint32_t state_executing_frame = 0x0004;	// machine is executing framing commands.
constexpr uint32_t state_paused = 0x0008;			// machine is paused mid-job.
constexpr uint32_t state_receiving = 0x0010;		// machine is receiving a file from LB
constexpr uint32_t state_file_loaded = 0x0020;		// a file is loaded and ready to execute.
constexpr uint32_t state_computing = 0x0040;		// machine is performing a non-trivial computation.

// Position Queries
constexpr uint16_t query_pos = cls_state_query | 0x0100;

// return one uint32_t value in axis units per indicated axis.
constexpr uint16_t cmd_pos_x = query_pos | axis_x;
constexpr uint16_t cmd_pos_y = query_pos | axis_y;
constexpr uint16_t cmd_pos_z = query_pos | axis_z;
constexpr uint16_t cmd_pos_a = query_pos | axis_a;
constexpr uint16_t cmd_pos_b = query_pos | axis_b;
constexpr uint16_t cmd_pos_c = query_pos | axis_c;
constexpr uint16_t cmd_pos_u = query_pos | axis_u;
constexpr uint16_t cmd_pos_v = query_pos | axis_v;

constexpr uint16_t cmd_pos_xy = query_pos | axis_x | axis_y;
constexpr uint16_t cmd_pos_xyz = query_pos | axis_x | axis_y | axis_z;
constexpr uint16_t cmd_pos_xyzu = query_pos | axis_x | axis_y | axis_z | axis_u;
constexpr uint16_t cmd_pos_xyza = query_pos | axis_x | axis_y | axis_z | axis_a;
constexpr uint16_t cmd_pos_abc = query_pos | axis_a | axis_b | axis_c;
constexpr uint16_t cmd_pos_xyzabc = query_pos | axis_x | axis_y | axis_z | axis_a | axis_b | axis_c;

// TODO: machine lifespan queries
constexpr uint16_t query_time = cls_state_query | 0x0800;
constexpr uint16_t cmd_on_time			= query_time | 0x0001; // TODO: return uint32 seconds
constexpr uint16_t cmd_processing_time	= query_time | 0x0002; // TODO: return uint32 seconds
constexpr uint16_t cmd_laser_on_time	= query_time | 0x0003; // TODO: Arg: 1 byte laser intex. TODO: return uint32 seconds

constexpr uint16_t query_traversal = cls_state_query | 0x0900;
constexpr uint16_t cmd_traversal_x = query_traversal | axis_x; // TODO: return uint32 meters
constexpr uint16_t cmd_traversal_y = query_traversal | axis_y; // TODO: return uint32 meters
constexpr uint16_t cmd_traversal_z = query_traversal | axis_z; // TODO: return uint32 meters
constexpr uint16_t cmd_traversal_a = query_traversal | axis_a; // TODO: return uint32 meters
constexpr uint16_t cmd_traversal_b = query_traversal | axis_b; // TODO: return uint32 meters
constexpr uint16_t cmd_traversal_c = query_traversal | axis_c; // TODO: return uint32 meters
constexpr uint16_t cmd_traversal_u = query_traversal | axis_u; // TODO: return uint32 meters
constexpr uint16_t cmd_traversal_v = query_traversal | axis_v; // TODO: return uint32 meters

// ----------------------------------------------------------------------------
// 0xC000: Configuration ------------------------------------------------------
// ----------------------------------------------------------------------------
constexpr uint16_t cls_cfg = 0xC000; // "C" for "Config"

// TODO: very few configurations are meaningfully implemented in the simulator.
// codes, units, and flag definitions are subject to change.

// Axis configuration indexes
constexpr uint16_t axis_index_x = 0x01;
constexpr uint16_t axis_index_y = 0x02;
constexpr uint16_t axis_index_z = 0x03;
constexpr uint16_t axis_index_a = 0x04;
constexpr uint16_t axis_index_b = 0x05;
constexpr uint16_t axis_index_c = 0x06;
constexpr uint16_t axis_index_u = 0x07;
constexpr uint16_t axis_index_v = 0x08;

// Axis units
const uint8_t unit_none = 0x00;
const uint8_t unit_micrometers = 0x01;
const uint8_t unit_steps = 0x02;
const uint8_t unit_millidegrees = 0x03;

// Axis Configurations
static constexpr uint16_t axis_cfg_settings				= cls_cfg | 0x0A10;
static constexpr uint16_t axis_cfg_unit					= cls_cfg | 0x0A20;
static constexpr uint16_t axis_cfg_size					= cls_cfg | 0x0A30;
static constexpr uint16_t axis_cfg_home_offset			= cls_cfg | 0x0A40;
static constexpr uint16_t axis_cfg_max_speed			= cls_cfg | 0x0A50;
static constexpr uint16_t axis_cfg_jumpoff_speed		= cls_cfg | 0x0A60;
static constexpr uint16_t axis_cfg_key_jumpoff_speed	= cls_cfg | 0x0A80;
static constexpr uint16_t axis_cfg_max_accel			= cls_cfg | 0x0A90;
static constexpr uint16_t axis_cfg_key_accel			= cls_cfg | 0x0AA0;
static constexpr uint16_t axis_cfg_estop_accel			= cls_cfg | 0x0AB0;
static constexpr uint16_t axis_cfg_backlash				= cls_cfg | 0x0AC0;
static constexpr uint16_t axis_cfg_docking_pos			= cls_cfg | 0x0AD0;

constexpr uint16_t cfg_x_settings			= axis_index_x | axis_settings;
constexpr uint16_t cfg_x_unit				= axis_index_x | axis_unit;
constexpr uint16_t cfg_x_size 				= axis_index_x | axis_size;
constexpr uint16_t cfg_x_home_offset		= axis_index_x | axis_home_offset;
constexpr uint16_t cfg_x_max_speed			= axis_index_x | axis_max_speed;
constexpr uint16_t cfg_x_jumpoff_speed		= axis_index_x | axis_jumpoff_speed;
constexpr uint16_t cfg_x_key_jumpoff_speed	= axis_index_x | axis_key_jumpoff_speed;
constexpr uint16_t cfg_x_max_accel			= axis_index_x | axis_max_accel;
constexpr uint16_t cfg_x_key_accel			= axis_index_x | axis_key_accel;
constexpr uint16_t cfg_x_estol_accel		= axis_index_x | axis_estop_accel;
constexpr uint16_t cfg_x_backlash			= axis_index_x | axis_backlash;
constexpr uint16_t cfg_x_docking_pos		= axis_index_x | axis_docking_pos;

constexpr uint16_t cfg_y_settings			= axis_index_y | axis_settings;
constexpr uint16_t cfg_y_unit				= axis_index_y | axis_unit;
constexpr uint16_t cfg_y_size 				= axis_index_y | axis_size;
constexpr uint16_t cfg_y_home_offset		= axis_index_y | axis_home_offset;
constexpr uint16_t cfg_y_max_speed			= axis_index_y | axis_max_speed;
constexpr uint16_t cfg_y_jumpoff_speed		= axis_index_y | axis_jumpoff_speed;
constexpr uint16_t cfg_y_key_jumpoff_speed	= axis_index_y | axis_key_jumpoff_speed;
constexpr uint16_t cfg_y_max_accel			= axis_index_y | axis_max_accel;
constexpr uint16_t cfg_y_key_accel			= axis_index_y | axis_key_accel;
constexpr uint16_t cfg_y_estol_accel		= axis_index_y | axis_estop_accel;
constexpr uint16_t cfg_y_backlash			= axis_index_y | axis_backlash;
constexpr uint16_t cfg_y_docking_pos		= axis_index_y | axis_docking_pos;

constexpr uint16_t cfg_z_settings			= axis_index_z | axis_settings;
constexpr uint16_t cfg_z_unit				= axis_index_z | axis_unit;
constexpr uint16_t cfg_z_size 				= axis_index_z | axis_size;
constexpr uint16_t cfg_z_home_offset		= axis_index_z | axis_home_offset;
constexpr uint16_t cfg_z_max_speed			= axis_index_z | axis_max_speed;
constexpr uint16_t cfg_z_jumpoff_speed		= axis_index_z | axis_jumpoff_speed;
constexpr uint16_t cfg_z_key_jumpoff_speed	= axis_index_z | axis_key_jumpoff_speed;
constexpr uint16_t cfg_z_max_accel			= axis_index_z | axis_max_accel;
constexpr uint16_t cfg_z_key_accel			= axis_index_z | axis_key_accel;
constexpr uint16_t cfg_z_estol_accel		= axis_index_z | axis_estop_accel;
constexpr uint16_t cfg_z_backlash			= axis_index_z | axis_backlash;
constexpr uint16_t cfg_z_docking_pos		= axis_index_z | axis_docking_pos;

constexpr uint16_t cfg_a_settings			= axis_index_a | axis_settings;
constexpr uint16_t cfg_a_unit				= axis_index_a | axis_unit;
constexpr uint16_t cfg_a_size 				= axis_index_a | axis_size;
constexpr uint16_t cfg_a_home_offset		= axis_index_a | axis_home_offset;
constexpr uint16_t cfg_a_max_speed			= axis_index_a | axis_max_speed;
constexpr uint16_t cfg_a_jumpoff_speed		= axis_index_a | axis_jumpoff_speed;
constexpr uint16_t cfg_a_key_jumpoff_speed	= axis_index_a | axis_key_jumpoff_speed;
constexpr uint16_t cfg_a_max_accel			= axis_index_a | axis_max_accel;
constexpr uint16_t cfg_a_key_accel			= axis_index_a | axis_key_accel;
constexpr uint16_t cfg_a_estol_accel		= axis_index_a | axis_estop_accel;
constexpr uint16_t cfg_a_backlash			= axis_index_a | axis_backlash;
constexpr uint16_t cfg_a_docking_pos		= axis_index_a | axis_docking_pos;

constexpr uint16_t cfg_b_settings			= axis_index_b | axis_settings;
constexpr uint16_t cfg_b_unit				= axis_index_b | axis_unit;
constexpr uint16_t cfg_b_size 				= axis_index_b | axis_size;
constexpr uint16_t cfg_b_home_offset		= axis_index_b | axis_home_offset;
constexpr uint16_t cfg_b_max_speed			= axis_index_b | axis_max_speed;
constexpr uint16_t cfg_b_jumpoff_speed		= axis_index_b | axis_jumpoff_speed;
constexpr uint16_t cfg_b_key_jumpoff_speed	= axis_index_b | axis_key_jumpoff_speed;
constexpr uint16_t cfg_b_max_accel			= axis_index_b | axis_max_accel;
constexpr uint16_t cfg_b_key_accel			= axis_index_b | axis_key_accel;
constexpr uint16_t cfg_b_estol_accel		= axis_index_b | axis_estop_accel;
constexpr uint16_t cfg_b_backlash			= axis_index_b | axis_backlash;
constexpr uint16_t cfg_b_docking_pos		= axis_index_b | axis_docking_pos;

constexpr uint16_t cfg_c_settings			= axis_index_c | axis_settings;
constexpr uint16_t cfg_c_unit				= axis_index_c | axis_unit;
constexpr uint16_t cfg_c_size 				= axis_index_c | axis_size;
constexpr uint16_t cfg_c_home_offset		= axis_index_c | axis_home_offset;
constexpr uint16_t cfg_c_max_speed			= axis_index_c | axis_max_speed;
constexpr uint16_t cfg_c_jumpoff_speed		= axis_index_c | axis_jumpoff_speed;
constexpr uint16_t cfg_c_key_jumpoff_speed	= axis_index_c | axis_key_jumpoff_speed;
constexpr uint16_t cfg_c_max_accel			= axis_index_c | axis_max_accel;
constexpr uint16_t cfg_c_key_accel			= axis_index_c | axis_key_accel;
constexpr uint16_t cfg_c_estol_accel		= axis_index_c | axis_estop_accel;
constexpr uint16_t cfg_c_backlash			= axis_index_c | axis_backlash;
constexpr uint16_t cfg_c_docking_pos		= axis_index_c | axis_docking_pos;

constexpr uint16_t cfg_u_settings			= axis_index_u | axis_settings;
constexpr uint16_t cfg_u_unit				= axis_index_u | axis_unit;
constexpr uint16_t cfg_u_size 				= axis_index_u | axis_size;
constexpr uint16_t cfg_u_home_offset		= axis_index_u | axis_home_offset;
constexpr uint16_t cfg_u_max_speed			= axis_index_u | axis_max_speed;
constexpr uint16_t cfg_u_jumpoff_speed		= axis_index_u | axis_jumpoff_speed;
constexpr uint16_t cfg_u_key_jumpoff_speed	= axis_index_u | axis_key_jumpoff_speed;
constexpr uint16_t cfg_u_max_accel			= axis_index_u | axis_max_accel;
constexpr uint16_t cfg_u_key_accel			= axis_index_u | axis_key_accel;
constexpr uint16_t cfg_u_estol_accel		= axis_index_u | axis_estop_accel;
constexpr uint16_t cfg_u_backlash			= axis_index_u | axis_backlash;
constexpr uint16_t cfg_u_docking_pos		= axis_index_u | axis_docking_pos;

constexpr uint16_t cfg_v_settings			= axis_index_v | axis_settings;
constexpr uint16_t cfg_v_unit				= axis_index_v | axis_unit;
constexpr uint16_t cfg_v_size 				= axis_index_v | axis_size;
constexpr uint16_t cfg_v_home_offset		= axis_index_v | axis_home_offset;
constexpr uint16_t cfg_v_max_speed			= axis_index_v | axis_max_speed;
constexpr uint16_t cfg_v_jumpoff_speed		= axis_index_v | axis_jumpoff_speed;
constexpr uint16_t cfg_v_key_jumpoff_speed	= axis_index_v | axis_key_jumpoff_speed;
constexpr uint16_t cfg_v_max_accel			= axis_index_v | axis_max_accel;
constexpr uint16_t cfg_v_key_accel			= axis_index_v | axis_key_accel;
constexpr uint16_t cfg_v_estol_accel		= axis_index_v | axis_estop_accel;
constexpr uint16_t cfg_v_backlash			= axis_index_v | axis_backlash;
constexpr uint16_t cfg_v_docking_pos		= axis_index_v | axis_docking_pos;

// User Origin
constexpr uint16_t cfg_user_origin_x = cls_cfg | 0x0060 | axis_index_x;
constexpr uint16_t cfg_user_origin_y = cls_cfg | 0x0060 | axis_index_y;

constexpr uint16_t cfg_head_dist = cls_cfg | 0x001E;

constexpr uint16_t cfg_laser1_freq		= cls_cfg | 0x0111; // hz (20000)
constexpr uint16_t cfg_laser1_min_power = cls_cfg | 0x0121; // percent
constexpr uint16_t cfg_laser1_max_power = cls_cfg | 0x0131; // percent
constexpr uint16_t cfg_laser1_preig_freq = cls_cfg | 0x0141; // hz (20000)
constexpr uint16_t cfg_laser1_preig_pct	= cls_cfg | 0x0151; // percent * 10
constexpr uint16_t cfg_laser1_type		= cls_cfg | 0x0161; // TODO

constexpr uint16_t cfg_laser2_freq 		= cls_cfg | 0x0112;  // hz (20000)
constexpr uint16_t cfg_laser2_min_power	= cls_cfg | 0x0122;  // percent
constexpr uint16_t cfg_laser2_max_power	= cls_cfg | 0x0132;  // percent
constexpr uint16_t cfg_laser2_preig_freq = cls_cfg | 0x0142;  // hz (20000)
constexpr uint16_t cfg_laser2_preig_pct	= cls_cfg | 0x0152;  // percent * 10
constexpr uint16_t cfg_laser2_type 		= cls_cfg | 0x0162; // TODO

// Configurables - cut:
constexpr uint16_t cfg_idle_speed		= cls_cfg | 0x0201; // micrometers/sec
constexpr uint16_t cfg_idle_acc			= cls_cfg | 0x0202; // micrometers/sec^2
constexpr uint16_t cfg_idle_delay		= cls_cfg | 0x0203; // microseconds
constexpr uint16_t cfg_start_speed		= cls_cfg | 0x0204; // micrometers/sec
constexpr uint16_t cfg_min_acc			= cls_cfg | 0x0205; // micrometers/sec^2
constexpr uint16_t cfg_max_acc			= cls_cfg | 0x0206; // micrometers/sec^2
constexpr uint16_t cfg_acc_factor_pct	= cls_cfg | 0x0207; // percent
constexpr uint16_t cfg_G0_acc_factor_pct = cls_cfg | 0x0208; // percent
constexpr uint16_t cfg_speed_factor_pct = cls_cfg | 0x0209; // percent

// Configurables - engrave:
constexpr uint16_t cfg_engrave_x_start_speed = cls_cfg | 0x0E50 | axis_x; // micrometers/sec
constexpr uint16_t cfg_engrave_y_start_speed = cls_cfg | 0x0E50 | axis_y; // micrometers/sec

constexpr uint16_t cfg_engrave_x_acc		= cls_cfg | 0x0EA0 | axis_x; // micrometers/sec^2
constexpr uint16_t cfg_engrave_y_acc		= cls_cfg | 0x0EA0 | axis_y; // micrometers/sec^2
constexpr uint16_t cfg_line_shift_speed		= cls_cfg | 0x0E01; // micrometers/sec
constexpr uint16_t cfg_facula_size_pct		= cls_cfg | 0x0E02; // precent*10.0
constexpr uint16_t cfg_engrave_factor_pct	= cls_cfg | 0x0E03; // percent

// Configurables - Homing
constexpr uint16_t cfg_xy_home_speed = cls_cfg | 0x0AC0 | axis_index_x | axis_index_y; // micrometers/sec
constexpr uint16_t cfg_z_home_speed = cls_cfg | 0x0AC0 | axis_index_z; // micrometers/sec
constexpr uint16_t cfg_z_work_speed = cls_cfg | 0x0AD0 | axis_index_z; // micrometers/sec
constexpr uint16_t cfg_u_home_speed = cls_cfg | 0x0AC0 | axis_index_u; // micrometers/sec
constexpr uint16_t cfg_u_work_speed = cls_cfg | 0x0AD0 | axis_index_u; // micrometers/sec

// Configurables - Material
constexpr uint16_t cfg_material_thick = cls_cfg | 0x0301; // micrometers

// Configurables - Focus & misc
constexpr uint16_t cfg_focus_distance	= cls_cfg | 0x0211; // micrometers
constexpr uint16_t cfg_return_location	= cls_cfg | 0x0212; // flags (0 == origin, 0x8000 = absolute origin, 0x4000 = no return)

constexpr uint16_t cfg_reset_delay		= cls_cfg | 0x0213; // milliseconds
constexpr uint16_t cfg_status_on_delay	= cls_cfg | 0x0214; // milliseconds
constexpr uint16_t cfg_status_off_delay	= cls_cfg | 0x0215; // milliseconds
constexpr uint16_t cfg_finish_delay 	= cls_cfg | 0x0216; // milliseconds

// Configuration - Feeder
constexpr uint16_t cfg_feed_pre_delay	= cls_cfg | 0x0218; // milliseconds
constexpr uint16_t cfg_feed_post_delay	= cls_cfg | 0x0219; // milliseconds
constexpr uint16_t cfg_feed_backlash	= cls_cfg | 0x021A; // micrometers

// Configurables - Rotary
constexpr uint16_t cfg_rotary_enable = cls_cfg | 0x0221; // rotary enable = 0 or 1
constexpr uint16_t cfg_rotary_pulses_per_rotation = cls_cfg | 0x0222; // steps * 1000
constexpr uint16_t cfg_rotary_diameter = cls_cfg | 0x0223; // micrometers

constexpr uint16_t cfg_wireless_panel_fast = cls_cfg | 0x0224; // micrometers/sec
constexpr uint16_t cfg_wireless_panel_slow = cls_cfg | 0x0225; // micrometers/sec

// Configurables - Axis
constexpr uint16_t cfg_axis_auto_home = cls_cfg | 0x0402;

// Other boolean settings
// Laser 1 Output Signal
// Laser 2 Output Signal

} // namespace lbp

#endif // LBP_SPEC_H
