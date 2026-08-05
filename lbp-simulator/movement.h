// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include "configuration.h"
#include "simstate.h"
#include "simutils.h"
#include "vec4.h"

#include <cstdint>
#include <lbp/payload.h>
#include <lbp/queue.h>

/** A helper struct for storing the current settings of a laser. */
struct LaserSettings {
	float freq = 0.f;
	float power_min = 0.f;
	float power_max = 0.f;
	size_t num_rasters = 0;
	float raster_powers[lbp::size_max_cmd_args >> 1];
	bool on = false;
	bool enabled = false;

	/** Call only after reading payload's laser index*/
	void setRaster(lbp::CmdPayload &payload);
	/** Update the current max power to the respective raster power based on progress ratio [0.0, 1.0]. */
	void progressRaster(float progress);
	void clearRaster();
	bool isRaster() const;
};

/**
 * @brief The MovementSim - simulates the movement and physical state of the laser.
 *
 * This is the component responsible for simulating jobs, framing, jogging, etc.
 *
 * When it processes a payload, and that payload is related to laser movement or cutting, or other
 * job operations, it will send a return command and then push that command onto a FIFO command queue.
 * As long as it is not paused, it will process this command queue in order, moving the toolhead, setting
 * laser power, until the queue is exhausted.
 *
 * This component is not intended to represent real-world motion-planning. It does not, as yet, simulate
 * acceleration or deceleration.
 */
class MovementSim
{
public:
	MovementSim(Configuration &config);

	/**
	 * @brief Attempt to process the given request. May enqueue an output packet.
	 *
	 * If the command is a movement or laser command, it will enqueue it in the internal command queue.
	 *
	 * @param request The request to process.
	 * @param out_q Storage for resulting output packets.
	 * @return True if the  was able to process the request, false otherwise.
	 */
	bool process(lbp::MaxPayload &request, OutputQueue &out_q);

	/**
	 * @brief Update the simulated position and laser state.
	 * @param ms milliseconts that have passed since the last update.
	 */
	void update(int ms);

	/** @brief stop all movement and cutting and clear the command queue. */
	void stop();

	/** @brief pause command execution. */
	void pause();

	/** @brief resume command job. */
	void resume();

	/**
	 * @brief Gather the flags for firmware state from the movement simulation component.
	 * @param fw_state The currently understood firmward state.
	 * @return The firmware state, updated with whatever flags represent the movement simulation state.
	 */
	uint32_t getFwState(uint32_t fw_state);

	/** @return a structure representing the physical state of the toolhead, for display usage. */
	SimState getSimState() const;

	/** @return true if the internal command queue can enqueue another command, false otherwise. */
	bool canEnqueue() const;

private:
	enum class State { Idle, Jogging, Moving, Dwelling, Paused };
	struct PausedState {
		bool laser_1 = false;
		bool laser_2 = false;
		State state = State::Idle;
	};

	State updateTarget();
	void startJog(Vec4 dir);
	void stopJog();

	float getMoveProgress() const;

	Configuration &m_config; // reference to configuration component, for certain lookups.
	State m_state = State::Idle; // "movement state" - controls internal state machine.
	Vec4 m_pos; // current position vector.
	Vec4 m_start_pos; // start position of the current movement vector.
	Vec4 m_target_pos; // user-specified target position.
	Vec4 m_job_origin; // origin offset for absolute moves during the current job.
	Vec4 m_max_pos; // configured maximum dimension (assume Quadrant I).
	Vec4 m_vel; // current velocity vector.
	int32_t m_dwell_ms = 0; // ms to dwell, if we're in that state
	int32_t m_dwell_acc_ms = 0; // accumulated ms spent dwelling
	uint32_t m_target_vel_xy = 0; // user-specified target velocity (xy)
	uint32_t m_target_vel_z = 0; // user-specified target velocity (z)
	uint32_t m_target_vel_u = 0; // user-specified target velocity (u)
	LaserSettings m_laser_1; // current settings for laser 1
	LaserSettings m_laser_2; // current settings for laser 2
	PausedState m_paused; // state to resume after pause.
	CmdQueue m_cmd_q; // queue for scheduled movement commands.
	uint16_t m_job_cmd; // cache a "begin job" or "begin frame" command for state derivations.
};
