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
		, m_cursor(other.m_cursor)
	{
		std::memcpy(m_data, other.m_data, m_size);
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
	Payload &operator=(const Payload &other) noexcept
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
		if (&other != this) {
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

	/** @brief reset Reset the size and read cursor. */
	void reset(uint16_t size)
	{
		m_size = size <= Capacity ? size : Capacity;
		m_cursor = size_cmd;
	}

	/** @return the number of bytes yet to be read with read* commands */
	size_t available() const
	{
		return m_cursor >= m_size ? 0 : size_t(m_size - m_cursor);
	}

	/**
	 * @brief Read the next len bytes from the arg section of the payload. Advances arg read cursor by len.
	 * @param dst The destination buffer.
	 * @param len The number of bytes to read.
	 * @return True if len bytes could be read, false if there were not that many bytes remaining to read.
	 */
	[[nodiscard]] bool readData(uint8_t *dst, uint16_t len)
	{
		if (available() < len) return false;
		std::memcpy(dst, m_data + m_cursor, len);
		m_cursor += len;
		return true;
	}

	/**
	 * @brief Read the next byte from the args section and advance the read cursor accordingly.
	 * @param out Reference parameter for the output value.
	 * @return True if the value was successfully read and the read cursor was advanced.
	 *         False if the payload was too small for the requested value, in which case the read cursor is unchanged.
	 */
	[[nodiscard]] bool readArg(uint8_t &out)
	{
		if (available() < 1) return false;
		out = m_data[m_cursor];
		m_cursor += 1;
		return true;
	}

	/**
	 * @brief Read the next two bytes from the args section as an unsigned 16-bit integer and advance the read cursor accordingly.
	 * @param out Reference parameter for the output value.
	 * @return True if the value was successfully read and the read cursor was advanced.
	 *         False if the payload was too small for the requested value, in which case the read cursor is unchanged.
	 */
	[[nodiscard]] bool readArg(uint16_t &out)
	{
		if (available() < 2) return false;
		out = readBE16(m_data + m_cursor);
		m_cursor += 2;
		return true;
	}

	/**
	 * @brief Read the next two bytes from the args section as a signed 16-bit integer and advance the read cursor accordingly.
	 * @param out Reference parameter for the output value.
	 * @return True if the value was successfully read and the read cursor was advanced.
	 *         False if the payload was too small for the requested value, in which case the read cursor is unchanged.
	 */
	[[nodiscard]] bool readArg(int16_t &out)
	{
		if (available() < 2) return false;
		out = static_cast<int16_t>(readBE16(m_data + m_cursor));
		m_cursor += 2;
		return true;
	}

	/**
	 * @brief Read the next four bytes from the args section as an unsigned 32-bit integer and advance the read cursor accordingly.
	 * @param out Reference parameter for the output value.
	 * @return True if the value was successfully read and the read cursor was advanced.
	 *         False if the payload was too small for the requested value, in which case the read cursor is unchanged.
	 */
	[[nodiscard]] bool readArg(uint32_t &out)
	{
		if (available() < 4) return false;
		out = readBE32(m_data + m_cursor);
		m_cursor += 4;
		return true;
	}

	/**
	 * @brief Read the next four bytes from the args section as a signed 32-bit integer and advance the read cursor accordingly.
	 * @param out Reference parameter for the output value.
	 * @return True if the value was successfully read and the read cursor was advanced.
	 *         False if the payload was too small for the requested value, in which case the read cursor is unchanged.
	 */
	[[nodiscard]] bool readArg(int32_t &out)
	{
		if (available() < 4) return false;
		out = static_cast<int32_t>(readBE32(m_data + m_cursor));
		m_cursor += 4;
		return true;
	}

	/**
	 * @brief Read several arguments in sequence from the arg section of the payload.
	 * @tparam Args Types of the values to read. Must be uint8_t, int16_t, uint16_t, uint32_t, or int32_t.
	 * @param args lvalue references to receive the values, in payload order.
	 * @return True if every value was successfully read. False if the payload was too small for the
	 *         requested value, in which case the read cursor is unchanged.
	 */
	template <typename ... Args>
	[[nodiscard]] bool readArgs(Args &... args)
	{
		const uint16_t saved = m_cursor;
		if ((readArg(args) && ...)) return true; // C++17 "folding" syntax
		m_cursor = saved; // if read fails, rewind cursor.
		return false;
	}

private:
	uint8_t m_data[Capacity] = {};	// Stack-allocated data buffer.
	uint16_t m_size = 0;			// The size, in bytes, of m_data. <= Capacity.
	uint16_t m_cursor = size_cmd;	// Read cursor for the read* functions.
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
