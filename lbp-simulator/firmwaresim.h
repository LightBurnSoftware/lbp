// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include "configuration.h"
#include "filesystem.h"
#include "movement.h"
#include "simstate.h"
#include "simutils.h"

#include <lbp/message.h>
#include <lbp/parser.h>
#include <lbp/queue.h>

using TxCallback = std::function<void(const uint8_t*, size_t)>;

/**
 * @brief The FirmwareSim class. This is the entry-point for actual firmware simulation.
 *
 * Code in this class is intended to serve as an example for lbp library usage.
 * It's not real firmware, only a simulation, but the broad structure should be applicable.
 *
 * This class contains several simulation sub-components.
 * FileSystem, Configuration, and MovementSim each have responsibility for processing certain commands.
 * FirmwareSim is responsible for certain top-level commands.
 */
class FirmwareSim
{
public:
	FirmwareSim();

	/**
	 * @brief Advance the firmware by one iteration of dt_ns simulated time.
	 *
	 * Feeds job messages streamed from memory (if a job is in progress),
	 * advances the movement simulation, and sends resulting output messages.
	 *
	 * Any dt is exact for motion, but the job queue is only refilled once per
	 * call, so callers should keep dt small to avoid the
	 * movement queue running dry mid-step.
	 *
	 * @param dt_ns Simulated time to advance, in nanoseconds.
	 * @return A structure containing useful simulated machine state.
	 */
	SimState step(int64_t dt_ns);

	/** @brief Called when bytes are available to be read from transport. */
	void rxCallback(const uint8_t *bytes, size_t len);

	void setTxCallback(TxCallback cb) { m_tx_callback = cb; }

	/** Stop all movement, file operations, and queued commands. */
	void stop();

	/** Simulate a fault (e.g. an open interlock). While active, motion is stopped and refused. */
	void setFault(bool active);

	/** Switch between gantry and galvo simulation. A full reset: stops everything first. */
	void setMode(MachineMode mode);

private:
	/** Process incoming input. */
	bool process(lbp::MaxPayload &payload);

	/** Send queued output messages to transport. */
	void tx();

	/** Keep the movement queue fed from the job file while a job is executing. */
	void pumpJob();
	/** Start the loaded job. txid: the cmd_execute's transaction id, reported back in the job's event_result. */
	void beginJob(uint16_t txid);
	/**
	 * End the running job (no-op if none): send the final event_job_progress, then event_result with result
	 * (0 = ran to the end, err_canceled, err_fault). Call before anything that resets the movement simulator.
	 */
	void endJob(uint16_t result);
	void checkJobComplete();
	/** Send event_job_progress (progress_bytes_executed) while a job runs: on change, at most every 250 ms; at least every 2 s. */
	void reportProgress(int64_t dt_ns);

	void updateFwState();

	/** Handle cmd_galvo_correction (galvo mode). */
	void processCorrection(lbp::MaxPayload &payload);

	WireParser m_parser; // Buffers and parses incoming bytes into messages.
	Configuration m_config; // Manages reading, writing, and storing config values.
	MovementSim m_movement; // Movement component - simulates movement and laser actions.
	FileSystem m_filesystem; // Filesystem component - receives and manages files from LightBurn.
	OutputQueue m_out_q; // Output message queue.
	TxCallback m_tx_callback; // Callback for sending output messages.
	uint32_t m_fw_state = lbp::state_idle; // Machine state flags, returned with `cmd_get_state`.
	uint16_t m_last_txid = lbp::txid_none; // Most recent transaction id processed, for retry detection.
	bool m_fault = false; // Simulated fault, reported as state_fault.
	uint32_t m_job_size = 0; // Size of the running job file, for the final progress report.
	uint16_t m_job_txid = lbp::txid_none; // The cmd_execute that started the running job, for its event_result.
	uint32_t m_last_progress = UINT32_MAX; // Last progress reported (UINT32_MAX: none yet this job).
	int64_t m_since_progress_ns = 0; // Time since the last progress report.
};
