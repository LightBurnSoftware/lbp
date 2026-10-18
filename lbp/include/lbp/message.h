// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#ifndef LBP_MESSAGE_H
#define LBP_MESSAGE_H

#include <algorithm>
#include <cassert>
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
 * CmdMsg jogxy(cmd_jog_to_xy, 8);
 *
 * 2. Append arguments.
 *
 * jogxy.writeInt(10000);  // x target in um
 * jogxy.writeInt(12000);  // y target in um
 *
 * The checksum will be automatically calculated and written once all expected arguments are appended.
 * The complete() function can be used to confirm that a message is full, checksumed, and ready to send.
 *
 * if (jogxy.complete()) { send(jogxy.data(), jogxy.size()); }
 *
 * NOTE: Messages should be declared only as large as they need to be. But if for some reason
 *       an underfilled message is desired, finalize() can be called to zero-fill the remaining
 *       argument space and apply the the checksum. It will also check for dimensional correctness.
 *
 * if (jogxy.finalize()) { send(jogxy.data(), jogxy.size()); }
 *
 * Additionally, two convenience constructors are provided:
 * One with just a command code (arguably the most commonly sent message),
 * and another for commands with a single argument.
 *
 * Transaction IDs default to txid_none. Senders that track requests stamp one with setTxid(),
 * which may be called at any time (e.g. just before sending) and keeps the checksum valid.
 * Responses should be built with responseTo(), which copies the request's transaction id:
 *
 * auto reply = CmdMsg::responseTo(request, 4);
 * reply.writeInt(state);
 */
template <std::size_t Capacity>
class Message
{
public:
	static_assert(Capacity >= size_min_msg, "Message capacity must hold a minimum-sized message");
	static_assert(Capacity <= UINT16_MAX,   "Message capacity must be representable in uint16_t");

	/** Default Constructor, defined for static collections.  */
	Message() = default;

	/**
	 * @brief Construct message with a command code large enough for the required arguments.
	 * @param cmd The command code.
	 * @param arg_size Size of the arguments (in bytes).
	 */
	explicit Message(uint16_t cmd, uint16_t arg_size)
		: m_size(static_cast<uint16_t>(std::min<size_t>(size_min_msg + size_t(arg_size), Capacity)))
		, m_cursor(0)
	{
		assert((size_min_msg + size_t(arg_size)) <= Capacity && "Total message size cannot exceed capacity");
		// Cannot fail, the constructor and static limits on Capacity guarantee room to write.
		writeInt(magic_start);
		writeShort(txid_none);
		writeShort(payloadSize());
		writeShort(cmd);
	}

	/**
	 * @brief Convenience: Construct a complete message with a command code and no arguments. Ready to send.
	 * @param cmd The command code.
	 */
	explicit Message(uint16_t cmd) : Message(cmd, 0)
	{
		// Empty
	}

	/**
	 * @brief Convenience: Construct a message with a command code and a single argument.
	 * Can only be used with arg_size == 1, 2, or 4.
	 *
	 * @param cmd The command code.
	 * @param arg_size Size of the argument (in bytes).
	 * @param arg The first and only argument.
	 */
	explicit Message(uint16_t cmd, uint16_t arg_size, int32_t arg)
		: Message(cmd, arg_size)
	{
		assert((arg_size == 1 || arg_size == 2 || arg_size == 4)
				&& "single-arg constructor requires arg_size of exactly 1, 2, or 4");
		if (arg_size == 1) {
			writeByte(static_cast<uint8_t>(arg));
		} else if (arg_size == 2) {
			writeShort(static_cast<uint16_t>(arg));
		} else if (arg_size == 4) {
			writeInt(static_cast<uint32_t>(arg));
		}
	}

	/**
	 * @brief Construct a response to a received request: same command code and transaction id.
	 * @param request The payload of the request being answered.
	 * @param arg_size Size of the response arguments (in bytes).
	 */
	template <size_t PayloadCapacity>
	static Message responseTo(const Payload<PayloadCapacity> &request, uint16_t arg_size = 0)
	{
		Message m(request.cmd(), arg_size);
		m.setTxid(request.txid());
		return m;
	}

	/** Copy Constructor */
	Message(const Message &other)
		: m_size(other.m_size)
		, m_cursor(other.m_cursor)
	{
		std::memcpy(m_data, other.m_data, other.m_size);
	}

	/** Move Constructor */
	Message(Message &&other) noexcept
		: m_size(other.m_size)
		, m_cursor(other.m_cursor)
	{
		std::memcpy(m_data, other.m_data, other.m_size);
		other.m_size = 0;
		other.m_cursor = 0;
	}

	/** Copy assignment */
	Message &operator=(const Message &other) noexcept
	{
		if (this != &other) {
			m_size = other.m_size;
			m_cursor = other.m_cursor;
			std::memcpy(m_data, other.m_data, other.m_size);
		}
		return *this;
	}

	/** Move assignment */
	Message &operator=(Message &&other) noexcept
	{
		if (this != &other) {
			m_size = other.m_size;
			m_cursor = other.m_cursor;
			std::memcpy(m_data, other.m_data, other.m_size);
			other.m_size = 0;
			other.m_cursor = 0;
		}
		return *this;
	}

	/** @return The total size of the raw message data (including headers and checksum). */
	uint16_t size() const { return m_size; }

	/** @return the command code included in this message. */
	uint16_t cmd() const { return readBE16(m_data + size_payload_offset); }

	/** @return the transaction id included in this message. */
	uint16_t txid() const { return m_size >= size_min_msg ? readBE16(m_data + size_txid_offset) : txid_none; }

	/**
	 * @brief Set the transaction id. If the message is already complete, the checksum is recomputed.
	 * @return False if the message was never properly constructed.
	 */
	bool setTxid(uint16_t txid)
	{
		if (m_size < size_min_msg) return false;
		writeBE16(txid, m_data + size_txid_offset);
		if (full()) setChecksum();
		return true;
	}

	/** @return the checksum from the end of this message. */
	uint16_t checksum() const { return m_size >= size_min_msg ? readBE16(m_data + (m_size - size_crc)) : 0; }

	/** @return the entire raw data from the message. */
	const uint8_t *data() const { return m_data; }

	/** @return the size of the actual payload (command and argument) portion of this message. */
	uint16_t payloadSize() const { return m_size >= size_min_msg ? m_size - size_header_footer : 0; }

	/** @return a pointer to the start of the payload (command and argument) portion of this message. */
	const uint8_t *payloadData() const { return m_data + size_payload_offset; }

	/** @return true if we can write the specified number of bytes to the message, false otherwise. */
	bool canWrite(uint16_t num_bytes) const { return (m_size >= size_min_msg) && (m_cursor + num_bytes) <= (m_size - size_crc); }

	/** @return true if a message is well-formed, complete with checksum, and safe to send. */
	bool complete() const { return m_size >= size_min_msg && full(); }

	/**
	 * Append an 8-bit value into the args portion of the message.
	 * @param value The 8-bit value to write
	 * @return true if 8 bits can fit in the message, false otherwise.
	 */
	bool writeByte(uint8_t value)
	{
		if (!canWrite(1)) return false;

		writeBE8(value, m_data + m_cursor);
		m_cursor += 1;

		if (full()) setChecksum();
		return true;
	}

	/**
	 * Append a 16-bit value into the args portion of the message.
	 * @param value The 16-bit value to write
	 * @return true if 16 bits can fit in the message, false otherwise.
	 */
	bool writeShort(uint16_t value)
	{
		if (!canWrite(2)) return false;

		writeBE16(value, m_data + m_cursor);
		m_cursor += 2;

		if (full()) setChecksum();
		return true;
	}

	/**
	 * Append a 32-bit value into the args portion of the message.
	 * @param value The 32-bit value to write
	 * @return true if 32 bits can fit in the message, false otherwise.
	 */
	bool writeInt(uint32_t value)
	{
		if (!canWrite(4)) return false;

		writeBE32(value, m_data + m_cursor);
		m_cursor += 4;

		if (full()) setChecksum();
		return true;
	}

	/**
	 * Append data to the args portion of the message.
	 * @param data The data to be written.
	 * @param num_bytes The number of bytes to be written.
	 * @return True if num_bytes could fit in the message, false otherwise.
	 */
	bool writeData(const uint8_t *data, uint16_t num_bytes)
	{
		if (!canWrite(num_bytes)) return false;

		std::memcpy(m_data + m_cursor, data, num_bytes);
		m_cursor += num_bytes;

		if (full()) setChecksum();
		return true;
	}

	/**
	 * @brief Zero-fill any unwritten argument bytes and write the checksum.
	 *
	 * Normally the checksum is written automatically once the declared argument
	 * space is exactly filled. Call this to finish a message that was deliberately
	 * left short: the remaining argument bytes are zeroed and the message becomes
	 * complete() and safe to send.
	 *
	 * @return True if the message is complete. False if the message was never
	 *         properly constructed (default-constructed, or capacity exceeded).
	 */
	[[nodiscard]] bool finalize()
	{
		if (m_size < size_min_msg || m_size > Capacity) return false;

		const uint16_t end = m_size - size_crc;
		if (m_cursor < end) {
			std::memset(m_data + m_cursor, 0, end - m_cursor);
			m_cursor = end;
		}
		setChecksum();
		return true;
	}

private:
	/** @return true if the message can no longer accept more bytes. */
	bool full() const { return m_cursor >= (m_size - size_crc); }

	void setChecksum()
	{
		uint16_t crc_offset = m_size - size_crc;
		uint16_t checksum = crc16(m_data + size_crc_offset, crc_offset - size_crc_offset);
		m_data[crc_offset + 0] = (uint8_t) (checksum >> 8);
		m_data[crc_offset + 1] = (uint8_t) (checksum >> 0);
	}

	uint8_t m_data[Capacity] = {};	// Stack-allocated data buffer.
	uint16_t m_size = 0;			// The size, in bytes, of m_data.
	uint16_t m_cursor = 0;			// offset for writing data into the message.
};

using CmdMsg = Message<size_max_cmd_msg>;
using MaxMsg = Message<size_file_msg>;

} // namespace lbp

#endif // LBP_MESSAGE_H
