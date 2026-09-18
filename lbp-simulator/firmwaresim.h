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
	 * @brief Step the firmware by a pre-defined timestep.
	 *
	 * This function will also send resulting output messages,
	 * as well as process job messages streamed from memory
	 * if a job is in progress.
	 *
	 * @return A structure containing useful simulated machine state.
	 */
	SimState step();

	/** @brief Called when bytes are available to be read from transport. */
	void rxCallback(const uint8_t *bytes, size_t len);

	void setTxCallback(TxCallback cb) { m_tx_callback = cb; }

private:
	/** Process incoming input. */
	bool process(lbp::MaxPayload &payload);

	/** Send queued output messages to transport. */
	void tx();

	WireParser m_parser; // Buffers and parses incoming bytes into messages.
	Configuration m_config; // Manages reading, writing, and storing config values.
	MovementSim m_movement; // Movement component - simulates movement and laser actions.
	FileSystem m_filesystem; // Filesystem component - receives and manages files from LightBurn.
	OutputQueue m_out_q; // Output message queue.
	TxCallback m_tx_callback; // Callback for sending output messages.
	uint32_t m_fw_state = lbp::state_idle; // Machine state flags, returned with `cmd_get_state`.
};
