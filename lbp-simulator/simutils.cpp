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
	if (msg.txid() != lbp::txid_none) {
		m_last_response = msg;
	}
	if (!m_q.push(msg)) {
		gLog().error(QString("output queue full, dropped response %1")
						 .arg(msg.cmd(), 0, 16));
		m_overflowed = true;
	}
}

void OutputQueue::respond(const lbp::MaxPayload &request)
{
	push(lbp::CmdMsg::responseTo(request));
}

void OutputQueue::respond(const lbp::MaxPayload &request, int32_t value)
{
	lbp::CmdMsg m = lbp::CmdMsg::responseTo(request, 4);
	m.writeInt(static_cast<uint32_t>(value));
	push(m);
}

bool OutputQueue::lastResponse(uint16_t txid, lbp::CmdMsg &out) const
{
	if (txid == lbp::txid_none || m_last_response.txid() != txid) {
		return false;
	}
	out = m_last_response;
	return true;
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
