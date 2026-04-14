// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#ifndef LBP_QUEUE_H
#define LBP_QUEUE_H

#include <cassert>
#include <cstddef>
#include <utility>

namespace lbp {

/**
 * @brief A simple FIFO queue with templated maximum capacity which MUST be a power of two.
 * Example: An output queue of outgoing messages:
 * Queue<CmdMessage, 256> out_q;
 *
 * Example: A queue of scheduled movement commands:
 * Queue<CmdPayload, 64> cmd_q;
 *
 * An empty queue will still return a default-constructed item, so proper usage
 * should check before popping:
 *
 * while(!cmd_q.empty()) {
 *     CmdPayload cp = cmd_q.pop();
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
	 * @brief Push an item to the back of the queue.
	 * @param item The item to push.
	 * @return True if there was room for the item in the queue, false otherwise.
	 */
	bool push(T item)
	{
		if (!freeSpace()) {
			return false;
		}
		size_t pos = m_head & kModMask;

		m_head += 1;

		m_data[pos] = item;

		return true;
	}

	/**
	 * @brief Remove and return the first item from the front of the queue.
	 * @return The item at the front of the queue, or a default-constructed item if the queue was empty.
	 */
	T pop() {
		if (!size()) {
			return T();
		}

		size_t pos = m_tail & kModMask;

		m_tail += 1;
		return std::move(m_data[pos]);
	}

	/** @return the number of items currently queued. */
	size_t size() const { return m_head - m_tail; }

	/** @return the number of items that can be queued before overflowing. */
	size_t freeSpace() const { return Capacity - size(); }

	/** @return true if no items exist in the queue. */
	bool empty() const { return m_head == m_tail; }

private:
	static constexpr size_t kModMask = Capacity - 1;

	T m_data[Capacity]; // array-backed queue data.
	size_t m_head = 0;  // write index
	size_t m_tail = 0;  // read index
};

} // namespace lbp

#endif // LBP_QUEUE_H
