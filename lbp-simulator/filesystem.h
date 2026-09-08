// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include "simutils.h"

#include <QFile>

/**
 * @brief A simulation component responsible for managing files.
 *
 * This component is responsible for processing incoming file-related lbp messages
 * (e.g. concatenating incoming file messages into a contiguous file.)
 *
 * It also provides its own buffered parser for parsing said files into lbp payloads.
 */
class FileSystem
{
public:
	FileSystem() = default;

	/**
	 * @brief Attempt to process the given request. May enqueue an output packet.
	 *
	 * @param request The request to process.
	 * @param out_q Storage for resulting output packets.
	 * @return True if the  was able to process the request, false otherwise.
	 */
	bool process(lbp::MaxPayload &request, OutputQueue &out_q);

	/** @brief Stop any long-running operation (like receiving a file.) */
	void stop();

	/**
	 * @brief Gather the flags for firmware state from the filesystem component.
	 * @param state The currently understood firmward state.
	 * @return The firmware state, updated with whatever flags represent the filesystem state.
	 */
	uint32_t getFwState(uint32_t state) const;

	/** Rewind the current filebuffer and parser */
	void reset();

	/** Feed the parser with data from the filebuffer. */
	void feedParser();

	/** @return The parser that is loaded with file data. */
	FileParser &parser();

	void startReading();
	void stopReading();

private:
	enum class State { Idle, Receiving, Reading };
	void startReceiving(lbp::MaxPayload &payload);
	void stopReceiving();
	void receive(const lbp::MaxPayload &payload);

	State m_state = State::Idle;
	FileParser m_parser;
	QFile m_file;
};
