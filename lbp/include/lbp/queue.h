// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#ifndef LBP_QUEUE_H
#define LBP_QUEUE_H

#include <cstddef>
#include <utility>

namespace lbp {

/**
 * @brief A simple FIFO queue with templated maximum capacity which MUST be a power of two.
 * Example: An output queue of outgoing messages:
 * Queue<CmdMsg, 256> out_q;
 *
 * Example: A queue of scheduled movement commands:
 * Queue<CmdPayload, 64> cmd_q;
 *
 * CmdPayload cp;
 * while(cmd_q.pop(cp)) {
 *     process(cp); // your custom processing function.
 * }
 */
template <typename T, size_t Capacity>
class Queue
{
	static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");
public:
	Queue() = default;

	/**
	 * @brief Push a copy of an item to the back of the queue.
	 * @param item The item to copy into the queue.
	 * @return True if there was room for the item, false if the queue was full.
	 */
	[[nodiscard]] bool push(const T &item)
	{
		if (!freeSpace()) return false;
		m_data[m_head & kModMask] = item;
		m_head += 1;
		return true;
	}

	/**
	 * @brief Move an item into the back of the queue.
	 * @param item The item to move into the queue.
	 * @return True if there was room for the item, false if the queue was full.
	 */
	[[nodiscard]] bool push(T &&item)
	{
		if (!freeSpace()) return false;
		m_data[m_head & kModMask] = std::move(item);
		m_head += 1;
		return true;
	}

	/**
	 * @brief Remove the item at the front of the queue.
	 * @param out Reference parameter to receive the item.
	 * @return True if an item was moved into out. False if the queue was empty, in which case out is unchanged.
	 */
	[[nodiscard]] bool pop(T &out)
	{
		if (empty()) return false;
		out = std::move(m_data[m_tail & kModMask]);
		m_tail += 1;
		return true;
	}

	/** @return the number of items currently queued. */
	size_t size() const { return m_head - m_tail; }

	/** @return the number of items that can be queued before overflowing. */
	size_t freeSpace() const { return Capacity - size(); }

	/** @return true if no items exist in the queue. */
	bool empty() const { return m_head == m_tail; }

	/** Clear the queue. */
	void clear() { m_head = m_tail = 0; }

private:
	static constexpr size_t kModMask = Capacity - 1;

	T m_data[Capacity]; // array-backed queue data.
	size_t m_head = 0;  // write index
	size_t m_tail = 0;  // read index
};

} // namespace lbp

#endif // LBP_QUEUE_H
