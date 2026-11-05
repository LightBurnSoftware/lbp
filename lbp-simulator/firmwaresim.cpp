// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include "firmwaresim.h"

#include "log.h"
#include <lbp/message.h>
#include <lbp/payload.h>
#include <lbp/spec.h>
#include <lbp/version.h>

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

SimState FirmwareSim::step(int64_t dt_ns)
{
	pumpJob(); // no-op unless a job is executing.
	m_movement.step(double(dt_ns) / 1e9); // step movement simulation.
	updateFwState();
	tx(); // send output packets.
	SimState state = m_movement.getSimState();
	state.correction_on = m_correction_on;
	return state;
}

void FirmwareSim::setMode(MachineMode mode)
{
	// A full reset: all stop, queues cleared, file transfers and the loaded job dropped.
	stop();
	m_filesystem.reset();
	m_filesystem.setGalvo(mode == MachineMode::Galvo);
	m_correction_on = false;
	m_movement.setMode(mode);
	updateFwState();
}

void FirmwareSim::processCorrection(lbp::MaxPayload &payload)
{
	uint8_t enable = 0;
	if (payload.size() != 1 || !payload.readArg(enable)) {
		m_out_q.nack(payload, lbp::err_invalid_length, 1);
		return;
	}
	if (enable == 0) {
		m_correction_on = false;
		gLog().push(Log::INFO, "Correction off");
		m_out_q.respond(payload);
		return;
	}

	// Applying: the stored table must fit the current configuration.
	const CorrectionTable &table = m_filesystem.correction();
	if (!table.valid) {
		m_out_q.nack(payload, lbp::err_wrong_state, m_fw_state);
		return;
	}
	if (table.field_size_x_um != m_config.getOrDefault(lbp::cfg_field_size_x)) {
		gLog().push(Log::WARNING, "Correction table field size X doesn't match cfg_field_size_x");
		m_out_q.nack(payload, lbp::err_file_mismatch, 16); // header offset of field_size_x
		return;
	}
	if (table.field_size_y_um != m_config.getOrDefault(lbp::cfg_field_size_y)) {
		gLog().push(Log::WARNING, "Correction table field size Y doesn't match cfg_field_size_y");
		m_out_q.nack(payload, lbp::err_file_mismatch, 20); // header offset of field_size_y
		return;
	}
	m_correction_on = true;
	gLog().push(Log::INFO, "Correction on");
	m_out_q.respond(payload);
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
			m_movement.process(fparser.payload(), m_out_q, CmdSource::File);
		} else if (m_filesystem.feedParser() == 0) {
			break;
		}
	}
}

void FirmwareSim::setFault(bool active)
{
	if (active == m_fault) {
		return;
	}
	m_fault = active;
	if (active) {
		gLog().push(Log::WARNING, "Fault: stopping and refusing motion.");
		stop();
	} else {
		gLog().push(Log::INFO, "Fault cleared.");
	}
	updateFwState();
}

void FirmwareSim::updateFwState()
{
	m_fw_state = m_filesystem.getFwState(m_fw_state);
	m_fw_state = m_movement.getFwState(m_fw_state);
	if (m_fault) {
		m_fw_state |= lbp::state_fault;
	} else {
		m_fw_state &= ~lbp::state_fault;
	}

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
	const uint16_t txid = payload.txid();

	// A repeated transaction id is a retry: answer again without repeating side effects.
	if (txid != lbp::txid_none && txid == m_last_txid) {
		lbp::CmdMsg prev;
		if (m_out_q.lastResponse(txid, prev)) {
			gLog().push(Log::INFO, QString("Duplicate txid %1, resending response").arg(txid));
			m_out_q.push(prev);
			return true;
		}
	}
	if (txid != lbp::txid_none) {
		m_last_txid = txid;
	}

	const uint16_t req_cmd = payload.cmd();
	const bool starts_motion = (req_cmd & lbp::mask_class) == lbp::cls_move
		|| req_cmd == lbp::cmd_execute || req_cmd == lbp::cmd_continue;
	if (m_fault && starts_motion) {
		m_out_q.nack(payload, lbp::err_fault);
		return true;
	}

	// Try to handle this request with one of the components.
	if (m_filesystem.process(payload, m_out_q)) {
		if (m_filesystem.takeNewCorrection()) {
			m_correction_on = false; // a new table is stored, not applied
		}
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
	case lbp::cmd_commit_cfg: {
		const int32_t field_x = m_config.getOrDefault(lbp::cfg_field_size_x);
		const int32_t field_y = m_config.getOrDefault(lbp::cfg_field_size_y);
		m_config.commit();
		gLog().push(Log::INFO, "Config Commit");
		if (field_x != m_config.getOrDefault(lbp::cfg_field_size_x)
			|| field_y != m_config.getOrDefault(lbp::cfg_field_size_y)) {
			// The table may no longer fit the field: correction goes off until it's applied again.
			m_correction_on = false;
			m_movement.refreshBounds();
		}
		m_out_q.respond(payload);
		return true;
	}
	case lbp::cmd_galvo_correction:
		if (m_movement.mode() == MachineMode::Galvo) {
			processCorrection(payload);
			return true;
		}
		m_out_q.nack(payload, lbp::err_invalid_cmd);
		return false;
	case lbp::cmd_handshake: {
		uint16_t host_version = 0;
		if (payload.readArg(host_version)) {
			gLog().push(Log::DEBUG, QString("Handshake from host v%1.%2. Shaking back :)")
										.arg(lbp::versionMajor(host_version))
										.arg(lbp::versionMinor(host_version)));
		} else {
			gLog().push(Log::WARNING, "Handshake without a version. Shaking back anyway.");
		}
		lbp::CmdMsg reply = lbp::CmdMsg::responseTo(payload, 2);
		reply.writeShort(lbp::protocol_version);
		m_out_q.push(reply);
		return true;
	}
	case lbp::cmd_stop:
		gLog().push(Log::INFO, "Stop command received.");
		stop();
		m_out_q.respond(payload);
		return true;
	case lbp::cmd_pause:
		gLog().push(Log::INFO, "Pause command received.");
		m_movement.pause();
		m_out_q.respond(payload);
		return true;
	case lbp::cmd_continue:
		gLog().push(Log::INFO, "Resume command received.");
		m_movement.resume();
		m_out_q.respond(payload);
		return true;
	case lbp::cmd_get_state:
		m_out_q.respond(payload, static_cast<int32_t>(m_fw_state));
		return true;
	case lbp::cmd_execute: {
		gLog().push(Log::INFO, "Executing file!");
		beginJob();
		m_out_q.respond(payload);
		return true;
	}
	default:
		m_out_q.nack(payload, lbp::err_invalid_cmd);
		break;
	}
	return false;
}
