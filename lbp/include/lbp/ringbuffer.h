// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#ifndef LBP_BYTEBUFFER_H
#define LBP_BYTEBUFFER_H

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace lbp {

/**
 * @brief A simple Ring Buffer designed to be used by a single producer and a single consumer on the same thread.
 *
 * Templated by total capacity, which MUST be a power of two.
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

		size_t pos = m_tail & kModMask;
		size_t first_chunk = std::min(Capacity - pos, len);

		std::memcpy(dst, m_data + pos, first_chunk);

		if (first_chunk < len) {
			std::memcpy(dst + first_chunk, m_data, len - first_chunk);
		}

		m_tail += len;

		return true;
	}

	/** @return A single byte from the buffer, 0x0 if Empty. */
	uint8_t readByte()
	{
		if (empty()) {
			return 0x0;
		}

		size_t pos = m_tail & kModMask;
		m_tail += 1;
		return m_data[pos];
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
	}

private:
	static constexpr size_t kModMask = Capacity - 1;

	uint8_t m_data[Capacity]; // Actual data.
	size_t m_head = 0;		// Write index
	size_t m_tail = 0;		// Read index
};

} // namespace lbp

#endif // LBP_BYTEBUFFER_H
