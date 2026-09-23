// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include "simutils.h"

#include "log.h"

void badPayload(const lbp::CmdPayload &p)
{
	gLog().error(QString("Bad Payload detected: %1 of size %2").arg(p.cmd()).arg(p.size()));
}

void OutputQueue::push(lbp::CmdMsg msg)
{
	if (!m_q.push(msg)) {
		gLog().error(QString("output queue full, dropped response %1")
						 .arg(msg.cmd(), 0, 16));
		m_overflowed = true;
	}
}

bool OutputQueue::takeOverflow()
{
	const bool f = m_overflowed;
	m_overflowed = false;
	return f;
}

int32_t toWire(int64_t v)
{
	if (v > INT32_MAX || v < INT32_MIN) {
		gLog().error(QString("coordinate %1 exceeds protocol range").arg(v));
		return v > 0 ? INT32_MAX : INT32_MIN;
	}
	return static_cast<int32_t>(v);
}
