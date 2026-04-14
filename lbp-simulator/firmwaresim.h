// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include "configuration.h"
#include "connection.h"
#include "filesystem.h"
#include "movement.h"
#include "simstate.h"
#include "simutils.h"

#include <lbp/message.h>
#include <lbp/parser.h>
#include <lbp/queue.h>

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
	/**
	 * @brief Constructor
	 * @param conn The transport class, enables communication with LightBurn.
	 */
	FirmwareSim(Connection &conn);

	/**
	 * @brief Execute a single iteration of the simulation loop.
	 *
	 * This loop will:
	 * 1. Parse received bytes from the connection into lbp payloads.
	 * 2: (If executing a received file) Parse received file into lbp payloads.
	 * 3. Process these payloads.
	 * 4. Update the simulation.
	 * 5. Send any generated output messages via the connection.
	 *
	 * @param ms The number of milliseconds since the last loop.
	 * @return A structure containing useful simulated machine state.
	 */
	SimState loop(int ms);

private:
	/** Process incoming input. */
	bool process(lbp::MaxPayload &payload);

	/** Update the simulation. */
	void update(int ms);

	Connection &m_connection; // Connection to LightBurn - sends and receives bytes.
	Configuration
		m_config; // Configuration component - manages reading, writing, and storing config values.
	MovementSim m_movement;	 // Movement component - simulates movement and laser actions.
	FileSystem m_filesystem; // Filesystem component - receives and manages files from LightBurn.
	OutputQueue m_out_q;	 // Output message queue.
	uint32_t m_fw_state = lbp::state_idle; // Machine state flags, returned with `cmd_get_state`.
};
