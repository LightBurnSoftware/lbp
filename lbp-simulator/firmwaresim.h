// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include "configuration.h"
#include "transport.h"
#include "filesystem.h"
#include "movement.h"
#include "simstate.h"
#include "simutils.h"

#include <lbp/message.h>
#include <lbp/parser.h>
#include <lbp/queue.h>

#include <QElapsedTimer>

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

	/**
	 * @brief Stop previous transport connection and start new one.
	 * @param conn The new connection.
	 */
	void setTransport(Transport *conn);

	/** @brief Called when bytes are available to be read from transport. */
	void rxCallback(const uint8_t *bytes, size_t len);

private:
	/** Update the simulation. */
	SimState update(int ms);
	/** send outgoing messages */
	void tx();
	/** Process incoming input. */
	bool process(lbp::MaxPayload &payload);

	Transport *m_transport = nullptr; // Connection to LightBurn - sends and receives bytes.
	WireParser m_parser; // Buffers and parses incoming bytes into messages.
	Configuration m_config; // Manages reading, writing, and storing config values.
	MovementSim m_movement; // Movement component - simulates movement and laser actions.
	FileSystem m_filesystem; // Filesystem component - receives and manages files from LightBurn.
	OutputQueue m_out_q; // Output message queue.
	uint32_t m_fw_state = lbp::state_idle; // Machine state flags, returned with `cmd_get_state`.

	QElapsedTimer m_profile;
};
