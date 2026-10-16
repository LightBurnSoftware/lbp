// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include "movement.h"
#include "log.h"

#include <lbp/beio.h>
#include <lbp/spec.h>

#include <algorithm>
#include <cmath>

static inline float normalize(uint16_t power) { return static_cast<float>(power) / 65535.f; }

static inline int64_t clampTo(int64_t v, int64_t limit)
{
	if (limit > 0) {
		return std::clamp<int64_t>(v, 0, limit);
	}
	return v;
}

static inline int64_t wrapTo(int64_t v, int64_t limit)
{
	if (limit > 0) {
		v %= limit;
		if (v < 0) v += limit;
	}
	return v;
}

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

MovementSim::MovementSim(Configuration &config)
	: m_config(config)
	, m_max_pos(300000, 180000, 120000, 60000) // hardcoded for now.
{
	// Empty
}

bool MovementSim::process(lbp::MaxPayload &request, OutputQueue &out_q, CmdSource source)
{
	const uint16_t cmd = request.cmd();

	switch (cmd) {
	case lbp::cmd_pos_x:
		gLog().push(Log::DEBUG, QString("X Axis Query: %1").arg(m_pos.x));
		out_q.push(lbp::CmdMsg(cmd, 4, toWire(m_pos.x)));
		return true;
	case lbp::cmd_pos_y:
		gLog().push(Log::DEBUG, QString("Y Axis Query: %1").arg(m_pos.y));
		out_q.push(lbp::CmdMsg(cmd, 4, toWire(m_pos.y)));
		return true;
	case lbp::cmd_pos_z:
		gLog().push(Log::DEBUG, QString("Z Axis Query: %1").arg(m_pos.z));
		out_q.push(lbp::CmdMsg(cmd, 4, toWire(m_pos.z)));
		return true;
	case lbp::cmd_pos_u:
		gLog().push(Log::DEBUG, QString("U Axis Query: %1").arg(m_pos.u));
		out_q.push(lbp::CmdMsg(cmd, 4, toWire(m_pos.u)));
		return true;
	case lbp::cmd_pos_xy: {
		lbp::CmdMsg m(cmd, 8);
		m.writeInt(toWire(m_pos.x));
		m.writeInt(toWire(m_pos.y));
		out_q.push(m);
		return true;
	}
	case lbp::cmd_pos_xyz: {
		lbp::CmdMsg m(cmd, 12);
		m.writeInt(toWire(m_pos.x));
		m.writeInt(toWire(m_pos.y));
		m.writeInt(toWire(m_pos.z));
		out_q.push(m);
		return true;
	}
	case lbp::cmd_pos_xyzu: {
		gLog().push(Log::DEBUG, "XYZU Axis Query");

		lbp::CmdMsg m(cmd, 16);
		m.writeInt(toWire(m_pos.x));
		m.writeInt(toWire(m_pos.y));
		m.writeInt(toWire(m_pos.z));
		m.writeInt(toWire(m_pos.u));
		out_q.push(m);
		return true;
	}
	case lbp::cmd_jog_start_pos_x:
		beginJog(Vec4(1, 0, 0, 0), m_target_vel_xy);
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_stop_pos_x:
		endJog();
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_start_neg_x:
		beginJog(Vec4(-1, 0, 0, 0), m_target_vel_xy);
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_stop_neg_x:
		endJog();
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_start_pos_y:
		beginJog(Vec4(0, 1, 0, 0), m_target_vel_xy);
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_stop_pos_y:
		endJog();
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_start_neg_y:
		beginJog(Vec4(0, -1, 0, 0), m_target_vel_xy);
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_stop_neg_y:
		endJog();
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_start_pos_z:
		beginJog(Vec4(0, 0, 1, 0), m_target_vel_z);
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_stop_pos_z:
		endJog();
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_start_neg_z:
		beginJog(Vec4(0, 0, -1, 0), m_target_vel_z);
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_stop_neg_z:
		endJog();
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_start_pos_u:
		beginJog(Vec4(0, 0, 0, 1), m_target_vel_u);
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_stop_pos_u:
		endJog();
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_start_neg_u:
		beginJog(Vec4(0, 0, 0, -1), m_target_vel_u);
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_stop_neg_u:
		endJog();
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_cut_from:
	case lbp::cmd_cut_type:
	case lbp::cmd_speed_x:
	case lbp::cmd_speed_y:
	case lbp::cmd_home_xy:
	case lbp::cmd_home_z:
	case lbp::cmd_speed_xy:
	case lbp::cmd_speed_z:
	case lbp::cmd_speed_u:
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
		if (!m_cmd_q.push(toCmd(request))) {
			gLog().error("Move command queue full");
		}
		if (source == CmdSource::Wire) {
			out_q.push(lbp::CmdMsg(cmd));
		}
		return true;
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
	lbp::CmdPayload p;
	while (m_cmd_q.pop(p)) {
		const uint16_t cmd = p.cmd();
		gLog().debug(QString("Dequeued a command: %1 of %2").arg(cmd).arg(m_cmd_q.size()));
		if (isMoveCmd(cmd)) {
			Vec4 delta;
			if (!decodeMoveDelta(p, delta)) {
				badPayload(p);
				continue; // malformed: discard, try the next
			}
			if (!beginMove(delta)) {
				continue; // nothing to move: consumed, try the next
			}
			return State::Moving;
		}

		switch (p.cmd()) {
		case lbp::cmd_speed_x:
		case lbp::cmd_speed_y:
		case lbp::cmd_speed_xy:
			if (!p.readArg(m_target_vel_xy)) {
				badPayload(p);
			}
			break;
		case lbp::cmd_speed_z:
			if (!p.readArg(m_target_vel_z)) {
				badPayload(p);
			}
			break;
		case lbp::cmd_speed_u:
			if (!p.readArg(m_target_vel_u)) {
				badPayload(p);
			}
			break;
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
			} else if (index == 1) {
				m_laser_2.enabled = false;
			}
		} break;
		case lbp::cmd_laser_on: {
			gLog().push(Log::DEBUG, "Laser on");
			uint8_t index = 0;
			if (!p.readArg(index)) {
				badPayload(p);
				continue;
			}
			if (index == 0) {
				m_laser_1.on = true;
			}
			else if (index == 1) {
				m_laser_2.on = true;
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
			int16_t power = 0;
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
			int16_t power = 0;
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
	return SimState {
		m_pos,
		m_laser_1.on ? m_laser_1.power_max : 0.f,
		m_laser_2.on ? m_laser_2.power_max : 0.f
	};
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
	if (payload.size() < 3) {
		num_rasters = 0;
		return;
	}

	num_rasters = (payload.size() - 3) >> 1;
	//gLog().info(QString("set raster ") + QString::number(num_rasters));

	if (!num_rasters) {
		return;
	}

	// Assume index byte has already been read
	int16_t power;
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
	const int64_t limit = m_max_pos.*(kAxes[idx].m);
	return clampTo(curr + arg, limit) - curr;
}

int64_t MovementSim::absoluteDelta(int idx, int64_t arg) const
{
	const int64_t curr   = m_pos.*(kAxes[idx].m);
	const int64_t limit  = m_max_pos.*(kAxes[idx].m);
	const int64_t target = arg + m_job_origin.*(kAxes[idx].m);
	const int64_t capped = clampTo(target, limit);
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
        const int64_t limit = m_max_pos.*(kAxes[i].m);
        result.*(kAxes[i].m) = (i == m_rotary_axis) ? wrapTo(v, limit) : clampTo(v, limit);
    }
	return result;
}

bool MovementSim::beginMove(const Vec4 &delta)
{
	double length = 0.0, speed = 0.0;
	if (!pacingFor(delta, length, speed)) return false;

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
bool MovementSim::pacingFor(const Vec4 &delta, double &length, double &speed) const
{
	const double xy = std::hypot(double(delta.x), double(delta.y));
	if (xy > 0.0) {
		length = xy;
		speed  = double(m_target_vel_xy);
	} else if (delta.z != 0) {
		length = std::abs(double(delta.z));
		speed  = double(m_target_vel_z);
	} else if (delta.u != 0) {
		length = std::abs(double(delta.u));
		speed  = double(m_target_vel_u);
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
