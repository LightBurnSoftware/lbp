// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#ifndef LBP_PARSER_H
#define LBP_PARSER_H

#include <algorithm>
#include <cassert>

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
 * const size_t fed = parser.feed(incoming_data, len);
 * // fed may be < len if the buffer is full; keep the remainder and retry.
 *
 * while (parser.parseNext()) {
 *	 MaxPayload &p = parser.payload();
 *	 process(p);
 * }
 */
template <size_t Capacity>
class Parser
{
public:
	static_assert(Capacity >= size_file_msg, "Parser capacity must hold a maximum-size message");

	static constexpr size_t kCapacity = Capacity;

	Parser() = default;
	Parser(const Parser&) = delete;
	Parser& operator=(const Parser&) = delete;

	/**
	 * @brief Parse the next payload from the internal buffer.
	 * @return True if a payload was completely parsed, false otherwise.
	 */
	bool parseNext()
	{
		while (!m_buffer.empty()) {
			switch (m_state) {
			case State::HeaderSync:
				if (m_buffer.readByte(m_byte)) {
					seeklen += 1;
					m_header = (m_header << 8) | m_byte;
					if (m_header == magic_start) {
						m_buffer.setBookmark(); // If a bad packet is detected, rewind to just after this header.
						m_state = State::TxId;
						seeklen = 0;
					}
				}
				else {
					return false;
				}
				break;
			case State::TxId:
				if (m_buffer.read(m_txid_buffer, size_txid)) {
					m_state = State::Cmd;
				} else {
					return false;
				}
				break;
			case State::Cmd:
				if (m_buffer.read(m_cmd_buffer, size_cmd)) {
					m_state = State::Size;
				} else {
					return false;
				}
				break;
			case State::Size:
				if (m_buffer.read(m_len_buffer, size_len)) {
					uint16_t len = readBE16(m_len_buffer);
					if (len > size_file_args) {
						// Reject messages of invalid size.
						size_rejections += 1;
						m_header = 0;
						m_buffer.rewind();
						m_state = State::HeaderSync;
					} else {
						m_payload.reset(readBE16(m_cmd_buffer), readBE16(m_txid_buffer), len);
						m_state = State::Args;
					}
				} else {
					return false;
				}
				break;
			case State::Args:
				if (m_buffer.read(m_payload.data(), m_payload.size())) {
					m_state = State::Checksum;
				} else {
					return false;
				}
				break;
			case State::Checksum:
				if (m_buffer.read(m_crc_buffer, 2)) {
					m_state = State::HeaderSync;
					uint16_t crc = crc16_begin();
					crc = crc16_update(crc, m_txid_buffer, size_txid);
					crc = crc16_update(crc, m_cmd_buffer, size_cmd);
					crc = crc16_update(crc, m_len_buffer, size_len);
					crc = crc16_update(crc, m_payload.data(), m_payload.size());
					if (readBE16(m_crc_buffer) == crc16_end(crc)) {
						m_buffer.clearBookmark();
						return true;
					} else {
						// Reject messages with invalid checksum.
						checksum_rejections += 1;
						m_header = 0;
						m_buffer.rewind();
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
		checksum_rejections = 0;
		size_rejections = 0;
		seeklen = 0;
	}

	/**
	 * @brief Load the internal ringbuffer with incoming data to be parsed.
	 * @param src Data to be written to the internal buffer.
	 * @param len Number of bytes to attempt to write. May write less if there isn't enough room.
	 * @return The number of bytes successfully fed to the internal buffer.
	 */
	[[nodiscard]] size_t feed(const uint8_t *src, size_t len)
	{
		len = std::min(len, m_buffer.freeSpace());
		[[maybe_unused]] const bool ok = m_buffer.write(src, len);
		assert(ok && "write cannot fail: len was clamped to freeSpace()");
		return len;
	}

	/** @return The number of bytes that can be fed to the parser. */
	size_t freeSpace() const { return m_buffer.freeSpace(); }

	/** Debug value representing the number of bytes the parser searched unsuccessfully for a valid header. */
	size_t seeklen = 0;

	/** Debug value representing the number of packets that have been rejected with an invalid checksum. */
	size_t checksum_rejections = 0;

	/** Debug value representing the number of packets that have been rejected with a bad length (possible misframe. */
	size_t size_rejections = 0;

private:
	enum class State {
		HeaderSync,
		TxId,
		Cmd,
		Size,
		Args,
		Checksum
	};

	State m_state = State::HeaderSync;	// Controls the parsing state machine.
	RingBuffer<Capacity> m_buffer;		// Stores bytes to be parsed.
	MaxPayload m_payload;				// Stores current payload tha we're working on.
	uint32_t m_header = 0;			 	// Storage for the header we're searching for.
	uint8_t m_crc_buffer[2] = {};	  	// Scratch for read value of the checksum.
	uint8_t m_txid_buffer[size_txid] = {}; // Raw txid bytes, kept for the checksum computation.
	uint8_t m_cmd_buffer[size_cmd] = {};	// Raw cmd bytes, kept for the checksum computation.
	uint8_t m_len_buffer[size_len] = {};	// Raw size bytes, kept for the checksum computation.
	uint8_t m_byte = 0;					// Buffer for reading one byte at a time.
};

} // namespace lbp

#endif // LBP_PARSER_H

