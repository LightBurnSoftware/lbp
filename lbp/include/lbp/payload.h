// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#ifndef LBP_PAYLOAD_H
#define LBP_PAYLOAD_H

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <lbp/beio.h>
#include <lbp/spec.h>

namespace lbp {

/**
 * @brief The Payload class, a slightly structured container for the payload section of an incoming LB Protocol message.
 * It is intended to be filled only by the Parser class, or copied from another payload.
 * Memory is stack-allocated, so this class is templated by total capacity.
 * We expect only two broad catagories of message: Those with a command and a few arguments, and "file chunks", consisting of
 * many smaller messages. Thus, two hard-coded template specializations are defined below: CmdPayload and MaxPayload.
 *
 * The readArg methods are provided as convenient ways to incrementally read values from the "argument" section of the payload.
 * The "argument section" is defined as the section after the "cmd" value, which is size_cmd bytes long.
 * These methods advance an "argument read cursor" by the respective number of bytes.
 */
template<size_t Capacity>
class Payload
{
public:
	/** Public version of Capacity template argument. */
	static constexpr size_t kCapacity = Capacity;

	/** Default Constructor */
	Payload() = default;

	/** Copy Constructor */
	Payload(const Payload &other)
		: m_size(other.m_size)
		, m_cursor(size_cmd)
	{
		memcpy(m_data, other.m_data, m_size);
	}

	/** Move Constructor */
	Payload(Payload &&other) noexcept
		: m_size(other.m_size)
		, m_cursor(other.m_cursor)
	{
		std::memcpy(m_data, other.m_data, other.m_size);
		other.m_size = 0;
		other.m_cursor = size_cmd;
	}

	/** Copy Assignment */
	Payload &operator=(Payload &other) noexcept
	{
		if (&other != this) {
			m_size = other.m_size;
			m_cursor = other.m_cursor;
			std::memcpy(m_data, other.m_data, other.m_size);
		}
		return *this;
	}

	/** Move Assignment */
	Payload &operator=(Payload &&other) noexcept
	{
		if (*other != this) {
			m_size = other.m_size;
			m_cursor = other.m_cursor;
			std::memcpy(m_data, other.m_data, other.m_size);
			other.m_size = 0;
			other.m_cursor = size_cmd;
		}
		return *this;
	}

	/** @return The size of the data in this payload. */
	size_t size() const { return m_size; }

	/** @return The LB protocol command code. */
	uint16_t cmd() const
	{
		if (m_size == 0) {
			return 0;
		}
		if (m_size == 1) {
			return m_data[0];
		}
		return ((uint16_t) m_data[0] << 8) | m_data[1];
	}

	/** @return A const pointer to the payload data. */
	const uint8_t *data() const { return m_data; }

	/** @return A non-const pointer to the payload data. */
	uint8_t *data() { return m_data; }

	/** @return A const pointer to the "arguments" section of data, assuming data begins with a short cmd */
	const uint8_t *args() const
	{
		return m_data + size_cmd;
	}

	/** @brief reset Clears and reallocates the memory. */
	void reset(uint16_t size)
	{
		m_size = size;
		m_cursor = size_cmd;
	}

	/** @return the number of bytes yet to be read with read* commands */
	uint16_t available() const
	{
		return m_size - m_cursor;
	}

	/**
	 * @brief Read the next 8-bit byte from the args section of the payload. Advances arg read cursor by 1.
	 * @return The next Byte from the args section of the payload. -1 if there are no more bytes to read.
	 */
	uint8_t readByteArg()
	{
		if (available() < 1) {
			return -1;
		}
		uint8_t val = m_data[m_cursor];
		m_cursor += 1;
		return val;
	}

	/**
	 * @brief Read the next 16-bit short from the args section of the payload. Advances arg read cursor by 2.
	 * @return The next short from the args section of the payload. -1 if there were not two bytes to read.
	 */
	int16_t readShortArg()
	{
		if (available() < 2) {
			return -1;
		}
		int16_t val = readBE16(m_data + m_cursor);
		m_cursor += 2;
		return val;
	}

	/**
	 * @brief Read the next 32-bit int from the args section of the payload. Advances arg read cursor by 4.
	 * @return The next int from the args section of the payload. -1 if there were not four bytes to read.
	 */
	int32_t readIntArg()
	{
		if (available() < 4) {
			return -1;
		}
		int32_t val = readBE32(m_data + m_cursor);
		m_cursor += 4;
		return val;
	}

	/**
	 * @brief Read the next bytes from the arg section of the payload. Advances arg read cursor by len.
	 * @param dst The destination buffer.
	 * @param len The number of bytes to read.
	 * @return True if len bytes could be read, false if there were not that many bytes remaining to read.
	 */
	bool readDataArg(uint8_t *dst, uint16_t len)
	{
		if (available() < len) {
			return false;
		}
		std::memcpy(dst, m_data + m_cursor, len);
		m_cursor += len;
		return true;
	}

private:
	uint8_t m_data[Capacity];	// Stack-allocated data buffer.
	uint16_t m_size = 0;		// The size, in bytes, of m_data. <= Capacity.
	uint16_t m_cursor = size_cmd; // Read cursor for the read* functions.
};

/** Most payloads will be small "command" payloads. They might be stored in command queues. */
using CmdPayload = Payload<size_max_cmd_payload>;

/** Some payloads will quite large. They might be sections of job files containing many messages. */
using MaxPayload = Payload<size_file_payload>;

/**
 * @brief A helper function to copy a maximum-capacity payload from a parser to a much smaller command.
 * @param mp The maximum-capacity payload in question.
 * @return A payload of much smaller capacity which can be placed in command queues.
 */
CmdPayload toCmd(const MaxPayload &mp);

} // namespace lbp

#endif // LBP_PAYLOAD_H
