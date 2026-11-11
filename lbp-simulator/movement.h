// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include "configuration.h"
#include "correction.h"
#include "simstate.h"
#include "simutils.h"
#include "vec8.h"

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
	int32_t pulse_width_ns = 0; // galvo MOPA sources (cmd_laser_pulse_width)
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
	Vec8   start;            // wrapped position when the move began
	Vec8   delta;            // signed displacement; may exceed a rotary axis limit
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
	Vec8   start;
	double dir[Vec8::kSize] = {};  // unit direction, indexed like Vec8
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
	 * @param file_offset For a job-file command, its offset in the file (for job progress); else kNoFileOffset.
	 * @return True if the  was able to process the request, false otherwise.
	 */
	bool process(lbp::MaxPayload &request, OutputQueue &out_q, CmdSource source, uint32_t file_offset = kNoFileOffset);

	/** @return job progress: bytes of the job file whose commands have finished (the offset of the one running now). */
	uint32_t executedBytes() const { return m_exec_offset; }

	/** @brief Advance position and laser state by dt_s seconds. Exact for any dt (moves are interpolated). */
	void step(double dt_s);

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

	/**
	 * @brief Switch between gantry and galvo behaviour: travel limits and galvo command support.
	 * Call FirmwareSim::setMode instead, which also stops everything first.
	 */
	void setMode(MachineMode mode);
	MachineMode mode() const { return m_mode; }

	/** @brief Recompute travel limits from the mode and configuration (e.g. after cfg_field_size changes). */
	void refreshBounds();

	/**
	 * @brief Set the lens correction applied to galvo motion (cmd_galvo_correction), or nullptr for none.
	 * The caller checks that the table fits first, owns it, and turns correction off before replacing or
	 * dropping it. Turned off by setMode.
	 */
	void setCorrection(const CorrectionTable *table) { m_correction = table; }

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
	/** Calculate the axis delta for a relative move. @param idx The axis (see Vec8). */
	int64_t relativeDelta(int idx, int64_t arg) const;
	/** Calculate the axis delta for an absolute move. @param idx The axis (see Vec8). */
	int64_t absoluteDelta(int idx, int64_t arg) const;
	/** Calculate the axis delta for a move to machine 0. @param idx The axis (see Vec8). */
	int64_t homeDelta(int idx) const;

	bool decodeMoveDelta(lbp::CmdPayload &p, Vec8 &delta);

	/** Speeds for each axis group, in um/s (0 = not set). */
	struct SpeedSet {
		uint32_t xy = 0;
		uint32_t z = 0;
		uint32_t u = 0;
	};

	/** Store a cut, travel, or jog speed command in its family. @return False if p is not a speed command. */
	bool takeSpeed(lbp::CmdPayload &p);
	/** The speeds that govern a move command: its family's, or the configured defaults. */
	SpeedSet speedsFor(uint16_t cmd) const;
	/** Choose the path metric and rate for a move. */
	bool pacingFor(const Vec8 &delta, const SpeedSet &speeds, double &length, double &speed) const;
	bool beginMove(const Vec8 &delta, const SpeedSet &speeds);
	void beginJog(const Vec8 &dir, int32_t target_vel);
	void endJog();

	/** Apply each axis's travel limit: linear axes clip, rotary axes wrap. */
	Vec8 applyLimits(const Vec8 &vec) const;

	Configuration &m_config;	 // reference to configuration component, for certain lookups.
	State m_state = State::Idle; // "movement state" - controls internal state machine.
	Move m_move;				 // the Move currently executing
	Jog m_jog;					 // the Jog currently executing
	int m_rotary_axis = -1; // rotary axis idx (see Vec8), or -1 for none, refreshed when a move or jog begins.
	Vec8 m_pos;				// current laser position
	Vec8 m_job_origin;		// origin offset for absolute moves during the current job.
	MachineMode m_mode = MachineMode::Gantry;
	// Lens correction being applied, or nullptr. Owned by the FileSystem. Not applied yet: see step().
	const CorrectionTable *m_correction = nullptr;
	Vec8 m_min_pos;			// travel limits per axis: gantry [0, max]; galvo X/Y [-field/2, +field/2].
	Vec8 m_max_pos;
	double m_dwell_remaining_s = 0.0;  // remaining seconds to dwell if in dwelling state.
	SpeedSet m_cut_speed;		  // speeds for cut moves (cmd_cut_speed_*)
	SpeedSet m_travel_speed;	  // speeds for travel moves (cmd_travel_speed_*)
	SpeedSet m_jog_speed;		  // speeds for operator moves (cmd_jog_speed_*)
	LaserSettings m_laser_1;	  // current settings for laser 1
	LaserSettings m_laser_2;	  // current settings for laser 2
	PausedState m_paused;		  // state to resume after pause.
	CmdQueue m_cmd_q;			  // queue for scheduled movement commands.
	uint32_t m_exec_offset = 0;	  // job file offset of the command running now (job progress); reset by stop()
	bool m_in_job = false;		  // state boolean - are we currently executing a job?
	bool m_in_frame = false;	  // state boolean - are we currently framing?
	bool m_job_ended = false;	// latching flag to indicate a job has completed.

	// Galvo state (galvo mode only). The simulator stores these; it doesn't simulate their timing yet.
	int32_t m_galvo_laser_on_delay_us = 0;
	int32_t m_galvo_laser_off_delay_us = 0;
	int32_t m_galvo_polygon_delay_us = 0;
	int32_t m_galvo_end_delay_us = 0;
	int32_t m_galvo_travel_delay_min_us = 0;
	int32_t m_galvo_travel_delay_max_us = 0;
	int32_t m_galvo_travel_delay_dist_um = 0;
	bool m_guide_light = false;
	bool m_focus_light = false;
	bool m_warned_galvo_rotary = false; // logged once per mode change: rotary on X/Y is ignored in galvo mode
};
