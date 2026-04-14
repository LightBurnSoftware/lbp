// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#ifndef LBP_PARSER_H
#define LBP_PARSER_H

#include <lbp/checksum.h>
#include <lbp/beio.h>
#include <lbp/payload.h>
#include <lbp/ringbuffer.h>
#include <lbp/spec.h>

namespace lbp {

/**
 * A buffered parser for lbp payloads, templated by buffer capacity.
 * Capacity must be a power of two.
 *
 * Usage:
 *
 * Parser<4096> parser;
 *
 * parser.feed(incoming_data, len);
 *
 * while (parser.parseNext()) {
 *	 MaxPayload &p = parser.getPayload();
 *	 process(p);
 * }
 */
template <size_t Capacity>
class Parser
{
public:
	static constexpr size_t kCapacity = Capacity;

	Parser() = default;
	Parser(const Parser&) = delete;
	Parser& operator=(const Parser&) = delete;

	/**
	 * @brief parse the next payload from the internal buffer.
	 * @return True if a payload was completely parsed, false otherwise.
	 */
	bool parseNext()
	{
		while (!m_buffer.empty()) {
			switch (m_state) {
			case State::HeaderSync:
				seeklen += 1;
				m_header = (m_header << 8) | m_buffer.readByte();
				if (m_header == cmd_start) {
					m_state = State::Size;
					seeklen = 0;
				}
				break;
			case State::Size:
				if (m_buffer.read(m_shortbuffer, 2)) {
					m_payload.reset(readBE16(m_shortbuffer));
					m_state = State::Data;
				} else {
					return false;
				}
				break;
			case State::Data:
				if (m_buffer.read(m_payload.data(), m_payload.size())) {
					m_state = State::Checksum;
				} else {
					return false;
				}
				break;
			case State::Checksum:
				if (m_buffer.read(m_shortbuffer, 2)) {
					m_state = State::HeaderSync;
					if (readBE16(m_shortbuffer) == crc16(m_payload.data(), m_payload.size())) {
						return true;
					} else {
						numbad += 1;
					}
				} else {
					return false;
				}
				break;
			}
		}
		return false;
	}

	/** @return a reference to the current parsed payload. */
	MaxPayload &payload() { return m_payload; }

	/** Clears the buffer and resets the parser. */
	void clear()
	{
		m_buffer.clear();
		m_header = 0;
		m_state = State::HeaderSync;
		numbad = 0;
		seeklen = 0;
	}

	/**
	 * @brief Load the internal ringbuffer with incoming data to be parsed.
	 * @param src Data to be written to the internal buffer.
	 * @param len Number of bytes to attempt to write. May write less if there isn't enough room.
	 * @return The number of bytes successfully fed to the internal buffer.
	 */
	size_t feed(const uint8_t *src, size_t len)
	{
		len = std::min(len, m_buffer.freeSpace());
		if (m_buffer.write(src, len)) {
			return len;
		}
		return 0;
	}

	/** Debug value representing the number of bytes the parser searched unsuccessfully for a valid header. */
	size_t seeklen = 0;

	/** Debug value representing the number of packets that have been rejected with an invalid checksum. */
	size_t numbad = 0;

private:
	enum class State {
		HeaderSync,
		Size,
		Data,
		Checksum
	};

	State m_state = State::HeaderSync;	// Controls the parsing state machine.
	RingBuffer<Capacity> m_buffer;		// Stores bytes to be parsed.
	MaxPayload m_payload;				// Stores current payload tha we're working on.
	uint32_t m_header = 0;			 	// Storage for the header we're searching for.
	uint8_t m_shortbuffer[2];		  	// Buffer for several two-byte chunks of data.
};

} // namespace lbp

#endif // LBP_PARSER_H

