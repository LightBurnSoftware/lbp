// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include "correction.h"
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
	uint32_t getFwState(uint32_t state);

	/** Rewind the current filebuffer and parser */
	void reset();

	/**
	 * @brief Feed the parser with data from the filebuffer.
	 * @return the number of bytes fed from the filebuffer into the parser.
	 */
	size_t feedParser();

	/** @return The parser that is loaded with file data. */
	FileParser &parser();

	/** @return True if a file was successfully opened and reading begun. */
	bool startReading();
	/** @return the size of the job file being read (valid while reading), for job progress. */
	qint64 jobFileSize() const { return m_file.size(); }
	void stopReading();
	bool isReading() const;

	/** @brief Galvo mode accepts file_type_correction. Leaving galvo mode discards any stored table. */
	void setGalvo(bool galvo);

	/** @return The stored lens correction table (valid == false if none). */
	const CorrectionTable &correction() const { return m_correction; }

	/** @return True once after a new correction table has been received and stored. */
	bool takeNewCorrection();

private:
	enum class State { Idle, Receiving, Reading };
	/** @return 0 on success, or an err_* code. */
	uint16_t startReceiving(lbp::MaxPayload &payload);
	void stopReceiving();
	/** Write one cmd_file_chunk at its offset. @return 0 on success, or an err_* code (detail filled in). */
	uint16_t receive(const lbp::MaxPayload &payload, uint32_t &detail);
	/** Handle cmd_file_end: check the CRC-32, then store the file. Ends the transfer either way.
	 *  @return 0 on success, or an err_* code. */
	uint16_t finishReceiving(const lbp::MaxPayload &payload);
	/** Close and delete the .part file of an unfinished or failed transfer. */
	void discardReceiveFile();

	/** Validate and store a received correction file. @return 0 on success, or an err_* code. */
	uint16_t finishCorrection(uint32_t &detail);

	State m_state = State::Idle;
	FileParser m_parser;
	QFile m_file;						// the stored job, while it's being read
	QFile m_receive_file;				// "<target>.part": the file being received, sized by cmd_file_begin
	QString m_receive_target;			// where it goes once its CRC-32 checks out
	uint32_t m_receive_size = 0;		// declared size, for the chunk bounds check
	uint8_t m_receiving_type = 0;		// file_type_* of the file being received
	bool m_galvo = false;
	CorrectionTable m_correction;
	bool m_new_correction = false;
};
