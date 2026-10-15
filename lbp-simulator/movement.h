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
	float raster_powers[lbp::size_max_cmd_args >> 1] = {};
	bool on = false;
	bool enabled = false;

	/** Call only after reading payload's laser index*/
	void setRaster(lbp::CmdPayload &payload);
	/** Update the current max power to the respective raster power based on progress ratio [0.0, 1.0]. */
	void progressRaster(float progress);
	void clearRaster();
};

/** Data describing a move in progress. */
struct Move {
	Vec4   start;            // wrapped position when the move began
	Vec4   delta;            // signed displacement; may exceed a rotary axis limit
	double length = 0.0;     // path distance used to pace the move (um)
	double traveled = 0.0;   // distance covered so far (um)
	double speed = 0.0;      // path speed (um/s)

	/** @return progress along the path, [0.0, 1.0]. */
	double progress() const { return length > 0.0 ? std::min(1.0, traveled / length) : 1.0; }
	bool done() const { return traveled >= length; }
	void reset() { *this = Move(); }
};

/** Data describing a jog in progress. A jog has no endpoint; it runs until stopped. */
struct Jog {
	Vec4   start;
	double dir[4] = {0, 0, 0, 0};  // unit direction, indexed like kAxes
	double traveled = 0.0;
	double speed = 0.0;
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
	 * @param source identies the source of the command to be processed.
	 * @return True if the  was able to process the request, false otherwise.
	 */
	bool process(lbp::MaxPayload &request, OutputQueue &out_q, CmdSource source);

	/** @brief Update the simulated position and laser state by a constant time step.  */
	void step();

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

	/** @return true if cmd_job_end has been executed since the last call. */
	bool takeJobEnded();

	/** @return true if nothing is executing and no commands are pending. */
	bool isIdle() const;

private:
	enum class State { Idle, Jogging, Moving, Dwelling, Paused };
	struct PausedState {
		bool laser_1 = false;
		bool laser_2 = false;
		State state = State::Idle;
	};

	State takeNextCommand();

	/** Refresh our knowledge of the configured rotary axis. Called when a move begins. */
	void refreshRotary();
	/** Calculate the axis delta for a relative move. */
	int64_t relativeDelta(int idx, int64_t arg) const;
	/** Calculate the axis delta for an absolute move. */
	int64_t absoluteDelta(int idx, int64_t arg) const;
	/** Calculate the axis delta for an a move to machine 0 */
	int64_t homeDelta(int idx) const;

	bool decodeMoveDelta(lbp::CmdPayload &p, Vec4 &delta);

	/** Choose the path metric and rate for a move. */
	bool pacingFor(const Vec4 &delta, double &length, double &speed) const;
	bool beginMove(const Vec4 &delta);
	void beginJog(const Vec4 &dir, int32_t target_vel);
	void endJog();

	/** Apply each axis's travel limit: linear axes clip, rotary axes wrap. */
	Vec4 applyLimits(const Vec4 &vec) const;

	Configuration &m_config;	 // reference to configuration component, for certain lookups.
	State m_state = State::Idle; // "movement state" - controls internal state machine.
	Move m_move;				 // the Move currently executing
	Jog m_jog;					 // the Jog currently executing
	int m_rotary_axis = -1; // rotary axis index, or -1 for none, refreshed when a move or jog begins.
	Vec4 m_pos;				// current laser position
	Vec4 m_job_origin;		// origin offset for absolute moves during the current job.
	Vec4 m_max_pos;			// configured maximum dimension (assume Quadrant I).
	double m_dwell_remaining_s = 0.0;  // remaining seconds to dwell if in dwelling state.
	uint32_t m_target_vel_xy = 0; // user-specified target velocity (xy)
	uint32_t m_target_vel_z = 0;  // user-specified target velocity (z)
	uint32_t m_target_vel_u = 0;  // user-specified target velocity (u)
	LaserSettings m_laser_1;	  // current settings for laser 1
	LaserSettings m_laser_2;	  // current settings for laser 2
	PausedState m_paused;		  // state to resume after pause.
	CmdQueue m_cmd_q;			  // queue for scheduled movement commands.
	bool m_in_job = false;		  // state boolean - are we currently executing a job?
	bool m_in_frame = false;	  // state boolean - are we currently framing?
	bool m_job_ended = false;	// latching flag to indicate a job has completed.
};
