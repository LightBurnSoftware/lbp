// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include <lbp/message.h>
#include <lbp/parser.h>
#include <lbp/queue.h>

/** Identifies if a command comes from a job file or transport. */
enum class CmdSource { File, Wire };

using CmdQueue = lbp::Queue<lbp::CmdPayload, 512>;
using WireParser = lbp::Parser<4096>;
using FileParser = lbp::Parser<4096>;

constexpr int64_t sim_step_us = 500;
constexpr int64_t sim_step_ns = sim_step_us * 1'000;

void badPayload(const lbp::CmdPayload &p);

/**
 * @brief Output queue with drop detection.
 *
 * Sending can fail if the consumer isn't draining. There's no useful recovery
 * at the call site, so failures are logged and latched here; the flag is
 * reported in fw_state and cleared when read.
 */
class OutputQueue
{
public:
	void push(lbp::CmdMsg msg);
	bool pop(lbp::CmdMsg &out) { return m_q.pop(out); }
	bool empty() const { return m_q.empty(); }
	/** @return true if any message has been dropped since the last call. */
	bool takeOverflow();

private:
	lbp::Queue<lbp::CmdMsg, 512> m_q;
	bool m_overflowed = false;
};

/** Downcast 64-bit positions to 32 for transport. */
int32_t toWire(int64_t v);
