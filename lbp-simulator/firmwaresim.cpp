// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include "firmwaresim.h"

#include "log.h"
#include <lbp/message.h>
#include <lbp/payload.h>
#include <lbp/spec.h>

static void tx_nop(const uint8_t *bytes, size_t len)
{
	(void)bytes;
	(void)len;
}

FirmwareSim::FirmwareSim()
	: m_movement(m_config)
	, m_tx_callback(&tx_nop)
{
	gLog().push(Log::INFO, "Started sim");
	m_config.load();
}

void FirmwareSim::rxCallback(const uint8_t *bytes, size_t len)
{
	size_t offset = 0;
	while (offset < len) {
		const size_t fed = m_parser.feed(bytes + offset, len - offset);
		offset += fed;

		// Drain what we just fed, freeing space for the next pass.
		while (m_parser.parseNext()) {
			process(m_parser.payload());
		}

		if (fed == 0) {
			gLog().error("rx: parser cannot consume, dropping remaining bytes");
			break;
		}
	}
	tx();
}

SimState FirmwareSim::step()
{
	pumpJob(); // no-op unless a job is executing.
	m_movement.step(); // step movement simulation.
	updateFwState();
	tx(); // send output packets.
	return m_movement.getSimState();
}

void FirmwareSim::tx()
{
	lbp::CmdMsg m;
	while (m_out_q.pop(m)) {
		m_tx_callback(m.data(), m.size());
	}
}

void FirmwareSim::pumpJob()
{
	if (!(m_fw_state & lbp::state_executing_job)) {
		return;
	}

	auto &fparser = m_filesystem.parser();

	while (m_movement.canEnqueue()) {
		if (fparser.parseNext()) {
			m_movement.process(fparser.payload(), m_out_q, CmdSource::Wire);
		} else if (m_filesystem.feedParser() == 0) {
			gLog().warning(QString("pumpJob stalled: reading=%1 idle=%2 parserFree=%3")
							   .arg(m_filesystem.isReading())
							   .arg(m_movement.isIdle())
							   .arg(fparser.freeSpace()));
			break;
		}
	}
}

void FirmwareSim::updateFwState()
{
	m_fw_state = m_filesystem.getFwState(m_fw_state);
	m_fw_state = m_movement.getFwState(m_fw_state);

	if (m_fw_state & lbp::state_executing_job) {
		checkJobComplete();
	}
}

void FirmwareSim::checkJobComplete()
{
	if (m_movement.takeJobEnded()) {
		endJob();
		return;
	}

	// No terminator: Maybe the job file was malformed.
	// wait until the file is exhausted AND the movement queue
	// has drained, so we never report completion while the head is still moving.
	if (!m_filesystem.isReading() && m_movement.isIdle()) {
		gLog().warning("job file ended without cmd_job_end");
		endJob();
	}
}

void FirmwareSim::endJob()
{
	m_fw_state &= ~lbp::state_executing_job;
	m_filesystem.stop();
	// TODO: perhaps a different command for notification?
	m_out_q.push(lbp::CmdMsg(lbp::cmd_job_end));
	gLog().push(Log::INFO, "Job complete");
}

void FirmwareSim::beginJob()
{
	m_movement.stop();
	m_filesystem.reset();
	if (m_filesystem.startReading()) {
		m_fw_state |= lbp::state_executing_job;
	}
}

void FirmwareSim::stop()
{
	m_movement.stop();
	m_filesystem.stop();
	m_fw_state &= ~lbp::state_executing_job;
	m_fw_state &= ~lbp::state_executing_frame;
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
	if (m_movement.process(payload, m_out_q, CmdSource::Wire)) {
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
		stop();
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
		beginJob();
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
