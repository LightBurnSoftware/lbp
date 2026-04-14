// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#ifndef LBP_MESSAGE_H
#define LBP_MESSAGE_H

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <lbp/beio.h>
#include <lbp/checksum.h>
#include <lbp/payload.h>
#include <lbp/spec.h>

namespace lbp {
/**
 * @brief The Message class. A utility class which helps users construct correctly formed LBP messages for transit.
 *
 * Allocated on the stack, not the heap, so this class is defined with a template argument for maximum capacity.
 * All messages will either be commands with relatively small argument payloads, or giant file-chunks for bulk commands.
 * Thus, two hard-coded capacities are defined below as aliases: MaxMsg and CmdMsg.
 *
 * Usage:
 *
 * 1. Construct a message with a known command code with space for all arguments.
 *
 * CmdMsg jogxy(cmd_move_xy_abs, 8);
 *
 * 2. Append arguments.
 *
 * jogxy.writeInt(10000); // x jog target in um
 * jogxy.writeInt(12000); // y jog target in um
 *
 * The checksum will be automatically calculated and written once all expected arguments are appended.
 *
 * Additionally, two convenience constructors are provided:
 * One with just a command code (arguably the most commonly sent message),
 * and another for commands with a single argument.
 */
template <std::size_t Capacity>
class Message
{
public:
	/** Default Constructor, defined for static collections.  */
	Message() = default;

	/**
	 * @brief Construct message with a command code large enough for the required arguments.
	 * @param cmd The command code.
	 * @param arg_size Size of the arguments (in bytes).
	 */
	Message(uint16_t cmd, uint16_t arg_size)
		: m_size(size_start + size_len + size_cmd + arg_size + size_crc)
		, m_cursor(0)
	{
		writeInt(cmd_start);
		writeShort(payloadSize());
		writeShort(cmd);
	}

	/**
	 * @brief Convenience: Construct a complete message with a command code and no arguments. Ready to send.
	 * @param cmd The command code.
	 */
	Message(uint16_t cmd) : Message(cmd, 0)
	{
		// Empty
	}


	/**
	 * @brief Convenience: Construct a message with a command code and a single argument.
	 *
	 * To avoid confusion, please only use this for messages with exactly one argument of exactly 1, 2, or 4 bytes.
	 *
	 * @param cmd The command code.
	 * @param arg_size Size of the argument (in bytes).
	 * @param arg The first (and preferably only) argument.
	 */
	Message(uint16_t cmd, uint16_t arg_size, int32_t arg)
		: Message(cmd, arg_size)
	{
		if (arg_size == 1) {
			writeByte((uint8_t) arg);
		} else if (arg_size <= 3) {
			writeShort((uint16_t) arg);
		} else if (arg_size >= 4) {
			writeInt(arg);
		}
	}

	/** Copy Constructor */
	Message(const Message &other)
		: m_size(other.m_size)
		, m_cursor(other.m_cursor)
	{
		memcpy(m_data, other.m_data, other.m_size);
	}

	/** Move Constructor */
	Message(Message &&other) noexcept
		: m_size(other.m_size)
		, m_cursor(other.m_cursor)
	{
		memcpy(m_data, other.m_data, other.m_size);
		other.m_size = 0;
		other.m_cursor = 0;
	}

	/** Copy assignment */
	Message &operator=(Message &other) noexcept
	{
		if (this != &other) {
			m_size = other.m_size;
			m_cursor = other.m_cursor;
			memcpy(m_data, other.m_data, other.m_size);
		}
		return *this;
	}

	/** Move assignment */
	Message &operator=(Message &&other) noexcept
	{
		if (this != &other) {
			m_size = other.m_size;
			m_cursor = other.m_cursor;
			memcpy(m_data, other.m_data, other.m_size);
			other.m_size = 0;
			other.m_cursor = 0;
		}
		return *this;
	}

	/** @return The total size of the raw message data (including headers and checksum). */
	uint16_t size() const { return m_size; }

	/** @return the command code included in this message. */
	uint16_t cmd() const
	{
		return readBE16(m_data + size_start + size_len);
	}

	/** @return the checksum from the end of this message. */
	uint16_t checksum() const
	{
		return readBE16(m_data + (m_size - size_crc));
	}

	/** @return the entire raw data from the message. */
	const uint8_t *data() const { return m_data; }

	/** @return the size of the actual payload (command and argument) portion of this message. */
	uint16_t payloadSize() const
	{
		return m_size - size_header_footer;
	}

	/** @return a pointer to the start of the payload (command and argument) portion of this message. */
	const uint8_t *payloadData() const
	{
		return m_data + size_payload_offset;
	}

	/** @return true if we can write the specified number of bytes to the message, false otherwise. */
	bool canWrite(uint16_t num_bytes) const
	{
		return (m_cursor + num_bytes) <= (m_size - size_crc);
	}

	/** @return true if the message can no longer accept more bytes. */
	bool full() const { return m_cursor >= (m_size - size_crc); }

	/**
	 * Append an 8-bit value into the args portion of the message.
	 * @param value The 8-bit value to write
	 * @return true if 8 bits can fit in the message, false otherwise.
	 */
	bool writeByte(uint8_t value)
	{
		if (!canWrite(1)) {
			return false;
		}

		writeBE8(value, m_data + m_cursor);
		m_cursor += 1;

		if (full()) {
			setChecksum();
		}
		return true;
	}

	/**
	 * Append a 16-bit value into the args portion of the message.
	 * @param value The 16-bit value to write
	 * @return true if 16 bits can fit in the message, false otherwise.
	 */
	bool writeShort(uint16_t value)
	{
		if (!canWrite(2)) {
			return false;
		}

		writeBE16(value, m_data + m_cursor);
		m_cursor += 2;

		if (full()) {
			setChecksum();
		}
		return true;
	}

	/**
	 * Append a 32-bit value into the args portion of the message.
	 * @param value The 32-bit value to write
	 * @return true if 32 bits can fit in the message, false otherwise.
	 */
	bool writeInt(uint32_t value)
	{
		if (!canWrite(4)) {
			return false;
		}

		writeBE32(value, m_data + m_cursor);
		m_cursor += 4;

		if (full()) {
			setChecksum();
		}
		return true;
	}

	/**
	 * Append data to the args portion of the message.
	 * @param data The data to be written.
	 * @param num_bytes The number of bytes to be written.
	 * @return True if num_bytes could fit in the message, false otherwise.
	 */
	bool writeData(uint8_t *data, uint16_t num_bytes)
	{
		if (!canWrite(num_bytes)) {
			return false;
		}

		memcpy(m_data + m_cursor, data, num_bytes);
		m_cursor += num_bytes;

		if (full()) {
			setChecksum();
		}
		return true;
	}

private:
	void setChecksum()
	{
		uint16_t crc_offset = m_size - size_crc;
		uint16_t checksum = crc16(payloadData(), payloadSize());
		m_data[crc_offset + 0] = (uint8_t) (checksum >> 8);
		m_data[crc_offset + 1] = (uint8_t) (checksum >> 0);
	}

	uint8_t m_data[Capacity];	// Stack-allocated data buffer.
	uint16_t m_size = 0;	// The size, in bytes, of m_data.
	uint16_t m_cursor = 0;	// offset for writing data into the message.
};

using CmdMsg = Message<size_max_cmd_msg>;
using MaxMsg = Message<size_file_msg>;

} // namespace lbp

#endif // LBP_MESSAGE_H
