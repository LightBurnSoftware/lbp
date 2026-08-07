// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include "movement.h"
#include "log.h"

#include <lbp/beio.h>
#include <lbp/spec.h>

#include <cmath>

/**
 * @brief Calculate a velocity vector.
 * @param delta The raw delta vector.
 * @param magnitude The target velocity magnitude.
 * @return Velocity vector with proper magnitude.
 */
static Vec4 calcVelocity(const Vec4 &delta, int32_t magnitude )
{
	Vec4 result;
	double hyp = sqrt( pow(delta.x, 2) + pow(delta.y, 2) + pow(delta.z, 2) + pow(delta.u, 2) );
	if (hyp > 0) {
		double coeff = static_cast<double>(magnitude) / hyp;
		result.x = static_cast<int32_t>(lround(delta.x * coeff));
		result.y = static_cast<int32_t>(lround(delta.y * coeff));
		result.z = static_cast<int32_t>(lround(delta.z * coeff));
		result.u = static_cast<int32_t>(lround(delta.u * coeff));
	}
	return result;
}

static inline double xydistance(const Vec4 &a, const Vec4 &b)
{
	return sqrt( pow(static_cast<double>(b.x - a.x), 2) + pow(static_cast<double>(b.y - a.y), 2) );
}

static inline float normalize(uint16_t power) { return static_cast<float>(power) / 65535; }

MovementSim::MovementSim(Configuration &config)
	: m_config(config)
	, m_max_pos(300000, 180000, 120000, 60000) // hardcoded for now.
{
	// Empty
}

bool MovementSim::process(lbp::MaxPayload &request, OutputQueue &out_q)
{
	const uint16_t cmd = request.cmd();

	switch (cmd) {
	case lbp::cmd_pos_x:
		gLog().push(Log::DEBUG, QString("X Axis Query: %1").arg(m_pos.x));
		out_q.push(lbp::CmdMsg(cmd, 4, m_pos.x));
		return true;
	case lbp::cmd_pos_y:
		gLog().push(Log::DEBUG, QString("Y Axis Query: %1").arg(m_pos.y));
		out_q.push(lbp::CmdMsg(cmd, 4, m_pos.y));
		return true;
	case lbp::cmd_pos_z:
		gLog().push(Log::DEBUG, QString("Z Axis Query: %1").arg(m_pos.z));
		out_q.push(lbp::CmdMsg(cmd, 4, m_pos.z));
		return true;
	case lbp::cmd_pos_u:
		gLog().push(Log::DEBUG, QString("U Axis Query: %1").arg(m_pos.u));
		out_q.push(lbp::CmdMsg(cmd, 4, m_pos.u));
		return true;
	case lbp::cmd_pos_xy: {
		lbp::CmdMsg m(cmd, 8);
		m.writeInt(m_pos.x);
		m.writeInt(m_pos.y);
		out_q.push(m);
		return true;
	}
	case lbp::cmd_pos_xyz: {
		lbp::CmdMsg m(cmd, 12);
		m.writeInt(m_pos.x);
		m.writeInt(m_pos.y);
		m.writeInt(m_pos.z);
		out_q.push(m);
		return true;
	}
	case lbp::cmd_pos_xyzu: {
		gLog().push(Log::DEBUG, "XYZU Axis Query");

		lbp::CmdMsg m(cmd, 16);
		m.writeInt(m_pos.x);
		m.writeInt(m_pos.y);
		m.writeInt(m_pos.z);
		m.writeInt(m_pos.u);
		out_q.push(m);
		return true;
	}
	case lbp::cmd_jog_start_pos_x:
		startJog(Vec4(1, 0, 0, 0));
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_stop_pos_x:
		stopJog();
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_start_neg_x:
		startJog(Vec4(-1, 0, 0, 0));
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_stop_neg_x:
		stopJog();
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_start_pos_y:
		startJog(Vec4(0, 1, 0, 0));
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_stop_pos_y:
		stopJog();
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_start_neg_y:
		startJog(Vec4(0, -1, 0, 0));
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_stop_neg_y:
		stopJog();
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_start_pos_z:
		startJog(Vec4(0, 0, 1, 0));
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_stop_pos_z:
		stopJog();
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_start_neg_z:
		startJog(Vec4(0, 0, -1, 0));
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_stop_neg_z:
		stopJog();
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_start_pos_u:
		startJog(Vec4(0, 0, 0, 1));
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_stop_pos_u:
		stopJog();
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_start_neg_u:
		startJog(Vec4(0, 0, 0, -1));
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_jog_stop_neg_u:
		stopJog();
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_cut_from:
	case lbp::cmd_cut_type:
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
	case lbp::cmd_goto_x:
	case lbp::cmd_goto_y:
	case lbp::cmd_goto_z:
	case lbp::cmd_goto_u:
	case lbp::cmd_goto_xy:
	case lbp::cmd_goto_xyz:
	case lbp::cmd_goto_xyzu:
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
	case lbp::cmd_rapid_abs_x:
	case lbp::cmd_rapid_abs_y:
	case lbp::cmd_rapid_abs_z:
	case lbp::cmd_rapid_abs_u:
	case lbp::cmd_rapid_abs_xy:
	case lbp::cmd_rapid_abs_xyz:
	case lbp::cmd_rapid_abs_xyzu:
	case lbp::cmd_rapid_rel_x:
	case lbp::cmd_rapid_rel_y:
	case lbp::cmd_rapid_rel_z:
	case lbp::cmd_rapid_rel_u:
	case lbp::cmd_rapid_rel_xy:
	case lbp::cmd_rapid_rel_xyz:
	case lbp::cmd_rapid_rel_xyzu:
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
		m_cmd_q.push(toCmd(request));
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	default:
		break;
	}
	return false;
}

MovementSim::State MovementSim::updateTarget()
{
	while (!m_cmd_q.empty()) {
		Vec4 target = m_pos;
		lbp::Payload p = m_cmd_q.pop();

		switch(p.cmd()) {
		case lbp::cmd_home_xy:
			target.x = 0;
			target.y = 0;
			break;
		case lbp::cmd_home_z:
			target.z = 0;
			break;
		case lbp::cmd_goto_x:
		case lbp::cmd_cut_abs_x:
		case lbp::cmd_rapid_abs_x:
			target.x = p.readIntArg() + m_job_origin.x;
			break;
		case lbp::cmd_jog_step_x:
		case lbp::cmd_cut_rel_x:
		case lbp::cmd_rapid_rel_x:
			target.x += p.readIntArg();
			break;
		case lbp::cmd_goto_y:
		case lbp::cmd_cut_abs_y:
		case lbp::cmd_rapid_abs_y:
			target.y = p.readIntArg() + m_job_origin.y;
			break;
		case lbp::cmd_jog_step_y:
		case lbp::cmd_cut_rel_y:
		case lbp::cmd_rapid_rel_y:
			target.y += p.readIntArg();
			break;
		case lbp::cmd_goto_z:
		case lbp::cmd_cut_abs_z:
		case lbp::cmd_rapid_abs_z:
			target.z = p.readIntArg();
			break;
		case lbp::cmd_jog_step_z:
		case lbp::cmd_cut_rel_z:
		case lbp::cmd_rapid_rel_z:
			target.z += p.readIntArg();
			break;
		case lbp::cmd_goto_u:
		case lbp::cmd_cut_abs_u:
		case lbp::cmd_rapid_abs_u:
			target.u = p.readIntArg();
			break;
		case lbp::cmd_jog_step_u:
		case lbp::cmd_cut_rel_u:
		case lbp::cmd_rapid_rel_u:
			target.u += p.readIntArg();
			break;
		case lbp::cmd_goto_xy:
		case lbp::cmd_cut_abs_xy:
		case lbp::cmd_rapid_abs_xy:
			target.x = p.readIntArg() + m_job_origin.x;
			target.y = p.readIntArg() + m_job_origin.y;
			break;
		case lbp::cmd_jog_step_xy:
		case lbp::cmd_cut_rel_xy:
		case lbp::cmd_rapid_rel_xy:
			target.x += p.readIntArg();
			target.y += p.readIntArg();
			break;
		case lbp::cmd_goto_xyz:
		case lbp::cmd_cut_abs_xyz:
		case lbp::cmd_rapid_abs_xyz:
			target.x = p.readIntArg() + m_job_origin.x;
			target.y = p.readIntArg() + m_job_origin.y;
			target.z = p.readIntArg();
			break;
		case lbp::cmd_jog_step_xyz:
		case lbp::cmd_cut_rel_xyz:
		case lbp::cmd_rapid_rel_xyz:
			target.x += p.readIntArg();
			target.y += p.readIntArg();
			target.z += p.readIntArg();
			break;
		case lbp::cmd_goto_xyzu:
		case lbp::cmd_cut_abs_xyzu:
		case lbp::cmd_rapid_abs_xyzu:
			target.x = p.readIntArg() + m_job_origin.x;
			target.y = p.readIntArg() + m_job_origin.y;
			target.z = p.readIntArg();
			target.u = p.readIntArg();
			break;
		case lbp::cmd_jog_step_xyzu:
		case lbp::cmd_cut_rel_xyzu:
		case lbp::cmd_rapid_rel_xyzu:
			target.x += p.readIntArg();
			target.y += p.readIntArg();
			target.z += p.readIntArg();
			target.u += p.readIntArg();
			break;
		case lbp::cmd_speed_x:
		case lbp::cmd_speed_y:
		case lbp::cmd_speed_xy:
			m_target_vel_xy = p.readIntArg();
			break;
		case lbp::cmd_speed_z:
			m_target_vel_z = p.readIntArg();
			break;
		case lbp::cmd_speed_u:
			m_target_vel_u = p.readIntArg();
			break;
		case lbp::cmd_laser_enable: {
			gLog().push(Log::DEBUG, "Laser enable");
			uint8_t index = p.readByteArg();
			if (index == 0) {
				m_laser_1.enabled = true;
			}
			else if (index == 1) {
				m_laser_2.enabled = true;
			}
		} break;
		case lbp::cmd_laser_disable: {
			gLog().push(Log::DEBUG, "Laser disable");
			uint8_t index = p.readByteArg();
			if (index == 0) {
				m_laser_1.enabled = false;
			}
			else if (index == 1) {
				m_laser_2.enabled = false;
			}
		} break;
		case lbp::cmd_laser_on: {
			gLog().push(Log::DEBUG, "Laser on");
			uint8_t index = p.readByteArg();
			if (index == 0) {
				m_laser_1.on = true;
			}
			else if (index == 1) {
				m_laser_2.on = true;
			}
		} break;
		case lbp::cmd_laser_off: {
			gLog().push(Log::DEBUG, "Laser off");
			uint8_t index = p.readByteArg();
			if (index == 0) {
				m_laser_1.on = false;
			}
			else if (index == 1) {
				m_laser_2.on = false;
			}
		} break;
		case lbp::cmd_laser_power_min: {
			uint8_t index = p.readByteArg();
			if (index == 0) {
				m_laser_1.power_min = normalize(p.readShortArg());
			}
			else if (index == 1) {
				m_laser_2.power_min = normalize(p.readShortArg());
			}
		} break;
		case lbp::cmd_laser_power_max: {
			uint8_t index = p.readByteArg();
			if (index == 0) {
				m_laser_1.power_max = normalize(p.readShortArg());
				gLog().push(Log::INFO, QString("set power max 1: ") + QString::number(m_laser_1.power_max));
				m_laser_1.clearRaster();
			}
			else if (index == 1) {
				m_laser_2.power_max = normalize(p.readShortArg());
				gLog().push(Log::INFO, QString("set power max 2: ") + QString::number(m_laser_1.power_max));
				m_laser_2.clearRaster();
			}
		} break;
		case lbp::cmd_laser_freq: {
			uint8_t index = p.readByteArg();
			if (index == 0) {
				m_laser_1.freq = p.readIntArg();
			}
			else if (index == 1) {
				m_laser_2.freq = p.readIntArg();
			}
		} break;
		case lbp::cmd_raster_power: {
			uint8_t index = p.readByteArg();
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
		case lbp::cmd_dwell:
			m_dwell_ms = p.readIntArg();
			m_dwell_acc_ms = 0;
			return MovementSim::State::Dwelling;
		case lbp::cmd_cut_from:
			switch (p.readByteArg()) {
			case lbp::cut_from_current_position:
				m_job_origin.x = m_pos.x;
				m_job_origin.y = m_pos.y;
				break;
			case lbp::cut_from_user_origin:
				m_config.get(lbp::cfg_user_origin_x, m_job_origin.x);
				m_config.get(lbp::cfg_user_origin_y, m_job_origin.y);
				break;
			case lbp::cut_from_absolute:
			default:
				m_job_origin.reset();
				break;
			}
			break;
		case lbp::cmd_job_header_begin:
		case lbp::cmd_job_header_end:
		case lbp::cmd_job_body_begin:
		case lbp::cmd_job_body_end:
			break; // TODO
		case lbp::cmd_frame_begin:
		case lbp::cmd_frame_end:
		case lbp::cmd_job_begin:
			m_job_cmd = p.cmd();
			break;
		case lbp::cmd_job_end:
			m_job_cmd = p.cmd();
			m_job_origin.reset();
			break;
		default:
			gLog().push(Log::ERROR, QString("unexpected queued move command %1").arg(p.cmd()));
			break;
		}
		target = clamp(target, m_max_pos);
		if (target != m_pos) {
			m_vel = calcVelocity(target - m_pos, m_target_vel_xy);
			m_target_pos = target;
			m_start_pos = m_pos;
			gLog().push(Log::DEBUG, QString("New Target:   %1").arg(toQString(m_target_pos)));
			gLog().push(Log::DEBUG, QString("New Velocity: %1").arg(toQString(m_vel)));
			return MovementSim::State::Moving;
		}
	}
	return MovementSim::State::Idle;
}

void MovementSim::stop()
{
	m_vel.reset();
	m_target_pos = m_pos;
	m_state = MovementSim::State::Idle;
	while (!m_cmd_q.empty()) {
		m_cmd_q.pop();
	}
	m_job_cmd = 0;
	m_laser_1.on = false;
	m_laser_2.on = false;
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

void MovementSim::startJog(Vec4 dir)
{
	if (m_state != MovementSim::State::Idle) {
		return;
	}
	m_vel = dir * m_target_vel_xy;
	m_state = MovementSim::State::Jogging;
}

void MovementSim::stopJog()
{
	if (m_state == MovementSim::State::Jogging) {
		stop();
	}
}

/** @return True if b is within a and c */
static bool within(int32_t a, int32_t b, int32_t c)
{
	if (a < c) {
		return a <= b && b <= c;
	}
	return c <= b && b <= a;
}

void MovementSim::update(int ms)
{
	int64_t elapsed = ms;
	Vec4 prev_pos = m_pos;
	Vec4 next_pos = clamp(m_pos + ((m_vel * elapsed) / 1000), m_max_pos);

	MovementSim::State next_state = m_state;

	switch (m_state) {
	case MovementSim::State::Idle:
		next_state = updateTarget();
		break;
	case MovementSim::State::Jogging:
		m_pos = next_pos;
		break;
	case MovementSim::State::Moving: {
		if (within(prev_pos.x, m_target_pos.x, next_pos.x)) {
			next_pos.x = m_target_pos.x;
		}
		if (within(prev_pos.y, m_target_pos.y, next_pos.y)) {
			next_pos.y = m_target_pos.y;
		}
		if (within(prev_pos.z, m_target_pos.z, next_pos.z)) {
			next_pos.z = m_target_pos.z;
		}
		if (within(prev_pos.u, m_target_pos.u, next_pos.u)) {
			next_pos.u = m_target_pos.u;
		}
		m_pos = next_pos;
		float progress = getMoveProgress();
		m_laser_1.progressRaster(progress);
		m_laser_2.progressRaster(progress);
		if (m_pos == m_target_pos) {
			m_laser_1.clearRaster();
			m_laser_2.clearRaster();
			next_state = updateTarget();
		} else {
			m_vel = calcVelocity(m_target_pos - m_pos, m_target_vel_xy);
		}
	} break;
	case MovementSim::State::Dwelling:
		m_dwell_acc_ms += ms;
		if (m_dwell_acc_ms > m_dwell_ms) {
			next_state = updateTarget();
		}
		break;
	case MovementSim::State::Paused:
		break;
	}
	m_state = next_state;
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

	if (m_job_cmd == lbp::cmd_job_begin) {
		state |= lbp::state_executing_job;
	} else {
		state &= ~lbp::state_executing_job;
	}

	if (m_job_cmd == lbp::cmd_frame_begin) {
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

float MovementSim::getMoveProgress() const
{
	double total = xydistance(m_target_pos, m_start_pos);
	if (total < 1.0) {
		return 1.0;
	}
	return static_cast<float>(xydistance(m_pos, m_start_pos) / total);
}

void LaserSettings::setRaster(lbp::CmdPayload &payload)
{
	if (payload.cmd() != lbp::cmd_raster_power) return;

	num_rasters = (payload.size() - 3) >> 1;
	gLog().push(Log::INFO, QString("set raster ") + QString::number(num_rasters));

	if (!num_rasters) {
		return;
	}

	// Assume index byte has already been read
	for (size_t i = 0; i < num_rasters; i++) {
		float power = normalize(payload.readShortArg());
		raster_powers[i] = power;
		gLog().push(Log::INFO, QString("----- ") + QString::number(power));
	}
}

void LaserSettings::progressRaster(float progress)
{
	if (num_rasters > 0) {
		power_max = raster_powers[(size_t) (progress * (num_rasters - 1))];
		gLog().push(Log::INFO, "..." + QString::number(progress) + "..." + QString::number(power_max));
	}
}

void LaserSettings::clearRaster()
{
	gLog().push(Log::INFO, "clearing rasters");
	num_rasters = 0;
}
