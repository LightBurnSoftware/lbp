// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#ifndef LBP_BYTEBUFFER_H
#define LBP_BYTEBUFFER_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace lbp {

/**
 * @brief A simple Ring Buffer designed to be used by a single producer and a single consumer on the same thread.
 *
 * Templated by total capacity, which MUST be a power of two.
 * The queue is designed to be consumed by a message parser, and thus has the ability to
 * "rewind" in the case of a misaligned message.
 */
template <size_t Capacity>
class RingBuffer
{
	static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");
public:
	RingBuffer() = default;

	/** @return the number of bytes available to be read. */
	size_t available() const { return m_head - m_tail; }

	/** @return the number of bytes available to be written. */
	size_t freeSpace() const { return Capacity - (m_head - reserved()); }

	/**
	 * @brief Read data from the buffer.
	 * @param dst Destination buffer.
	 * @param len Number of bytes to read.
	 * @return true on success. Return false and read nothing if len > available().
	 */
	[[nodiscard]] bool read(uint8_t *dst, size_t len)
	{
		if (len > available()) return false;

		size_t pos = m_tail & kModMask;
		size_t first_chunk = std::min(Capacity - pos, len);

		std::memcpy(dst, m_data + pos, first_chunk);

		if (first_chunk < len) {
			std::memcpy(dst + first_chunk, m_data, len - first_chunk);
		}

		m_tail += len;

		return true;
	}

	/**
	 * @brief Read a single byte from the buffer.
	 * @param out Destination byte.
	 * @return true on success. Return false if empty().
	 */
	[[nodiscard]] bool readByte(uint8_t &out)
	{
		if (empty()) return false;
		size_t pos = m_tail & kModMask;
		out = m_data[pos];
		m_tail += 1;
		return true;
	}

	/**
	 * @brief Write data to the buffer.
	 * @param src Source data to write.
	 * @param len Number of bytes to write.
	 * @return true on success. Return false write nothing if len > freeSpace().
	 */
	[[nodiscard]] bool write(const uint8_t *src, size_t len)
	{
		if (len > freeSpace()) return false;

		size_t pos = m_head & kModMask;
		size_t first_chunk = std::min(Capacity - pos, len);

		std::memcpy(m_data + pos, src, first_chunk);
		if (first_chunk < len) {
			std::memcpy(m_data, src + first_chunk, len - first_chunk);
		}

		m_head += len;

		return true;
	}

	/** @return true if no bytes exist to be read. */
	bool empty() const { return m_head == m_tail; }

	/** @brief clears the buffer. */
	void clear()
	{
		m_head = 0;
		m_tail = 0;
		m_mark = kNoMark;
	}

	/** Bookmark the current tail in case a rewind is requested */
	void setBookmark() { m_mark = m_tail; }

	/** Release the current bookmark. */
	void clearBookmark() { m_mark = kNoMark; }

	/** Move the tail back to the mark. */
	void rewind()
	{
		if (m_mark != kNoMark) m_tail = m_mark;
		clearBookmark();
	}


private:
	static constexpr size_t kModMask = Capacity - 1;
	static constexpr size_t kNoMark = SIZE_MAX;

	size_t reserved() const { return m_mark == kNoMark ? m_tail : m_mark; }

	uint8_t m_data[Capacity]; // Actual data.
	size_t m_head = 0;		// Write index
	size_t m_tail = 0;		// Read index
	size_t m_mark = kNoMark; // A sentinel value to indicate whether a rewind mark is present.
};

} // namespace lbp

#endif // LBP_BYTEBUFFER_H
