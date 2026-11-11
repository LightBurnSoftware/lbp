// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#ifndef LBP_SPEC_H
#define LBP_SPEC_H

#include <cstdint>

// Specification for the LightBurn Protocol.
// Currently in Alpha state. Codes and framing are subject to change.

namespace lbp {

//header of every message
constexpr uint32_t magic_start = 0x4452474E; //'DRGN' in ascii, short for "dragon".

// Message layout: [magic 4][txid 2][cmd 2][size 2][args n][crc 2]
// size counts the args only (may be 0). crc covers txid, cmd, size, and args.

//sizes of message components
constexpr uint8_t size_start = sizeof(magic_start); // 4 bytes for start header
constexpr uint8_t size_txid = 2; // 2 bytes for the transaction id
constexpr uint8_t size_cmd = 2; // 2 bytes for the lbp command code.
constexpr uint8_t size_len = 2; // 2 bytes to store the argument size
constexpr uint8_t size_crc = 2; // 2 bytes to store crc16 at end of message
constexpr uint8_t size_header = size_start + size_txid + size_cmd + size_len; // everything before the args.
constexpr uint8_t size_header_footer = size_header + size_crc; // size of everything *except* the args.
constexpr uint8_t size_min_args = 0; // a message may have no arguments.
constexpr uint8_t size_cfg_args = 4; // all cfg args are a single 4-byte value.
constexpr uint8_t size_min_msg = size_header_footer + size_min_args; // size of the smallest permitted message.
constexpr uint8_t size_txid_offset = size_start; // index of the transaction id
constexpr uint8_t size_cmd_offset = size_txid_offset + size_txid; // index of the command code
constexpr uint8_t size_len_offset = size_cmd_offset + size_cmd; // index of the argument size
constexpr uint8_t size_args_offset = size_header; // index of the first argument byte
constexpr uint8_t size_crc_offset = size_txid_offset; // index of the first byte covered by the checksum

constexpr uint16_t txid_none = 0; // transaction id for messages that aren't part of a request/response pair: job file
								  // contents and unsolicited firmware messages. Firmware ignores txid 0 from the connection.

// Protocol version exchanged in cmd_handshake: upper byte major, lower byte minor. See lbp/version.h.
constexpr uint16_t protocol_version = 0x000A; // 0.10

// sizes of common msgs and args (defined to facilitate static allocations).
constexpr int size_max_cmd_args = 32; // no commands require more than eight 4-byte integers.
constexpr int size_max_cmd_msg = size_header_footer + size_max_cmd_args; // largest expected command message.
constexpr int size_file_msg = 512; // total size of a file message.
constexpr int size_file_args = size_file_msg - size_header_footer; // largest args of any message.
constexpr int size_file_chunk = size_file_args - 4; // max data in a cmd_file_chunk, after its uint32 offset.

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

// NOTE: Only values that begin with the prefix `cmd_` or `cfg_` are valid commands. All other values
// defined in this header are either partial values used to construct commands codes,
// dimensional constants (defined above), or pre-set values used as command arguments.

constexpr uint16_t mask_class = 0xF000; // Mask for the MSN, indicating command class/category

// ---- LSB Flags (Least Significant Byte) ----

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
constexpr uint16_t cmd_stop 	= 0x0CFF;	// Immediately: stop all movement, turn off every laser, end any job or frame,
											// and discard the command buffer. Never refused. Does not clear a fault.
constexpr uint16_t cmd_clear_fault = 0x0CEE;	// Acknowledge and clear a latched fault (state_fault). Refused with err_fault
												// while the cause is still present. With no fault, does nothing. Resumes nothing.
												// TODO: Not yet sent by Lightburn.

constexpr uint16_t cmd_frame_begin = 0x0FAB; // Marks beginning of a frame commands.
constexpr uint16_t cmd_frame_end = 0x0FAE; // Marks the end of frame commands.

constexpr uint16_t cmd_commit_cfg = 0x0CCC; // commit changes sent with cfg_ commands.

// Sent by the firmware instead of the normal response when it rejects a request.
// Carries the rejected request's transaction id. Args: uint16 rejected cmd, uint16 err_* code, uint32 detail.
constexpr uint16_t cmd_nack = 0x0BAD;
constexpr uint8_t size_nack_args = 2 + 2 + 4;

// Sent by the firmware, unsolicited (txid 0), when something happens. Never acknowledged or retried.
// Args: uint8 event kind, uint16 code, uint32 detail. See the event_* kinds below.
constexpr uint16_t cmd_event = 0x0E7E;
constexpr uint8_t size_event_args = 1 + 2 + 4;

// ----------------------------------------------------------------------------
// Error codes for cmd_nack ---------------------------------------------------
// ----------------------------------------------------------------------------
// Codes are loosely grouped by upper byte. Detail is code-specific (noted below), 0 if unused.
// Error codes are works in progress and subject to changes and additions.

// 0x01xx: the request itself is invalid.
constexpr uint16_t err_invalid_cmd		= 0x0101; // command not recognized or not implemented by this firmware.
constexpr uint16_t err_invalid_length	= 0x0102; // argument size wrong for this command. detail: expected size.
constexpr uint16_t err_invalid_arg		= 0x0103; // argument value invalid or out of range. detail: index of the bad argument.

// 0x02xx: the machine can't do it right now.
constexpr uint16_t err_buffer_full		= 0x0201; // command buffer full; retry shortly.
constexpr uint16_t err_wrong_state		= 0x0202; // command not allowed in the current state. detail: state_* flags.
constexpr uint16_t err_fault			= 0x0203; // refused because a fault is active (see state_fault).
constexpr uint16_t err_canceled			= 0x0204; // stopped (cmd_stop or an operator stop) before it finished.

// 0x04xx: filesystem-related error.
constexpr uint16_t err_storage_full		= 0x0401; // not enough space for the file.
constexpr uint16_t err_transfer			= 0x0402; // chunk/end without a file_begin, or a chunk past the file's size. Detail: the declared size, or 0.
constexpr uint16_t err_invalid_file		= 0x0403; // file failed validation (checksum, length, malformed contents).
constexpr uint16_t err_invalid_header	= 0x0404; // file header not recognized or not supported. Detail: byte offset of the offending field.
constexpr uint16_t err_file_mismatch	= 0x0405; // a valid stored file does not fit the current configuration. Detail: byte offset of the header field that doesn't match.

// 0xD0xx: reserved for vendor-specific codes.

constexpr uint16_t err_unknown = 0xFFFF; // catch-all.

// Event kinds for cmd_event. 0 is not a valid kind. LightBurn ignores unknown kinds, and handles
// unknown codes within a known kind generically.
constexpr uint8_t event_fault	= 0x01; // the machine can't operate until cleared (state_fault is set). code: err_* reason. detail: context.
constexpr uint8_t event_warning	= 0x02; // something is wrong but the machine is still operable. code: warning code. detail: context.
constexpr uint8_t event_input	= 0x03; // an operator input happened. code: input_*. detail: 0.
constexpr uint8_t event_result	= 0x04; // a long-running request finished. code: 0 on success, else err_*. detail: the request's txid.
constexpr uint8_t event_job_progress = 0x05; // progress of a job run from a file. code: progress_* (the unit). detail: per code.

// Codes for event_input.
constexpr uint16_t input_start	= 0x0001; // start marking (typically a foot pedal)
constexpr uint16_t input_frame	= 0x0002; // start framing
constexpr uint16_t input_stop	= 0x0003; // stop marking

// Codes for event_job_progress: the unit of detail. 0 is not a valid unit.
// progress_bytes_executed: detail = bytes of the job file whose messages have finished executing, i.e. the starting
// offset of the message running now (executed, not read ahead); the file size when the job finishes.
// lbp::Parser::messageOffset() gives each message's offset.
constexpr uint16_t progress_bytes_executed = 0x0001;

// ----------------------------------------------------------------------------
// 0x1000: Laser Commands -----------------------------------------------------
// ----------------------------------------------------------------------------
constexpr uint16_t cls_laser = 0x1000; // (1 = "L", for "Laser")

constexpr uint16_t cmd_laser_power_min	= cls_laser | 0x05A0; // uint8 laser index, uint16 power (65535 = 100% of max configured power)
constexpr uint16_t cmd_laser_power_max	= cls_laser | 0x05A1; // uint8 laser index, uint16 power (65535 = 100% of max configured power)
constexpr uint16_t cmd_laser_freq 		= cls_laser | 0x05F0; // uint8 laser index, int32 frequency (Hz)
constexpr uint16_t cmd_laser_enable 	= cls_laser | 0x05E1; // uint8 laser index
constexpr uint16_t cmd_laser_disable 	= cls_laser | 0x05E2; // uint8 laser index

// Fire in place (dots, pierces). Cut moves light the enabled lasers and travel moves darken them on their own;
// these are only needed to fire without moving. A laser lit by cmd_laser_on stays lit until cmd_laser_off,
// cmd_laser_disable, or a travel move. cmd_laser_on for a laser that isn't enabled has no effect (checked when it
// runs, so it isn't refused). Any laser command naming a laser the machine doesn't have: err_invalid_arg, detail 0.
// See spec.md, "Moves and the Laser" and "Firing in Place".
constexpr uint16_t cmd_laser_off 		= cls_laser | 0x05C1; // uint8 laser index
constexpr uint16_t cmd_laser_on 		= cls_laser | 0x05C2; // uint8 laser index

constexpr uint16_t cmd_focus_z 			= cls_laser | 0x0F00 | axis_z;
constexpr uint16_t cmd_laser_offset_xy	= cls_laser | 0x0100 | axis_x | axis_y; // uint8 laser index, int32 x, y offsets (um) // TODO

constexpr uint16_t cmd_raster_power = cls_laser | 0x05AA; // uint8 laser index, up to fifteen 16-bit power % arguments

// GALVO: pulse width for MOPA fiber sources. Firmware should allow the source to settle after a change before marking.
constexpr uint16_t cmd_laser_pulse_width = cls_laser | 0x05D0; // GALVO (stub): uint8 laser index, int32 pulse width (ns)

// Guide light: a low-power aiming beam that shows where a laser source's beam will land (a galvo's
// red dot, or a gantry's pointer). Not a laser: it has no power or frequency and is never enabled.
// The index selects whose aiming beam. Firmware with one aiming beam shared by all sources
// implements index 0 only and NACKs other indices with err_invalid_arg (detail 0).
constexpr uint16_t cmd_guide_light_off = cls_laser | 0x0FB1; // uint8 laser index
constexpr uint16_t cmd_guide_light_on = cls_laser | 0x0FB2; // uint8 laser index

// Focus light: separate pointer(s) used to set focus height. Not a guide light.
constexpr uint16_t cmd_focus_light_off = cls_laser | 0x0FC1; // no args
constexpr uint16_t cmd_focus_light_on = cls_laser | 0x0FC2; // no args

// ----------------------------------------------------------------------------
// 0x4000: Files --------------------------------------------------------------
// ----------------------------------------------------------------------------
constexpr uint16_t cls_files = 0x4000; // (4 starts with "F", for "Files")

// Sending a file (see spec.md, "Receiving a File"):
constexpr uint16_t cmd_file_begin		= cls_files | 0x0401; // Args: uint8 file type, uint32 file size (bytes)
constexpr uint16_t cmd_file_end			= cls_files | 0x0402; // Args: uint32 CRC-32 (zlib) of the whole file.
															   // Refused with err_invalid_file if it doesn't match.
constexpr uint16_t cmd_file_chunk		= cls_files | 0x04FC; // Args: uint32 offset (bytes), then 0..size_file_chunk
															   // bytes of data, written at that offset. Any order.
															   // Refused with err_transfer if it would end past the size.

// Filesystem queries:
constexpr uint16_t cmd_flash_available	= cls_files | 0x05FA; // TODO: return int64 bytes

// File types, for cmd_file_begin. 0 is not a valid type.
constexpr uint8_t file_type_job			= 0x01; // a list of LBP messages; cmd_execute runs it
constexpr uint8_t file_type_correction	= 0x02; // galvo lens correction table: an .lbpcor file (below)
constexpr uint16_t cmd_mainboard_version = cls_files | 0x05B0; // TODO

// GALVO: .lbpcor, the galvo lens correction table (file_type_correction). See spec.md, "Lens Correction Files".
// Layout, big-endian: header, then grid_x * grid_y (dx, dy) int32 micrometer pairs (row-major, x fastest).
// No checksum of its own: the transfer's cmd_file_end carries the file's CRC-32.
// Header: uint32 magic, uint16 version, uint16 header_size, uint16 grid_x, uint16 grid_y, uint16 layers,
//         uint16 flags, int32 field_size (um; the field is square).
// Readers skip header bytes beyond the fields they know, using header_size.
constexpr uint32_t lbpcor_magic			= 0x4C425043; // 'LBPC'
constexpr uint16_t lbpcor_version		= 1;
constexpr uint16_t lbpcor_header_size	= 20; // size of the version 1 header, in bytes
constexpr uint16_t lbpcor_field_size_offset = 16; // byte offset of field_size in the header
constexpr uint16_t lbpcor_grid_size		= 65; // grid_x and grid_y: 65 x 65 nodes
constexpr uint16_t lbpcor_layers		= 1;
// flags: which linear corrections are baked into the table. The firmware ignores them; LightBurn uses
// identity for each baked-in part of its own field transform so it isn't applied twice.
constexpr uint16_t lbpcor_flag_scale	= 0x0001;
constexpr uint16_t lbpcor_flag_rotation	= 0x0002;
constexpr uint16_t lbpcor_flag_offset	= 0x0004;
constexpr uint16_t lbpcor_flags_reserved = 0xFFF8; // must be 0

// ----------------------------------------------------------------------------
// 0x5000: Settings -----------------------------------------------------------
// ----------------------------------------------------------------------------
constexpr uint16_t cls_settings = 0x5000; // (5 = "S", for "Settings")

// Speed commands: one family per kind of move. Each family's sub-code mirrors the movement
// sub-code of the moves it governs (cut 0x6A.., travel 0x68.., jog 0x66..).
// Argument: one int32 speed in axis units/second (e.g. micrometers/sec), whatever the axis flags.
// Each speed is sticky until changed. Until a speed has been set, the firmware uses its
// configured default. Homing (cmd_home_*) ignores these and uses the configured home speed.
constexpr uint16_t setting_speed_jog	= cls_settings | 0x0600; // operator moves: cmd_jog_start_*, cmd_jog_step_*, cmd_jog_to_*
constexpr uint16_t setting_speed_travel	= cls_settings | 0x0800; // cmd_travel_* (a galvo "jump")
constexpr uint16_t setting_speed_cut	= cls_settings | 0x0A00; // cmd_cut_* (a galvo "mark")

constexpr uint16_t cmd_cut_speed_xy = setting_speed_cut | axis_x | axis_y;
constexpr uint16_t cmd_cut_speed_ab = setting_speed_cut | axis_a | axis_b;
constexpr uint16_t cmd_cut_speed_x = setting_speed_cut | axis_x;
constexpr uint16_t cmd_cut_speed_y = setting_speed_cut | axis_y;
constexpr uint16_t cmd_cut_speed_z = setting_speed_cut | axis_z;
constexpr uint16_t cmd_cut_speed_a = setting_speed_cut | axis_a;
constexpr uint16_t cmd_cut_speed_b = setting_speed_cut | axis_b;
constexpr uint16_t cmd_cut_speed_c = setting_speed_cut | axis_c;
constexpr uint16_t cmd_cut_speed_u = setting_speed_cut | axis_u;
constexpr uint16_t cmd_cut_speed_v = setting_speed_cut | axis_v;

constexpr uint16_t cmd_travel_speed_xy = setting_speed_travel | axis_x | axis_y;
constexpr uint16_t cmd_travel_speed_ab = setting_speed_travel | axis_a | axis_b;
constexpr uint16_t cmd_travel_speed_x = setting_speed_travel | axis_x;
constexpr uint16_t cmd_travel_speed_y = setting_speed_travel | axis_y;
constexpr uint16_t cmd_travel_speed_z = setting_speed_travel | axis_z;
constexpr uint16_t cmd_travel_speed_a = setting_speed_travel | axis_a;
constexpr uint16_t cmd_travel_speed_b = setting_speed_travel | axis_b;
constexpr uint16_t cmd_travel_speed_c = setting_speed_travel | axis_c;
constexpr uint16_t cmd_travel_speed_u = setting_speed_travel | axis_u;
constexpr uint16_t cmd_travel_speed_v = setting_speed_travel | axis_v;

constexpr uint16_t cmd_jog_speed_xy = setting_speed_jog | axis_x | axis_y;
constexpr uint16_t cmd_jog_speed_ab = setting_speed_jog | axis_a | axis_b;
constexpr uint16_t cmd_jog_speed_x = setting_speed_jog | axis_x;
constexpr uint16_t cmd_jog_speed_y = setting_speed_jog | axis_y;
constexpr uint16_t cmd_jog_speed_z = setting_speed_jog | axis_z;
constexpr uint16_t cmd_jog_speed_a = setting_speed_jog | axis_a;
constexpr uint16_t cmd_jog_speed_b = setting_speed_jog | axis_b;
constexpr uint16_t cmd_jog_speed_c = setting_speed_jog | axis_c;
constexpr uint16_t cmd_jog_speed_u = setting_speed_jog | axis_u;
constexpr uint16_t cmd_jog_speed_v = setting_speed_jog | axis_v;

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
constexpr uint8_t cut_from_absolute = 0x2; // GALVO: on a galvo, absolute (0,0) is the centre of the marking field.

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

// GALVO: mirror timing delays, in microseconds. All five are per-layer settings: LightBurn sends all of them
// with each layer's settings, they stay in effect until changed, and the firmware applies them. Until a job sets
// them, the firmware uses its cfg_galvo_* defaults. Dwells (cmd_dwell) never get galvo delays added.
// See spec.md, "Galvo Delays".
constexpr uint16_t setting_galvo_delay = cls_settings | 0x0D00;
// Laser on / off delays ("Laser On TC" / "Laser Off TC"). Arg: int32 us; the on delay may be negative.
// They apply only where a cut move lights or darkens a laser; never to firing in place, never to guide lights.
// On delay: when a cut move lights a dark laser, it fires this long after the move starts (negative: before).
// Off delay: when lasers lit by a cut move go dark (travel, cmd_laser_off), they stay lit this long
// after that cut move ends.
constexpr uint16_t cmd_galvo_laser_on_delay		= setting_galvo_delay | 0x01;
constexpr uint16_t cmd_galvo_laser_off_delay	= setting_galvo_delay | 0x02;
// Polygon delay ("Polygon TC"). Arg: int32 us, the full delay. Applied with the laser on at each vertex between two
// consecutive cut moves, scaled by the turn angle t (0 = straight on, 180 = reversal) that the firmware computes
// from those moves: delay * min(t, 90) / 90. Full delay at 90 degrees or sharper.
constexpr uint16_t cmd_galvo_polygon_delay		= setting_galvo_delay | 0x03;
// End delay ("End TC", RTC "mark delay"). Arg: int32 us. After the last cut move before the lasers go dark, wait
// this long before continuing. Measured from the end of that cut move, like the off delay.
constexpr uint16_t cmd_galvo_end_delay			= setting_galvo_delay | 0x04;
// Travel delay: the galvo "jump delay" (EZCAD/JCZ/RTC), mirror settle time after each travel move.
// Args: int32 min (us), int32 max (us), int32 dist (um). For a travel move of length d:
//   d >= dist (or dist <= 0): max;  otherwise: min + (max - min) * d / dist.  No delay for d == 0.
constexpr uint16_t cmd_galvo_travel_delay		= setting_galvo_delay | 0x05;

// GALVO: apply (1) or stop applying (0) the stored lens correction table (file_type_correction).
// Arg: uint8 enable (0 or 1). Uploading a table stores it but does not apply it; enabling checks the stored
// table against the current configuration, then applies it. Refused with err_wrong_state if no valid table
// is stored, or err_file_mismatch if the table does not fit the configuration (e.g. its field size differs
// from cfg_field_size). Correction is off until enabled, and off again after a new table is uploaded
// or a change to cfg_field_size. Otherwise it keeps its last state.
// LightBurn sends the configuration, uploads the table, then enables it; and disables correction while its
// correction wizard marks an uncorrected test pattern.
constexpr uint16_t cmd_galvo_correction = cls_settings | 0x0C0E;

// ----------------------------------------------------------------------------
// 0x6000: Movement -----------------------------------------------------------
// ----------------------------------------------------------------------------
constexpr uint16_t cls_move = 0x6000; // (6 = "G", for "Go")
// Home: each axis homes at its configured home speed (cfg_*_home_speed); speed commands do not apply.
constexpr uint16_t move_home = cls_move | 0x0100;

constexpr uint16_t cmd_home_x = move_home | axis_x;
constexpr uint16_t cmd_home_y = move_home | axis_y;
constexpr uint16_t cmd_home_z = move_home | axis_z;
constexpr uint16_t cmd_home_a = move_home | axis_a;
constexpr uint16_t cmd_home_b = move_home | axis_b;
constexpr uint16_t cmd_home_c = move_home | axis_c;
constexpr uint16_t cmd_home_u = move_home | axis_u;
constexpr uint16_t cmd_home_v = move_home | axis_v;
constexpr uint16_t cmd_home_xy = move_home | axis_x | axis_y;
constexpr uint16_t cmd_home_xyz = move_home | axis_x | axis_y | axis_z;
constexpr uint16_t cmd_home_xyzu = move_home | axis_x | axis_y | axis_z | axis_u;
constexpr uint16_t cmd_home_abc = move_home | axis_a | axis_b | axis_c;
constexpr uint16_t cmd_home_xyzabc = move_home | axis_x | axis_y | axis_z | axis_a | axis_b | axis_c;

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
constexpr uint16_t cmd_jog_step_xyza = move_jog_step | axis_x | axis_y | axis_z | axis_a;
constexpr uint16_t cmd_jog_step_ab = move_jog_step | axis_a | axis_b;
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
constexpr uint16_t cmd_jog_to_xyza = move_jog_to | axis_x | axis_y | axis_z | axis_a;
constexpr uint16_t cmd_jog_to_ab = move_jog_to | axis_a | axis_b;
constexpr uint16_t cmd_jog_to_abc = move_jog_to | axis_a | axis_b | axis_c;
constexpr uint16_t cmd_jog_to_xyzabc = move_jog_to | axis_x | axis_y | axis_z | axis_a | axis_b | axis_c;

// Programmed Travel Commands (a.k.a. "rapid"; a galvo "jump"): move without firing. Every laser is darkened before the move starts.
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
constexpr uint16_t cmd_travel_rel_xyza = move_travel_rel | axis_x | axis_y | axis_z | axis_a;
constexpr uint16_t cmd_travel_rel_ab = move_travel_rel | axis_a | axis_b;
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
constexpr uint16_t cmd_travel_abs_xyza = move_travel_abs | axis_x | axis_y | axis_z | axis_a;
constexpr uint16_t cmd_travel_abs_ab = move_travel_abs | axis_a | axis_b;
constexpr uint16_t cmd_travel_abs_abc = move_travel_abs | axis_a | axis_b | axis_c;
constexpr uint16_t cmd_travel_abs_xyzabc = move_travel_abs | axis_x | axis_y | axis_z | axis_a | axis_b | axis_c;

// Programmed Cut Commands: move while cutting. Every enabled laser is lit when the move starts.
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
constexpr uint16_t cmd_cut_rel_xyza = move_cut_rel | axis_x | axis_y | axis_z | axis_a;
constexpr uint16_t cmd_cut_rel_ab = move_cut_rel | axis_a | axis_b;
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
constexpr uint16_t cmd_cut_abs_xyza = move_cut_abs | axis_x | axis_y | axis_z | axis_a;
constexpr uint16_t cmd_cut_abs_ab = move_cut_abs | axis_a | axis_b;
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
constexpr uint32_t state_fault = 0x0080;			// a fault prevents operation (interlock, e-stop, hardware). Latched: stays set until cmd_clear_fault.
constexpr uint32_t state_pedal_down = 0x0100;		// the foot pedal (start input) is currently pressed. Presses are reported by event_input.
constexpr uint32_t state_door_open = 0x0200;		// the enclosure door or lid is open.
constexpr uint32_t state_interlock_open = 0x0400;	// the safety interlock circuit is open; the laser cannot fire.
constexpr uint32_t state_needs_home = 0x0800;		// the machine's position can't be trusted (not homed since power-up, or lost
													// in a fault); cleared by homing. Never set by machines without homing.

// Position Queries
constexpr uint16_t query_pos = cls_state_query | 0x0100;

// return one int32_t value in axis units per indicated axis.
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

// Command buffer queries, for flow control when streaming (e.g. framing). The unit is a buffered command: one slot
// per command that goes into the firmware's command buffer (moves, settings, dwells, laser and light switching),
// which it executes in order. Queries and immediate commands (cmd_stop, cmd_pause, ...) never take a slot and are
// never refused with err_buffer_full. Firmware without these queries NACKs them with err_invalid_cmd.
constexpr uint16_t query_buffer = cls_state_query | 0x0A00;
constexpr uint16_t cmd_buffer_capacity	= query_buffer | 0x01; // returns uint32: total slots. Fixed; read once.
constexpr uint16_t cmd_buffer_free		= query_buffer | 0x02; // returns uint32: free slots now. The next that
															   // many buffered commands won't get err_buffer_full.

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

// Axis ids: name an axis in a per-axis cfg code (the code's low nibble, e.g. cfg_x_size = axis_id_x | axis_size)
// and as a cfg value (cfg_rotary_axis). 0 is reserved: it's the group base of each per-axis code, and means
// "no axis" as a value. Ids are labels, not array positions. xy / ab / uv name axis groups.
constexpr uint16_t axis_id_x = 0x01;
constexpr uint16_t axis_id_y = 0x02;
constexpr uint16_t axis_id_z = 0x03;
constexpr uint16_t axis_id_a = 0x04;
constexpr uint16_t axis_id_b = 0x05;
constexpr uint16_t axis_id_c = 0x06;
constexpr uint16_t axis_id_u = 0x07;
constexpr uint16_t axis_id_v = 0x08;
constexpr uint16_t axis_id_xy = 0x0A;
constexpr uint16_t axis_id_ab = 0x0C;
constexpr uint16_t axis_id_uv = 0x0E;

// Axis units
constexpr uint8_t unit_none = 0x00;
constexpr uint8_t unit_micrometers = 0x01;
constexpr uint8_t unit_steps = 0x02;
constexpr uint8_t unit_millidegrees = 0x03;

// Axis Configurations
static constexpr uint16_t axis_settings				= cls_cfg | 0x0A10;
static constexpr uint16_t axis_unit					= cls_cfg | 0x0A20;
static constexpr uint16_t axis_size					= cls_cfg | 0x0A30;
static constexpr uint16_t axis_home_offset			= cls_cfg | 0x0A40;
static constexpr uint16_t axis_max_speed			= cls_cfg | 0x0A50;
static constexpr uint16_t axis_jumpoff_speed		= cls_cfg | 0x0A60;
static constexpr uint16_t axis_key_jumpoff_speed	= cls_cfg | 0x0A80;
static constexpr uint16_t axis_max_accel			= cls_cfg | 0x0A90;
static constexpr uint16_t axis_key_accel			= cls_cfg | 0x0AA0;
static constexpr uint16_t axis_estop_accel			= cls_cfg | 0x0AB0;
static constexpr uint16_t axis_backlash				= cls_cfg | 0x0AC0;
static constexpr uint16_t axis_docking_pos			= cls_cfg | 0x0AD0;
static constexpr uint16_t axis_step_length			= cls_cfg | 0x0AE0;

constexpr uint16_t cfg_x_settings			= axis_id_x | axis_settings;
constexpr uint16_t cfg_x_unit				= axis_id_x | axis_unit;
constexpr uint16_t cfg_x_size 				= axis_id_x | axis_size;
constexpr uint16_t cfg_x_home_offset		= axis_id_x | axis_home_offset;
constexpr uint16_t cfg_x_max_speed			= axis_id_x | axis_max_speed;
constexpr uint16_t cfg_x_jumpoff_speed		= axis_id_x | axis_jumpoff_speed;
constexpr uint16_t cfg_x_key_jumpoff_speed	= axis_id_x | axis_key_jumpoff_speed;
constexpr uint16_t cfg_x_max_accel			= axis_id_x | axis_max_accel;
constexpr uint16_t cfg_x_key_accel			= axis_id_x | axis_key_accel;
constexpr uint16_t cfg_x_estop_accel		= axis_id_x | axis_estop_accel;
constexpr uint16_t cfg_x_backlash			= axis_id_x | axis_backlash;
constexpr uint16_t cfg_x_docking_pos		= axis_id_x | axis_docking_pos;
constexpr uint16_t cfg_x_step_length		= axis_id_x | axis_step_length;

constexpr uint16_t cfg_y_settings			= axis_id_y | axis_settings;
constexpr uint16_t cfg_y_unit				= axis_id_y | axis_unit;
constexpr uint16_t cfg_y_size 				= axis_id_y | axis_size;
constexpr uint16_t cfg_y_home_offset		= axis_id_y | axis_home_offset;
constexpr uint16_t cfg_y_max_speed			= axis_id_y | axis_max_speed;
constexpr uint16_t cfg_y_jumpoff_speed		= axis_id_y | axis_jumpoff_speed;
constexpr uint16_t cfg_y_key_jumpoff_speed	= axis_id_y | axis_key_jumpoff_speed;
constexpr uint16_t cfg_y_max_accel			= axis_id_y | axis_max_accel;
constexpr uint16_t cfg_y_key_accel			= axis_id_y | axis_key_accel;
constexpr uint16_t cfg_y_estop_accel		= axis_id_y | axis_estop_accel;
constexpr uint16_t cfg_y_backlash			= axis_id_y | axis_backlash;
constexpr uint16_t cfg_y_docking_pos		= axis_id_y | axis_docking_pos;
constexpr uint16_t cfg_y_step_length		= axis_id_y | axis_step_length;

constexpr uint16_t cfg_z_settings			= axis_id_z | axis_settings;
constexpr uint16_t cfg_z_unit				= axis_id_z | axis_unit;
constexpr uint16_t cfg_z_size 				= axis_id_z | axis_size;
constexpr uint16_t cfg_z_home_offset		= axis_id_z | axis_home_offset;
constexpr uint16_t cfg_z_max_speed			= axis_id_z | axis_max_speed;
constexpr uint16_t cfg_z_jumpoff_speed		= axis_id_z | axis_jumpoff_speed;
constexpr uint16_t cfg_z_key_jumpoff_speed	= axis_id_z | axis_key_jumpoff_speed;
constexpr uint16_t cfg_z_max_accel			= axis_id_z | axis_max_accel;
constexpr uint16_t cfg_z_key_accel			= axis_id_z | axis_key_accel;
constexpr uint16_t cfg_z_estop_accel		= axis_id_z | axis_estop_accel;
constexpr uint16_t cfg_z_backlash			= axis_id_z | axis_backlash;
constexpr uint16_t cfg_z_docking_pos		= axis_id_z | axis_docking_pos;
constexpr uint16_t cfg_z_step_length		= axis_id_z | axis_step_length;

constexpr uint16_t cfg_a_settings			= axis_id_a | axis_settings;
constexpr uint16_t cfg_a_unit				= axis_id_a | axis_unit;
constexpr uint16_t cfg_a_size 				= axis_id_a | axis_size;
constexpr uint16_t cfg_a_home_offset		= axis_id_a | axis_home_offset;
constexpr uint16_t cfg_a_max_speed			= axis_id_a | axis_max_speed;
constexpr uint16_t cfg_a_jumpoff_speed		= axis_id_a | axis_jumpoff_speed;
constexpr uint16_t cfg_a_key_jumpoff_speed	= axis_id_a | axis_key_jumpoff_speed;
constexpr uint16_t cfg_a_max_accel			= axis_id_a | axis_max_accel;
constexpr uint16_t cfg_a_key_accel			= axis_id_a | axis_key_accel;
constexpr uint16_t cfg_a_estop_accel		= axis_id_a | axis_estop_accel;
constexpr uint16_t cfg_a_backlash			= axis_id_a | axis_backlash;
constexpr uint16_t cfg_a_docking_pos		= axis_id_a | axis_docking_pos;
constexpr uint16_t cfg_a_step_length		= axis_id_a | axis_step_length;

constexpr uint16_t cfg_b_settings			= axis_id_b | axis_settings;
constexpr uint16_t cfg_b_unit				= axis_id_b | axis_unit;
constexpr uint16_t cfg_b_size 				= axis_id_b | axis_size;
constexpr uint16_t cfg_b_home_offset		= axis_id_b | axis_home_offset;
constexpr uint16_t cfg_b_max_speed			= axis_id_b | axis_max_speed;
constexpr uint16_t cfg_b_jumpoff_speed		= axis_id_b | axis_jumpoff_speed;
constexpr uint16_t cfg_b_key_jumpoff_speed	= axis_id_b | axis_key_jumpoff_speed;
constexpr uint16_t cfg_b_max_accel			= axis_id_b | axis_max_accel;
constexpr uint16_t cfg_b_key_accel			= axis_id_b | axis_key_accel;
constexpr uint16_t cfg_b_estop_accel		= axis_id_b | axis_estop_accel;
constexpr uint16_t cfg_b_backlash			= axis_id_b | axis_backlash;
constexpr uint16_t cfg_b_docking_pos		= axis_id_b | axis_docking_pos;
constexpr uint16_t cfg_b_step_length		= axis_id_b | axis_step_length;

constexpr uint16_t cfg_c_settings			= axis_id_c | axis_settings;
constexpr uint16_t cfg_c_unit				= axis_id_c | axis_unit;
constexpr uint16_t cfg_c_size 				= axis_id_c | axis_size;
constexpr uint16_t cfg_c_home_offset		= axis_id_c | axis_home_offset;
constexpr uint16_t cfg_c_max_speed			= axis_id_c | axis_max_speed;
constexpr uint16_t cfg_c_jumpoff_speed		= axis_id_c | axis_jumpoff_speed;
constexpr uint16_t cfg_c_key_jumpoff_speed	= axis_id_c | axis_key_jumpoff_speed;
constexpr uint16_t cfg_c_max_accel			= axis_id_c | axis_max_accel;
constexpr uint16_t cfg_c_key_accel			= axis_id_c | axis_key_accel;
constexpr uint16_t cfg_c_estop_accel		= axis_id_c | axis_estop_accel;
constexpr uint16_t cfg_c_backlash			= axis_id_c | axis_backlash;
constexpr uint16_t cfg_c_docking_pos		= axis_id_c | axis_docking_pos;
constexpr uint16_t cfg_c_step_length		= axis_id_c | axis_step_length;

constexpr uint16_t cfg_u_settings			= axis_id_u | axis_settings;
constexpr uint16_t cfg_u_unit				= axis_id_u | axis_unit;
constexpr uint16_t cfg_u_size 				= axis_id_u | axis_size;
constexpr uint16_t cfg_u_home_offset		= axis_id_u | axis_home_offset;
constexpr uint16_t cfg_u_max_speed			= axis_id_u | axis_max_speed;
constexpr uint16_t cfg_u_jumpoff_speed		= axis_id_u | axis_jumpoff_speed;
constexpr uint16_t cfg_u_key_jumpoff_speed	= axis_id_u | axis_key_jumpoff_speed;
constexpr uint16_t cfg_u_max_accel			= axis_id_u | axis_max_accel;
constexpr uint16_t cfg_u_key_accel			= axis_id_u | axis_key_accel;
constexpr uint16_t cfg_u_estop_accel		= axis_id_u | axis_estop_accel;
constexpr uint16_t cfg_u_backlash			= axis_id_u | axis_backlash;
constexpr uint16_t cfg_u_docking_pos		= axis_id_u | axis_docking_pos;
constexpr uint16_t cfg_u_step_length		= axis_id_u | axis_step_length;

constexpr uint16_t cfg_v_settings			= axis_id_v | axis_settings;
constexpr uint16_t cfg_v_unit				= axis_id_v | axis_unit;
constexpr uint16_t cfg_v_size 				= axis_id_v | axis_size;
constexpr uint16_t cfg_v_home_offset		= axis_id_v | axis_home_offset;
constexpr uint16_t cfg_v_max_speed			= axis_id_v | axis_max_speed;
constexpr uint16_t cfg_v_jumpoff_speed		= axis_id_v | axis_jumpoff_speed;
constexpr uint16_t cfg_v_key_jumpoff_speed	= axis_id_v | axis_key_jumpoff_speed;
constexpr uint16_t cfg_v_max_accel			= axis_id_v | axis_max_accel;
constexpr uint16_t cfg_v_key_accel			= axis_id_v | axis_key_accel;
constexpr uint16_t cfg_v_estop_accel		= axis_id_v | axis_estop_accel;
constexpr uint16_t cfg_v_backlash			= axis_id_v | axis_backlash;
constexpr uint16_t cfg_v_docking_pos		= axis_id_v | axis_docking_pos;
constexpr uint16_t cfg_v_step_length		= axis_id_v | axis_step_length;

// GALVO: Galvo field size, per mirror axis.
// The marking field is the area the mirrors can reach through the lens. It is always square.
// Field size is its span, edge to edge, in micrometers: the same on both mirror axes. Galvo coordinates on the
// wire stay in micrometers; the firmware converts them to its own mirror command units by mapping
// +/- size/2 onto its full command range (which only the firmware knows).
constexpr uint16_t cfg_field_size	= cls_cfg | 0x0F10;	// GALVO: int32 micrometers

// User Origin
constexpr uint16_t cfg_user_origin_x = cls_cfg | 0x0060 | axis_id_x;
constexpr uint16_t cfg_user_origin_y = cls_cfg | 0x0060 | axis_id_y;

// Configuration - Feeder
constexpr uint16_t cfg_feed_pre_delay	= cls_cfg | 0x0218; // milliseconds
constexpr uint16_t cfg_feed_post_delay	= cls_cfg | 0x0219; // milliseconds
constexpr uint16_t cfg_feed_backlash	= cls_cfg | 0x021A; // micrometers

// Configurables - Rotary
constexpr uint16_t cfg_rotary_axis = cls_cfg | 0x022A;				  // argument: axis id (axis_id_*), 0 = none
constexpr uint16_t cfg_rotary_enable = cls_cfg | 0x0221;			  // rotary enable = 0 or 1
constexpr uint16_t cfg_rotary_pulses_per_rotation = cls_cfg | 0x0222; // steps * 1000
constexpr uint16_t cfg_rotary_diameter = cls_cfg | 0x0223;			  // micrometers

constexpr uint16_t cfg_head_dist = cls_cfg | 0x001E;

constexpr uint16_t cfg_laser1_freq		= cls_cfg | 0x0111; // hz (20000)
constexpr uint16_t cfg_laser1_min_power = cls_cfg | 0x0121; // percent (65535 = 100%)
constexpr uint16_t cfg_laser1_max_power = cls_cfg | 0x0131; // percent (65535 = 100%)
constexpr uint16_t cfg_laser1_preig_freq = cls_cfg | 0x0141; // hz (20000)
constexpr uint16_t cfg_laser1_preig_pct	= cls_cfg | 0x0151; // percent (65535 = 100%)
constexpr uint16_t cfg_laser1_type		= cls_cfg | 0x0161; // TODO

constexpr uint16_t cfg_laser2_freq 		= cls_cfg | 0x0112;  // hz (20000)
constexpr uint16_t cfg_laser2_min_power	= cls_cfg | 0x0122;  // percent (65535 = 100%)
constexpr uint16_t cfg_laser2_max_power	= cls_cfg | 0x0132;  // percent (65535 = 100%)
constexpr uint16_t cfg_laser2_preig_freq = cls_cfg | 0x0142;  // hz (20000)
constexpr uint16_t cfg_laser2_preig_pct	= cls_cfg | 0x0152;  // percent (65535 = 100%)
constexpr uint16_t cfg_laser2_type 		= cls_cfg | 0x0162; // TODO

// Configurables - cut:
constexpr uint16_t cfg_idle_speed		= cls_cfg | 0x0501; // micrometers/sec
constexpr uint16_t cfg_idle_acc			= cls_cfg | 0x0502; // micrometers/sec^2
constexpr uint16_t cfg_idle_delay		= cls_cfg | 0x0503; // microseconds
constexpr uint16_t cfg_start_speed		= cls_cfg | 0x0504; // micrometers/sec
constexpr uint16_t cfg_min_acc			= cls_cfg | 0x0505; // micrometers/sec^2
constexpr uint16_t cfg_max_acc			= cls_cfg | 0x0506; // micrometers/sec^2
constexpr uint16_t cfg_acc_factor_pct	= cls_cfg | 0x0507; // percent (65535 = 100%; may exceed 100%)
constexpr uint16_t cfg_G0_acc_factor_pct = cls_cfg | 0x0508; // percent (65535 = 100%; may exceed 100%)
constexpr uint16_t cfg_speed_factor_pct = cls_cfg | 0x0509; // percent (65535 = 100%; may exceed 100%)

// Configurables - engrave:
constexpr uint16_t cfg_engrave_x_start_speed = cls_cfg | 0x0E50 | axis_id_x;
constexpr uint16_t cfg_engrave_y_start_speed = cls_cfg | 0x0E50 | axis_id_y;

constexpr uint16_t cfg_engrave_x_acc		= cls_cfg | 0x0EA0 | axis_id_x; // micrometers/sec^2
constexpr uint16_t cfg_engrave_y_acc		= cls_cfg | 0x0EA0 | axis_id_y; // micrometers/sec^2
constexpr uint16_t cfg_line_shift_speed		= cls_cfg | 0x0E01; // micrometers/sec
constexpr uint16_t cfg_facula_size_pct		= cls_cfg | 0x0E02; // percent (65535 = 100%; may exceed 100%)
constexpr uint16_t cfg_engrave_factor_pct	= cls_cfg | 0x0E03; // percent (65535 = 100%; may exceed 100%)

// Configurables - Homing
constexpr uint16_t cfg_xy_home_speed = cls_cfg | 0x0BC0 | axis_id_xy;
constexpr uint16_t cfg_z_home_speed = cls_cfg | 0x0BC0 | axis_id_z;
constexpr uint16_t cfg_z_work_speed = cls_cfg | 0x0BD0 | axis_id_z;
constexpr uint16_t cfg_u_home_speed = cls_cfg | 0x0BC0 | axis_id_u;
constexpr uint16_t cfg_u_work_speed = cls_cfg | 0x0BD0 | axis_id_u;

// Configurables - Material
constexpr uint16_t cfg_material_thick = cls_cfg | 0x0301; // micrometers

// Configurables - Focus & misc
constexpr uint16_t cfg_focus_distance	= cls_cfg | 0x0211; // micrometers
constexpr uint16_t cfg_return_location	= cls_cfg | 0x0212; // flags (0 == origin, 0x8000 = absolute origin, 0x4000 = no return)

constexpr uint16_t cfg_reset_delay		= cls_cfg | 0x0213; // milliseconds
constexpr uint16_t cfg_status_on_delay	= cls_cfg | 0x0214; // milliseconds
constexpr uint16_t cfg_status_off_delay	= cls_cfg | 0x0215; // milliseconds
constexpr uint16_t cfg_finish_delay 	= cls_cfg | 0x0216; // milliseconds

constexpr uint16_t cfg_wireless_panel_fast = cls_cfg | 0x0224; // micrometers/sec
constexpr uint16_t cfg_wireless_panel_slow = cls_cfg | 0x0225; // micrometers/sec

// Configurables - Axis
constexpr uint16_t cfg_axis_auto_home = cls_cfg | 0x0402;

// GALVO: Configurables - Galvo field
// Field offset, rotation, flips and axis swap are applied by LightBurn before sending.
// Lens correction (bulge, skew, trapezoid, and usually scale) is applied by the firmware, from an
// uploaded .lbpcor table (file_type_correction). See cmd_galvo_correction.

// GALVO: machine defaults for the per-layer galvo delays (cmd_galvo_*), same units.
constexpr uint16_t cfg_galvo_laser_on_delay		= cls_cfg | 0x0F41; // GALVO (stub): microseconds
constexpr uint16_t cfg_galvo_laser_off_delay	= cls_cfg | 0x0F42; // GALVO (stub): microseconds
constexpr uint16_t cfg_galvo_polygon_delay		= cls_cfg | 0x0F43; // GALVO (stub): microseconds
constexpr uint16_t cfg_galvo_end_delay			= cls_cfg | 0x0F44; // GALVO (stub): microseconds
constexpr uint16_t cfg_galvo_travel_delay_min	= cls_cfg | 0x0F45; // GALVO (stub): microseconds
constexpr uint16_t cfg_galvo_travel_delay_max	= cls_cfg | 0x0F46; // GALVO (stub): microseconds
constexpr uint16_t cfg_galvo_travel_delay_dist	= cls_cfg | 0x0F47; // GALVO (stub): micrometers
constexpr uint16_t cfg_travel_speed				= cls_cfg | 0x0F50; // GALVO (stub): default travel (jump) speed, micrometers/sec

// Other boolean settings
// Laser 1 Output Signal
// Laser 2 Output Signal

} // namespace lbp

#endif // LBP_SPEC_H
