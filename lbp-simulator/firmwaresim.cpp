// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include "firmwaresim.h"

#include "log.h"
#include <lbp/message.h>
#include <lbp/payload.h>
#include <lbp/spec.h>

FirmwareSim::FirmwareSim()
	: m_movement(m_config)
{
	gLog().push(Log::INFO, "Started sim");
	m_config.load();
}

void FirmwareSim::rxCallback(const uint8_t *bytes, size_t len)
{
	qint64 elapsed = m_profile.restart();
	qDebug() << "rx callback :" << len << elapsed;
	m_parser.feed(bytes, len);
	// Process commands from transport
	while (m_parser.parseNext()) {
		process(m_parser.payload());
	}
	elapsed = m_profile.restart();
	qDebug() << "processed in " << elapsed;

	tx();
	elapsed = m_profile.restart();
	qDebug() << "tx in" << elapsed;
}

SimState FirmwareSim::loop(int ms)
{
	SimState val = update(ms);
	tx();
	return val;
}

void FirmwareSim::tx()
{
	// send output packets
	if (m_transport) {
		while (!m_out_q.empty()) {
			lbp::CmdMsg p = m_out_q.pop();
			m_transport->sendBytes(p.data(), p.size());
		}
	}
}

void FirmwareSim::setTransport(Transport *conn)
{
	m_transport = conn;
}

bool FirmwareSim::process(lbp::MaxPayload &payload)
{
	// Try to handle this request with one of the components.
	if (m_filesystem.process(payload, m_out_q)) {
		return true;
	}
	if (m_config.process(payload, m_out_q)) {
		return true;
	}
	if (m_movement.process(payload, m_out_q)) {
		return true;
	}

	// No? Then perhaps this is a top-level request.
	const uint16_t cmd = payload.cmd();

	switch (cmd) {
	case lbp::cmd_commit_cfg:
		m_config.commit();
		gLog().push(Log::INFO, "Config Commit");
		m_out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_handshake:
		gLog().push(Log::DEBUG, "Handshake detected. Shaking Back :)");
		m_out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_stop:
		gLog().push(Log::INFO, "Stop command received.");
		m_movement.stop();
		m_filesystem.stop();
		m_fw_state &= ~lbp::state_executing_job;
		m_fw_state &= ~lbp::state_executing_frame;
		m_out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_pause:
		gLog().push(Log::INFO, "Pause command received.");
		m_movement.pause();
		m_out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_continue:
		gLog().push(Log::INFO, "Resume command received.");
		m_movement.resume();
		m_out_q.push(lbp::CmdMsg(cmd));
		return true;
	case lbp::cmd_get_state:
		m_out_q.push(lbp::CmdMsg(cmd, 4, m_fw_state));
		return true;
	case lbp::cmd_execute: {
		gLog().push(Log::INFO, "Executing file!");
		m_movement.stop();
		m_filesystem.reset();
		m_filesystem.startReading();
		m_fw_state |= lbp::state_executing_job;
		m_out_q.push(lbp::CmdMsg(cmd));
		return true;
	}
	default:
		gLog().push(Log::WARNING,
					QString("Encountered unimplemented cmd %1. Sending default response.").arg(cmd));
		m_out_q.push(lbp::CmdMsg(cmd));
		break;
	}
	return false;
}

SimState FirmwareSim::update(int ms)
{
	m_movement.update(ms);

	// Process commands from the job, if applicable
	if (m_fw_state & lbp::state_executing_job) {
		auto &fparser = m_filesystem.parser();

		if (m_movement.canEnqueue()) {
			if (fparser.parseNext()) {
				m_movement.process(fparser.payload(), m_out_q);
			} else {
				m_filesystem.feedParser();
			}
		}

		while (m_movement.canEnqueue() && fparser.parseNext()) {
			m_movement.process(fparser.payload(), m_out_q);
		}
	}

	m_fw_state = m_filesystem.getFwState(m_fw_state);
	m_fw_state = m_movement.getFwState(m_fw_state);

	// collate simulation state for caller
	return m_movement.getSimState();
}
