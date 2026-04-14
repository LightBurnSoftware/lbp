// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include "simutils.h"

/**
 * @brief A simple utility class to assist in writing, reading, and parsing a recieved file.
 *
 * Contains helpful methods for concatenating the file together while it comes in, as well
 * as feeding a provided lbp::Parser.
 */
template<size_t Capacity>
class FileBuffer
{
public:
	static constexpr size_t kCapacity = Capacity;

	FileBuffer() = default;

	/** @return the number of bytes available to be read */
	size_t available() const { return m_head - m_tail; }

	/** @return the number of bytes available to be written */
	size_t freeSpace() const { return Capacity - available(); }

	/**
	 * @brief Read data from the buffer.
	 * @param dst Destination buffer.
	 * @param len Number of bytes to read.
	 * @return true on success. Return false and read nothing if len > available().
	 */
	bool read(uint8_t *dst, size_t len)
	{
		if (len > available()) {
			return false;
		}

		std::memcpy(dst, m_data + m_tail, len);

		m_tail += len;

		return true;
	}

	/**
	 * @brief Write data to the buffer.
	 * @param src Source data to write.
	 * @param len Number of bytes to write.
	 * @return true on success. Return false write nothing if len > freeSpace().
	 */
	bool write(const uint8_t *src, size_t len)
	{
		if (len > freeSpace()) {
			return false;
		}

		std::memcpy(m_data + m_head, src, len);

		m_head += len;

		return true;
	}

	/**
	 * @brief reset the filebuffer
	 * @param filesize the size of the file to store.
	 * @return True if filesize can fit in the static capacity, false otherwise.
	 */
	bool reset(size_t filesize)
	{
		m_head = 0;
		m_tail = 0;
		if (filesize > Capacity) {
			m_size = Capacity;
			return false;
		}
		m_size = filesize;
		return true;
	}

	/** Rewind read-cursor to zero. */
	void rewind() { m_tail = 0; }

	void feedParser(FileParser &parser) { m_tail += parser.feed(m_data + m_tail, available()); }

private:
	uint8_t m_data[Capacity];
	size_t m_head = 0;
	size_t m_tail = 0;
	size_t m_size = 0;
};

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

	/** Feed the parser with data from the filebuffer. */
	void feedParser();

	/** @return The parser that is loaded with file data. */
	FileParser &parser();

private:
	bool m_is_receiving = false; // true if the filesystem is in the process of receiving a file.
	FileParser m_parser;		 // buffered payload parser.
	FileBuffer<MaxFileSize> m_file_buffer; // simulated space for a fully loaded file.
};
