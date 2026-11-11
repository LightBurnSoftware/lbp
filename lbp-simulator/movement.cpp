// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include "movement.h"
#include "log.h"

#include <lbp/beio.h>
#include <lbp/spec.h>

#include <algorithm>
#include <bitset>
#include <cmath>

/**
 * @brief Check the argument size of a command this component queues.
 * @param expected Out: the expected argument size, for the nack detail.
 * @return True if args is a valid argument size for cmd.
 */
static bool argSizeValid(uint16_t cmd, size_t args, uint32_t &expected)
{
	const uint16_t high = cmd & 0xFF00;
	if ((cmd & lbp::mask_class) == lbp::cls_move) {
		if (high == lbp::move_home) {
			expected = 0;
		} else {
			expected = 4 * std::bitset<8>(cmd & lbp::mask_axis).count();
		}
		return args == expected;
	}
	if (high == lbp::setting_speed_cut || high == lbp::setting_speed_travel || high == lbp::setting_speed_jog) {
		expected = 4;
		return args == expected;
	}
	switch (cmd) {
	case lbp::cmd_laser_power_min:
	case lbp::cmd_laser_power_max:
		expected = 3;
		break;
	case lbp::cmd_laser_freq:
	case lbp::cmd_laser_pulse_width:
		expected = 5;
		break;
	case lbp::cmd_laser_enable:
	case lbp::cmd_laser_disable:
	case lbp::cmd_laser_on:
	case lbp::cmd_laser_off:
	case lbp::cmd_cut_from:
	case lbp::cmd_guide_light_on:
	case lbp::cmd_guide_light_off:
		expected = 1;
		break;
	case lbp::cmd_galvo_laser_on_delay:
	case lbp::cmd_galvo_laser_off_delay:
	case lbp::cmd_galvo_polygon_delay:
	case lbp::cmd_galvo_end_delay:
		expected = 4;
		break;
	case lbp::cmd_galvo_travel_delay:
		expected = 12; // min us, max us, dist um
		break;
	case lbp::cmd_cut_type:
		expected = 2;
		break;
	case lbp::cmd_dwell:
		expected = 4;
		break;
	case lbp::cmd_raster_power:
		// laser index + up to 15 uint16 powers
		expected = 1;
		return args >= 1 && args <= 31 && (args % 2) == 1;
	default:
		expected = 0;
		break;
	}
	return args == expected;
}

/** @return True for the commands only a galvo supports (queued by MovementSim in galvo mode). */
/** @return true for commands whose first argument is a uint8 laser index. */
static bool isLaserIndexCmd(uint16_t cmd)
{
	switch (cmd) {
	case lbp::cmd_laser_enable:
	case lbp::cmd_laser_disable:
	case lbp::cmd_laser_on:
	case lbp::cmd_laser_off:
	case lbp::cmd_laser_power_min:
	case lbp::cmd_laser_power_max:
	case lbp::cmd_laser_freq:
	case lbp::cmd_laser_pulse_width:
	case lbp::cmd_raster_power:
		return true;
	default:
		return false;
	}
}

static bool isGalvoCmd(uint16_t cmd)
{
	switch (cmd) {
	case lbp::cmd_galvo_laser_on_delay:
	case lbp::cmd_galvo_laser_off_delay:
	case lbp::cmd_galvo_polygon_delay:
	case lbp::cmd_galvo_end_delay:
	case lbp::cmd_galvo_travel_delay:
	case lbp::cmd_laser_pulse_width:
	case lbp::cmd_guide_light_on:
	case lbp::cmd_guide_light_off:
	case lbp::cmd_focus_light_on:
	case lbp::cmd_focus_light_off:
		return true;
	default:
		return false;
	}
}

static inline float normalize(uint16_t power) { return static_cast<float>(power) / 65535.f; }

static inline int64_t clampTo(int64_t v, int64_t lo, int64_t hi)
{
	if (hi > lo) {
		return std::clamp<int64_t>(v, lo, hi);
	}
	return v;
}

/** Wrap v into [lo, hi), for a rotary axis. */
static inline int64_t wrapTo(int64_t v, int64_t lo, int64_t hi)
{
	const int64_t span = hi - lo;
	if (span > 0) {
		v = (v - lo) % span;
		if (v < 0) v += span;
		v += lo;
	}
	return v;
}

/** Convenience structures for quick manipulation of Vec4's against different axis codes. */
struct AxisRef
{
	uint16_t flag;	// command bitmask: axis_x, axis_y, ...
	uint16_t index; // config ordinal: axis_index_x, axis_index_y, ...
	int64_t Vec4::*m;
};
static constexpr AxisRef kAxes[] = {
	{lbp::axis_x, lbp::axis_index_x, &Vec4::x},
	{lbp::axis_y, lbp::axis_index_y, &Vec4::y},
	{lbp::axis_z, lbp::axis_index_z, &Vec4::z},
	{lbp::axis_u, lbp::axis_index_u, &Vec4::u},
};

static constexpr uint16_t kSupportedAxes = lbp::axis_x | lbp::axis_y | lbp::axis_z | lbp::axis_u;

/** @return base + offset * scale, rounded per axis. Pure function of its inputs: no drift. */
static Vec4 advance(const Vec4 &base, const Vec4 &offset, double scale)
{
	Vec4 result;
	for (const AxisRef &a : kAxes) {
		result.*(a.m) = base.*(a.m) + llround(double(offset.*(a.m)) * scale);
	}
	return result;
}

// Gantry travel limits, hardcoded for now (micrometers).
static const Vec4 kGantryMaxPos(300000, 180000, 120000, 60000);

MovementSim::MovementSim(Configuration &config)
	: m_config(config)
	, m_max_pos(kGantryMaxPos)
{
	// Empty
}

void MovementSim::setMode(MachineMode mode)
{
	m_mode = mode;
	m_correction = nullptr;
	m_guide_light = false;
	m_focus_light = false;
	m_warned_galvo_rotary = false;
	refreshRotary(); // the rotary axis may not apply in the new mode
	refreshBounds();
	gLog().push(Log::INFO, mode == MachineMode::Galvo ? "Galvo mode" : "Gantry mode");
}

void MovementSim::refreshBounds()
{
	m_min_pos = Vec4();
	m_max_pos = kGantryMaxPos;
	if (m_mode == MachineMode::Galvo) {
		// The marking field is centred on (0,0). Z and U keep their gantry limits (e.g. a lift axis).
		const int32_t field = m_config.getOrDefault(lbp::cfg_field_size); // square
		m_min_pos.x = -field / 2;
		m_max_pos.x = field / 2;
		m_min_pos.y = -field / 2;
		m_max_pos.y = field / 2;
	}
	m_pos = applyLimits(m_pos);
}

bool MovementSim::process(lbp::MaxPayload &request, OutputQueue &out_q, CmdSource source, uint32_t file_offset)
{
	const uint16_t cmd = request.cmd();

	if (isGalvoCmd(cmd) && m_mode != MachineMode::Galvo) {
		return false; // a gantry doesn't implement these: FirmwareSim answers err_invalid_cmd
	}

	// One aiming beam, shared by all sources: only index 0 exists.
	if ((cmd == lbp::cmd_guide_light_on || cmd == lbp::cmd_guide_light_off) && request.size() == 1
		&& request.data()[0] != 0) {
		if (source == CmdSource::Wire) {
			out_q.nack(request, lbp::err_invalid_arg, 0);
		} else {
			gLog().error("Guide light index must be 0");
		}
		return true;
	}

	// Laser commands name a laser by index; the simulator has two (0 and 1). A laser that doesn't exist is refused on
	// arrival (it never changes). Whether a laser is enabled is checked when a command runs, not here.
	if (isLaserIndexCmd(cmd) && request.size() >= 1 && request.data()[0] > 1) {
		if (source == CmdSource::Wire) {
			out_q.nack(request, lbp::err_invalid_arg, 0);
		} else {
			gLog().error(QString("Laser index %1 doesn't exist").arg(request.data()[0]));
		}
		return true;
	}

	switch (cmd) {
	// Command buffer queries: answered immediately, never buffered. m_cmd_q is the command buffer.
	case lbp::cmd_buffer_capacity:
		out_q.respond(request, static_cast<int32_t>(m_cmd_q.capacity()));
		return true;
	case lbp::cmd_buffer_free:
		out_q.respond(request, static_cast<int32_t>(m_cmd_q.freeSpace()));
		return true;
	case lbp::cmd_pos_x:
		gLog().push(Log::DEBUG, QString("X Axis Query: %1").arg(m_pos.x));
		out_q.respond(request, toWire(m_pos.x));
		return true;
	case lbp::cmd_pos_y:
		gLog().push(Log::DEBUG, QString("Y Axis Query: %1").arg(m_pos.y));
		out_q.respond(request, toWire(m_pos.y));
		return true;
	case lbp::cmd_pos_z:
		gLog().push(Log::DEBUG, QString("Z Axis Query: %1").arg(m_pos.z));
		out_q.respond(request, toWire(m_pos.z));
		return true;
	case lbp::cmd_pos_u:
		gLog().push(Log::DEBUG, QString("U Axis Query: %1").arg(m_pos.u));
		out_q.respond(request, toWire(m_pos.u));
		return true;
	case lbp::cmd_pos_xy: {
		lbp::CmdMsg m = lbp::CmdMsg::responseTo(request, 8);
		m.writeInt(toWire(m_pos.x));
		m.writeInt(toWire(m_pos.y));
		out_q.push(m);
		return true;
	}
	case lbp::cmd_pos_xyz: {
		lbp::CmdMsg m = lbp::CmdMsg::responseTo(request, 12);
		m.writeInt(toWire(m_pos.x));
		m.writeInt(toWire(m_pos.y));
		m.writeInt(toWire(m_pos.z));
		out_q.push(m);
		return true;
	}
	case lbp::cmd_pos_xyzu: {
		gLog().push(Log::DEBUG, "XYZU Axis Query");

		lbp::CmdMsg m = lbp::CmdMsg::responseTo(request, 16);
		m.writeInt(toWire(m_pos.x));
		m.writeInt(toWire(m_pos.y));
		m.writeInt(toWire(m_pos.z));
		m.writeInt(toWire(m_pos.u));
		out_q.push(m);
		return true;
	}
	case lbp::cmd_jog_start_pos_x:
		beginJog(Vec4(1, 0, 0, 0), m_jog_speed.xy);
		out_q.respond(request);
		return true;
	case lbp::cmd_jog_stop_pos_x:
		endJog();
		out_q.respond(request);
		return true;
	case lbp::cmd_jog_start_neg_x:
		beginJog(Vec4(-1, 0, 0, 0), m_jog_speed.xy);
		out_q.respond(request);
		return true;
	case lbp::cmd_jog_stop_neg_x:
		endJog();
		out_q.respond(request);
		return true;
	case lbp::cmd_jog_start_pos_y:
		beginJog(Vec4(0, 1, 0, 0), m_jog_speed.xy);
		out_q.respond(request);
		return true;
	case lbp::cmd_jog_stop_pos_y:
		endJog();
		out_q.respond(request);
		return true;
	case lbp::cmd_jog_start_neg_y:
		beginJog(Vec4(0, -1, 0, 0), m_jog_speed.xy);
		out_q.respond(request);
		return true;
	case lbp::cmd_jog_stop_neg_y:
		endJog();
		out_q.respond(request);
		return true;
	case lbp::cmd_jog_start_pos_z:
		beginJog(Vec4(0, 0, 1, 0), m_jog_speed.z);
		out_q.respond(request);
		return true;
	case lbp::cmd_jog_stop_pos_z:
		endJog();
		out_q.respond(request);
		return true;
	case lbp::cmd_jog_start_neg_z:
		beginJog(Vec4(0, 0, -1, 0), m_jog_speed.z);
		out_q.respond(request);
		return true;
	case lbp::cmd_jog_stop_neg_z:
		endJog();
		out_q.respond(request);
		return true;
	case lbp::cmd_jog_start_pos_u:
		beginJog(Vec4(0, 0, 0, 1), m_jog_speed.u);
		out_q.respond(request);
		return true;
	case lbp::cmd_jog_stop_pos_u:
		endJog();
		out_q.respond(request);
		return true;
	case lbp::cmd_jog_start_neg_u:
		beginJog(Vec4(0, 0, 0, -1), m_jog_speed.u);
		out_q.respond(request);
		return true;
	case lbp::cmd_jog_stop_neg_u:
		endJog();
		out_q.respond(request);
		return true;
	case lbp::cmd_cut_from:
	case lbp::cmd_cut_type:
	case lbp::cmd_home_xy:
	case lbp::cmd_home_z:
	case lbp::cmd_cut_speed_x:
	case lbp::cmd_cut_speed_y:
	case lbp::cmd_cut_speed_xy:
	case lbp::cmd_cut_speed_z:
	case lbp::cmd_cut_speed_u:
	case lbp::cmd_travel_speed_x:
	case lbp::cmd_travel_speed_y:
	case lbp::cmd_travel_speed_xy:
	case lbp::cmd_travel_speed_z:
	case lbp::cmd_travel_speed_u:
	case lbp::cmd_jog_speed_x:
	case lbp::cmd_jog_speed_y:
	case lbp::cmd_jog_speed_xy:
	case lbp::cmd_jog_speed_z:
	case lbp::cmd_jog_speed_u:
	case lbp::cmd_jog_step_x:
	case lbp::cmd_jog_step_y:
	case lbp::cmd_jog_step_z:
	case lbp::cmd_jog_step_u:
	case lbp::cmd_jog_step_xy:
	case lbp::cmd_jog_step_xyz:
	case lbp::cmd_jog_step_xyzu:
	case lbp::cmd_jog_to_x:
	case lbp::cmd_jog_to_y:
	case lbp::cmd_jog_to_z:
	case lbp::cmd_jog_to_u:
	case lbp::cmd_jog_to_xy:
	case lbp::cmd_jog_to_xyz:
	case lbp::cmd_jog_to_xyzu:
	case lbp::cmd_cut_abs_x:
	case lbp::cmd_cut_abs_y:
	case lbp::cmd_cut_abs_z:
	case lbp::cmd_cut_abs_u:
	case lbp::cmd_cut_abs_xy:
	case lbp::cmd_cut_abs_xyz:
	case lbp::cmd_cut_abs_xyzu:
	case lbp::cmd_cut_rel_x:
	case lbp::cmd_cut_rel_y:
	case lbp::cmd_cut_rel_z:
	case lbp::cmd_cut_rel_u:
	case lbp::cmd_cut_rel_xy:
	case lbp::cmd_cut_rel_xyz:
	case lbp::cmd_cut_rel_xyzu:
	case lbp::cmd_travel_abs_x:
	case lbp::cmd_travel_abs_y:
	case lbp::cmd_travel_abs_z:
	case lbp::cmd_travel_abs_u:
	case lbp::cmd_travel_abs_xy:
	case lbp::cmd_travel_abs_xyz:
	case lbp::cmd_travel_abs_xyzu:
	case lbp::cmd_travel_rel_x:
	case lbp::cmd_travel_rel_y:
	case lbp::cmd_travel_rel_z:
	case lbp::cmd_travel_rel_u:
	case lbp::cmd_travel_rel_xy:
	case lbp::cmd_travel_rel_xyz:
	case lbp::cmd_travel_rel_xyzu:
	case lbp::cmd_laser_power_min:
	case lbp::cmd_laser_power_max:
	case lbp::cmd_laser_freq:
	case lbp::cmd_laser_enable:
	case lbp::cmd_laser_disable:
	case lbp::cmd_laser_on:
	case lbp::cmd_laser_off:
	case lbp::cmd_raster_power:
	case lbp::cmd_air_off:
	case lbp::cmd_air_on:
	case lbp::cmd_dwell:
	case lbp::cmd_job_begin:
	case lbp::cmd_job_end:
	case lbp::cmd_job_body_begin:
	case lbp::cmd_job_body_end:
	case lbp::cmd_job_header_begin:
	case lbp::cmd_job_header_end:
	case lbp::cmd_frame_begin:
	case lbp::cmd_frame_end:
	case lbp::cmd_galvo_laser_on_delay:
	case lbp::cmd_galvo_laser_off_delay:
	case lbp::cmd_galvo_polygon_delay:
	case lbp::cmd_galvo_end_delay:
	case lbp::cmd_galvo_travel_delay:
	case lbp::cmd_laser_pulse_width:
	case lbp::cmd_guide_light_on:
	case lbp::cmd_guide_light_off:
	case lbp::cmd_focus_light_on:
	case lbp::cmd_focus_light_off: {
		uint32_t expected = 0;
		if (!argSizeValid(cmd, request.size(), expected)) {
			if (source == CmdSource::Wire) {
				out_q.nack(request, lbp::err_invalid_length, expected);
			} else {
				gLog().error(QString("Job command %1 has bad argument size").arg(cmd, 4, 16, QChar('0')));
			}
			return true;
		}
		if (!m_cmd_q.push(QueuedCmd{toCmd(request), file_offset})) {
			gLog().error("Move command queue full");
			if (source == CmdSource::Wire) {
				out_q.nack(request, lbp::err_buffer_full);
			}
			return true;
		}
		if (source == CmdSource::Wire) {
			out_q.respond(request);
		}
		return true;
	}
	default:
		break;
	}
	return false;
}

static inline bool isMoveCmd(uint16_t cmd)
{
	return (cmd & 0xF000) == lbp::cls_move;
}

static bool isRelativeMove(uint16_t cmd)
{
	return isMoveCmd(cmd) && (cmd & lbp::flag_abs) == 0;
}

static bool isAbsoluteMove(uint16_t cmd)
{
	return isMoveCmd(cmd) && (cmd & lbp::flag_abs) == lbp::flag_abs;
}

bool MovementSim::decodeMoveDelta(lbp::CmdPayload &p, Vec4 &delta)
{
	const uint16_t cmd = p.cmd();
	const bool home = ((cmd & 0xFF00) == lbp::move_home);
	const bool abs = isAbsoluteMove(cmd);

	if (cmd & ~(lbp::mask_axis | 0xFF00)) {
		return false; // defensive
	}
	if (cmd & lbp::mask_axis & ~kSupportedAxes) {
		return false; // a/b/c/v: can't decode
	}
	for (int i = 0; i < 4; ++i) {
		if (!(cmd & kAxes[i].flag)) {
			continue;
		}

		if (home) {
			delta.*(kAxes[i].m) = homeDelta(i);
			continue;
		}
		int32_t arg = 0;
		if (!p.readArg(arg)) {
			return false;
		}
		delta.*(kAxes[i].m) = abs ? absoluteDelta(i, arg) : relativeDelta(i, arg);
	}
	return true;
}

MovementSim::State MovementSim::takeNextCommand()
{
	refreshRotary();
	QueuedCmd queued;
	while (m_cmd_q.pop(queued)) {
		lbp::CmdPayload &p = queued.payload;
		// Job progress counts what has run: from now until the next command is taken, everything before this
		// command's offset in the job file has finished (spec.md, "Job Progress").
		if (queued.file_offset != kNoFileOffset) {
			m_exec_offset = queued.file_offset;
		}
		const uint16_t cmd = p.cmd();
		gLog().debug(QString("Dequeued a command: %1 of %2").arg(cmd).arg(m_cmd_q.size()));
		if (takeSpeed(p)) {
			continue;
		}
		if (isMoveCmd(cmd)) {
			Vec4 delta;
			if (!decodeMoveDelta(p, delta)) {
				badPayload(p);
				continue; // malformed: discard, try the next
			}
			// Moves fire or darken the lasers (spec.md, "Moves and the Laser"): a cut lights the enabled lasers;
			// any other move (travel, jog, home) darkens them all. Lasers already lit by cmd_laser_on stay lit into a cut.
			const bool cut = (cmd & 0xFF00 & ~lbp::flag_abs) == lbp::move_cut;
			m_laser_1.on = cut && (m_laser_1.on || m_laser_1.enabled);
			m_laser_2.on = cut && (m_laser_2.on || m_laser_2.enabled);
			if (!beginMove(delta, speedsFor(cmd))) {
				continue; // nothing to move: consumed, try the next
			}
			return State::Moving;
		}

		switch (p.cmd()) {
		case lbp::cmd_laser_enable: {
			gLog().push(Log::DEBUG, "Laser enable");
			uint8_t index = 0;
			if (!p.readArg(index)) {
				badPayload(p);
				continue;
			}
			if (index == 0) {
				m_laser_1.enabled = true;
			} else if (index == 1) {
				m_laser_2.enabled = true;
			}
		} break;
		case lbp::cmd_laser_disable: {
			gLog().push(Log::DEBUG, "Laser disable");
			uint8_t index = 0;
			if (!p.readArg(index)) {
				badPayload(p);
				continue;
			}
			if (index == 0) {
				m_laser_1.enabled = false;
				m_laser_1.on = false; // disabling a lit laser darkens it
			} else if (index == 1) {
				m_laser_2.enabled = false;
				m_laser_2.on = false;
			}
		} break;
		case lbp::cmd_laser_on: {
			uint8_t index = 0;
			if (!p.readArg(index)) {
				badPayload(p);
				continue;
			}
			// Only an enabled laser lights, as with cut moves; otherwise this has no effect (spec.md, "Firing in Place").
			LaserSettings &laser = index == 0 ? m_laser_1 : m_laser_2;
			if (laser.enabled) {
				gLog().push(Log::DEBUG, QString("Laser %1 on").arg(index));
				laser.on = true;
			} else {
				gLog().push(Log::DEBUG, QString("Laser %1 on ignored: not enabled").arg(index));
			}
		} break;
		case lbp::cmd_laser_off: {
			gLog().push(Log::DEBUG, "Laser off");
			uint8_t index = 0;
			if (!p.readArg(index)) {
				badPayload(p);
				continue;
			}
			if (index == 0) {
				m_laser_1.on = false;
			}
			else if (index == 1) {
				m_laser_2.on = false;
			}
		} break;
		case lbp::cmd_laser_power_min: {
			uint8_t index = 0;
			uint16_t power = 0;
			if (!p.readArgs(index, power)) {
				badPayload(p);
				continue;
			}
			if (index == 0) {
				m_laser_1.power_min = normalize(power);
			}
			else if (index == 1) {
				m_laser_2.power_min = normalize(power);
			}
		} break;
		case lbp::cmd_laser_power_max: {
			uint8_t index = 0;
			uint16_t power = 0;
			if (!p.readArgs(index, power)) {
				badPayload(p);
				continue;
			}
			if (index == 0) {
				m_laser_1.power_max = normalize(power);
				// gLog().push(Log::INFO, QString("set power max 1: ") + QString::number(m_laser_1.power_max));
				m_laser_1.clearRaster();
			} else if (index == 1) {
				m_laser_2.power_max = normalize(power);
				// gLog().push(Log::INFO, QString("set power max 2: ") + QString::number(m_laser_1.power_max));
				m_laser_2.clearRaster();
			}
		} break;
		case lbp::cmd_laser_freq: {
			uint8_t index = 0;
			int32_t freq = 0;
			if (!p.readArgs(index, freq)) {
				badPayload(p);
				continue;
			}
			if (index == 0) {
				m_laser_1.freq = freq;
			}
			else if (index == 1) {
				m_laser_2.freq = freq;
			}
		} break;
		case lbp::cmd_raster_power: {
			uint8_t index = 0;
			if (!p.readArg(index)) {
				badPayload(p);
				continue;
			}
			if (index == 0) {
				m_laser_1.setRaster(p);
			}
			else if (index == 1) {
				m_laser_2.setRaster(p);
			}
		} break;
		case lbp::cmd_air_off:
			break; // TODO
		case lbp::cmd_air_on:
			break; // TODO
		case lbp::cmd_dwell: {
			uint32_t us = 0;
			if (!p.readArg(us)) {
				badPayload(p);
				continue;
			}
			us = std::min(us, uint32_t(lbp::max_dwell));
			m_dwell_remaining_s = double(us) / 1'000'000.0;
			return State::Dwelling;
		}
		case lbp::cmd_cut_from: {
			uint8_t code = 0;
			if (!p.readArg(code)) {
				badPayload(p);
				continue;
			}
			switch (code) {
			case lbp::cut_from_current_position:
				m_job_origin.x = m_pos.x;
				m_job_origin.y = m_pos.y;
				break;
			case lbp::cut_from_user_origin: {
				int32_t tmp = 0;
				m_config.get(lbp::cfg_user_origin_x, tmp);
				m_job_origin.x = static_cast<int64_t>(tmp);
				m_config.get(lbp::cfg_user_origin_y, tmp);
				m_job_origin.y = static_cast<int64_t>(tmp);
			} break;
			case lbp::cut_from_absolute:
			default:
				m_job_origin.reset();
				break;
			}
		} break;
		case lbp::cmd_cut_type:
			// TODO
			break;
		case lbp::cmd_job_header_begin:
		case lbp::cmd_job_header_end:
		case lbp::cmd_job_body_begin:
		case lbp::cmd_job_body_end:
			break; // TODO
		case lbp::cmd_frame_begin:
			m_in_frame = true;
			break;
		case lbp::cmd_frame_end:
			m_in_frame = false;
			break;
		case lbp::cmd_job_begin:
			m_in_job = true;
			break;
		case lbp::cmd_job_end:
			m_in_job = false;
			m_job_ended = true;
			m_job_origin.reset();
			break;
		case lbp::cmd_galvo_laser_on_delay:
		case lbp::cmd_galvo_laser_off_delay:
		case lbp::cmd_galvo_polygon_delay:
		case lbp::cmd_galvo_end_delay: {
			// Stored and logged; the simulator doesn't apply galvo delays to its timing yet.
			int32_t us = 0;
			if (!p.readArg(us)) {
				badPayload(p);
				continue;
			}
			const char *name = "end";
			int32_t *target = &m_galvo_end_delay_us;
			if (p.cmd() == lbp::cmd_galvo_laser_on_delay) {
				name = "laser on";
				target = &m_galvo_laser_on_delay_us;
			} else if (p.cmd() == lbp::cmd_galvo_laser_off_delay) {
				name = "laser off";
				target = &m_galvo_laser_off_delay_us;
			} else if (p.cmd() == lbp::cmd_galvo_polygon_delay) {
				name = "polygon";
				target = &m_galvo_polygon_delay_us;
			}
			if (*target != us) {
				gLog().push(Log::DEBUG, QString("Galvo %1 delay: %2 us").arg(name).arg(us));
			}
			*target = us;
		} break;
		case lbp::cmd_galvo_travel_delay: {
			// Stored and logged; not yet applied to the simulator's timing.
			int32_t min_us = 0, max_us = 0, dist_um = 0;
			if (!p.readArgs(min_us, max_us, dist_um)) {
				badPayload(p);
				continue;
			}
			if (min_us != m_galvo_travel_delay_min_us || max_us != m_galvo_travel_delay_max_us
				|| dist_um != m_galvo_travel_delay_dist_um) {
				gLog().push(Log::DEBUG, QString("Galvo travel delay: %1-%2 us, full at %3 um").arg(min_us).arg(max_us).arg(dist_um));
			}
			m_galvo_travel_delay_min_us = min_us;
			m_galvo_travel_delay_max_us = max_us;
			m_galvo_travel_delay_dist_um = dist_um;
		} break;
		case lbp::cmd_laser_pulse_width: {
			uint8_t index = 0;
			int32_t ns = 0;
			if (!p.readArgs(index, ns)) {
				badPayload(p);
				continue;
			}
			if (index == 0) {
				m_laser_1.pulse_width_ns = ns;
			}
			else if (index == 1) {
				m_laser_2.pulse_width_ns = ns;
			}
		} break;
		case lbp::cmd_guide_light_on:
			m_guide_light = true;
			break;
		case lbp::cmd_guide_light_off:
			m_guide_light = false;
			break;
		case lbp::cmd_focus_light_on:
			m_focus_light = true;
			gLog().push(Log::INFO, "Focus light on");
			break;
		case lbp::cmd_focus_light_off:
			m_focus_light = false;
			gLog().push(Log::INFO, "Focus light off");
			break;
		default:
			gLog().push(Log::ERROR, QString("unexpected queued move command %1").arg(QString::number(p.cmd(), 16)));
			break;
		}
	}
	return MovementSim::State::Idle;
}

void MovementSim::stop()
{
	m_state = MovementSim::State::Idle;
	m_cmd_q.clear();
	m_in_frame = false;
	m_in_job = false;
	m_job_ended = false;
	m_laser_1.on = false;
	m_laser_2.on = false;
	m_dwell_remaining_s = 0.0;
	m_exec_offset = 0;
}

void MovementSim::pause()
{
	if (m_state != MovementSim::State::Paused) {
		m_paused.state = m_state;
		m_paused.laser_1 = m_laser_1.on;
		m_paused.laser_2 = m_laser_2.on;

		m_laser_1.on = false;
		m_laser_2.on = false;
		m_state = MovementSim::State::Paused;
	}
}

void MovementSim::resume()
{
	if (m_state == MovementSim::State::Paused) {
		m_laser_1.on = m_paused.laser_1;
		m_laser_2.on = m_paused.laser_2;
		m_state = m_paused.state;
	}
}

void MovementSim::beginJog(const Vec4 &d, int32_t target_vel)
{
	refreshRotary();
	m_laser_1.on = false; // operator moves never fire
	m_laser_2.on = false;
	double norm = 0.0;
	double speed = double(target_vel);
	for (const AxisRef &a : kAxes) {
		const double v = double(d.*(a.m));
		norm += v * v;
	}
	norm = std::sqrt(norm);
	if (norm <= 0.0 || speed <= 0.0) {
		return;
	}

	for (int i = 0; i < 4; ++i) {
		m_jog.dir[i] = double(d.*(kAxes[i].m)) / norm;
	}
	m_jog.start = m_pos;
	m_jog.traveled = 0.0;
	m_jog.speed = speed;
	m_state = State::Jogging;
}

void MovementSim::endJog()
{
	if (m_state == MovementSim::State::Jogging) {
		m_state = MovementSim::State::Idle;
	}
}

void MovementSim::step(double dt_s)
{
	double budget_s = dt_s;

	while (budget_s > 0.0) {
		switch (m_state) {
		case State::Idle:
			m_state = takeNextCommand();          // renamed updateTarget
			if (m_state == State::Idle) return;   // nothing to do; don't spin
			continue;                             // re-enter with the new state, same budget

		case State::Moving: {
			const double remaining = m_move.length - m_move.traveled;
			const double step_dist = m_move.speed * budget_s;

			if (step_dist < remaining) {
				m_move.traveled += step_dist;
				budget_s = 0.0;
			} else {
				m_move.traveled = m_move.length;
				budget_s -= remaining / m_move.speed;   // carry the remainder forward
			}

			m_pos = applyLimits(advance(m_move.start, m_move.delta, m_move.progress()));
			if (m_correction != nullptr) {
				// TODO: apply lens correction. A firmware commands the mirrors to m_pos minus the table's distortion,
				// interpolated bilinearly at m_pos (spec.md, "Lens Correction Files"). It does this for every point
				// it outputs along the move, not just the endpoints: the distortion isn't linear, so a corrected
				// straight line is a curve in mirror space. The simulator draws the uncorrected (intended) path.
			}

			const float progress = float(m_move.progress());
			m_laser_1.progressRaster(progress);
			m_laser_2.progressRaster(progress);

			if (m_move.done()) {
				m_laser_1.clearRaster();
				m_laser_2.clearRaster();
				m_state = State::Idle;
			}
		} break;

		case State::Jogging: {
			m_jog.traveled += m_jog.speed * budget_s;
			Vec4 p;
			for (int i = 0; i < 4; ++i) {
				p.*(kAxes[i].m) = m_jog.start.*(kAxes[i].m)
								  + llround(m_jog.dir[i] * m_jog.traveled);
			}
			m_pos = applyLimits(p);
			budget_s = 0.0;
		} break;

		case State::Dwelling:
			if (m_dwell_remaining_s > budget_s) {
				m_dwell_remaining_s -= budget_s;   // dwell outlasts this tick
				budget_s = 0.0;
			} else {
				budget_s -= m_dwell_remaining_s;   // dwell ends mid-tick; carry the rest
				m_dwell_remaining_s = 0.0;
				m_state = State::Idle;             // loop re-enters, takes the next command
			}
			break;
		case State::Paused:
			return;
		}
	}
}

SimState MovementSim::getSimState() const
{
	SimState state;
	state.pos = m_pos;
	state.laser_1_power = m_laser_1.on ? m_laser_1.power_max : 0.f;
	state.laser_2_power = m_laser_2.on ? m_laser_2.power_max : 0.f;
	state.mode = m_mode;
	state.min_pos = m_min_pos;
	state.max_pos = m_max_pos;
	state.guide_light = m_guide_light;
	state.correction_on = m_correction != nullptr;
	return state;
}

bool MovementSim::canEnqueue() const
{
	return m_cmd_q.freeSpace() > 0;
}

uint32_t MovementSim::getFwState(uint32_t state)
{
	if (m_state == MovementSim::State::Jogging || m_state == MovementSim::State::Moving) {
		state |= lbp::state_moving;
	} else {
		state &= ~lbp::state_moving;
	}

	if (m_in_frame) {
		state |= lbp::state_executing_frame;
	} else {
		state &= ~lbp::state_executing_frame;
	}

	if (m_state == MovementSim::State::Paused) {
		state |= lbp::state_paused;
	} else {
		state &= ~lbp::state_paused;
	}

	return state;
}

void LaserSettings::setRaster(lbp::CmdPayload &payload)
{
	if (payload.cmd() != lbp::cmd_raster_power) return;
	if (payload.size() < 1) {
		num_rasters = 0;
		return;
	}

	num_rasters = (payload.size() - 1) >> 1;
	//gLog().info(QString("set raster ") + QString::number(num_rasters));

	if (!num_rasters) {
		return;
	}

	// Assume index byte has already been read
	uint16_t power;
	for (size_t i = 0; i < num_rasters; i++) {
		if (payload.readArg(power)) {
			raster_powers[i] = normalize(power);
		}
	}
}

void LaserSettings::progressRaster(float progress)
{
	if (num_rasters > 0) {
		// Each of the N rasters covers an equal 1/N share of the move.
		const size_t i = std::min(num_rasters - 1, (size_t) (progress * num_rasters));
		power_max = raster_powers[i];
		//gLog().info("..." + QString::number(progress) + "..." + QString::number(power_max));
	}
}

void LaserSettings::clearRaster()
{
	num_rasters = 0;
}

void MovementSim::refreshRotary()
{
	m_rotary_axis = -1;
	int32_t enabled = 0;
	if (!m_config.get(lbp::cfg_rotary_enable, enabled) || !enabled) return;

	int32_t axis = -1;
	if (!m_config.get(lbp::cfg_rotary_axis, axis)) return;

	// A galvo's X and Y are its mirrors: they can't be rotary. (A rotary on Z or U still works.)
	if (m_mode == MachineMode::Galvo && (axis == lbp::axis_index_x || axis == lbp::axis_index_y)) {
		if (!m_warned_galvo_rotary) {
			gLog().push(Log::WARNING, "Galvo mode: ignoring the rotary setting on X/Y (cfg_rotary_axis)");
			m_warned_galvo_rotary = true;
		}
		return;
	}

	for (int i = 0; i < 4; ++i) {
		if (axis == kAxes[i].index) {
			m_rotary_axis = i;
			return;
		}
	}
}

int64_t MovementSim::relativeDelta(int idx, int64_t arg) const
{
	if (idx == m_rotary_axis) return arg; // no clamping for rotary deltas
	const int64_t curr = m_pos.*(kAxes[idx].m);
	return clampTo(curr + arg, m_min_pos.*(kAxes[idx].m), m_max_pos.*(kAxes[idx].m)) - curr;
}

int64_t MovementSim::absoluteDelta(int idx, int64_t arg) const
{
	const int64_t curr   = m_pos.*(kAxes[idx].m);
	const int64_t target = arg + m_job_origin.*(kAxes[idx].m);
	const int64_t capped = clampTo(target, m_min_pos.*(kAxes[idx].m), m_max_pos.*(kAxes[idx].m));
	if (capped != target) {
		gLog().push(Log::WARNING, QString("absolute target %1 on axis %2 clamped to %3")
			.arg(target).arg(idx).arg(capped));
	}
	return capped - curr;
}

int64_t MovementSim::homeDelta(int idx) const
{
	if (idx == m_rotary_axis) {
		return 0; // rotary has no machine zero to seek
	}
	return -(m_pos.*(kAxes[idx].m));
}

Vec4 MovementSim::applyLimits(const Vec4 &vec) const
{
    Vec4 result;
    for (int i = 0; i < 4; ++i) {
        const int64_t v = vec.*(kAxes[i].m);
        const int64_t lo = m_min_pos.*(kAxes[i].m);
        const int64_t hi = m_max_pos.*(kAxes[i].m);
        result.*(kAxes[i].m) = (i == m_rotary_axis) ? wrapTo(v, lo, hi) : clampTo(v, lo, hi);
    }
	return result;
}

bool MovementSim::takeSpeed(lbp::CmdPayload &p)
{
	const uint16_t cmd = p.cmd();
	SpeedSet *set = nullptr;
	switch (cmd & 0xFF00) {
	case lbp::setting_speed_cut:
		set = &m_cut_speed;
		break;
	case lbp::setting_speed_travel:
		set = &m_travel_speed;
		break;
	case lbp::setting_speed_jog:
		set = &m_jog_speed;
		break;
	default:
		return false;
	}
	uint32_t value = 0;
	if (!p.readArg(value)) {
		badPayload(p);
		return true;
	}
	const uint16_t axes = cmd & lbp::mask_axis;
	if (axes & (lbp::axis_x | lbp::axis_y)) set->xy = value;
	if (axes & lbp::axis_z) set->z = value;
	if (axes & lbp::axis_u) set->u = value;
	return true;
}

/** Read a positive speed from the configuration, or use the fallback. */
static uint32_t configuredSpeed(const Configuration &config, uint16_t key, uint32_t fallback)
{
	int32_t value = 0;
	return (config.get(key, value) && value > 0) ? uint32_t(value) : fallback;
}

MovementSim::SpeedSet MovementSim::speedsFor(uint16_t cmd) const
{
	// Simulator defaults (um/s) for speeds the configuration leaves at 0.
	static constexpr uint32_t kHomeSpeedXY = 50000, kHomeSpeedZU = 5000, kTravelSpeedXY = 100000;

	if ((cmd & 0xFF00) == lbp::move_home) {
		SpeedSet home;
		home.xy = configuredSpeed(m_config, lbp::cfg_xy_home_speed, kHomeSpeedXY);
		home.z = configuredSpeed(m_config, lbp::cfg_z_home_speed, kHomeSpeedZU);
		home.u = configuredSpeed(m_config, lbp::cfg_u_home_speed, kHomeSpeedZU);
		return home;
	}
	switch (cmd & 0xFF00 & ~lbp::flag_abs) {
	case lbp::move_cut:
		return m_cut_speed;
	case lbp::move_jog:
		return m_jog_speed;
	default: {
		SpeedSet travel = m_travel_speed;
		if (travel.xy == 0) {
			travel.xy = configuredSpeed(m_config, lbp::cfg_idle_speed, kTravelSpeedXY);
		}
		return travel;
	}
	}
}

bool MovementSim::beginMove(const Vec4 &delta, const SpeedSet &speeds)
{
	double length = 0.0, speed = 0.0;
	if (!pacingFor(delta, speeds, length, speed)) return false;

	m_move.start    = m_pos;
	m_move.delta    = delta;
	m_move.length   = length;
	m_move.speed    = speed;
	m_move.traveled = 0.0;
	return true;
}

/**
 * @brief Choose the path metric and rate for a move.
 *
 * Feed rate applies to the XY path when there is any XY motion; Z and U are
 * carried along and arrive with it. A move with no XY component is paced by
 * whichever remaining group is moving, at that group's commanded rate.
 *
 * @param length Out: path distance the rate applies to, in um.
 * @param speed  Out: path rate, in um/s.
 * @return False if the move has no motion or no rate is set, in which case
 *         length and speed are unchanged.
 */
bool MovementSim::pacingFor(const Vec4 &delta, const SpeedSet &speeds, double &length, double &speed) const
{
	const double xy = std::hypot(double(delta.x), double(delta.y));
	if (xy > 0.0) {
		length = xy;
		speed  = double(speeds.xy);
	} else if (delta.z != 0) {
		length = std::abs(double(delta.z));
		speed  = double(speeds.z);
	} else if (delta.u != 0) {
		length = std::abs(double(delta.u));
		speed  = double(speeds.u);
	} else {
		return false;   // no motion
	}

	if (speed <= 0.0) {
		gLog().push(Log::WARNING, "move requested with no feed rate set");
		return false;
	}
	return true;
}

bool MovementSim::takeJobEnded()
{
	const bool f = m_job_ended;
	m_job_ended = false;
	return f;
}

bool MovementSim::isIdle() const
{
	return m_state == State::Idle && m_cmd_q.empty();
}
