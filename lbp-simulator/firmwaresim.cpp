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
	updateFwState(); // may end the job (endJob sends the final progress report)
	if (m_fw_state & lbp::state_executing_job) {
		reportProgress(dt_ns);
	}
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
	if (table.field_size_um != m_config.getOrDefault(lbp::cfg_field_size)) {
		gLog().push(Log::WARNING, "Correction table field size doesn't match cfg_field_size");
		m_out_q.nack(payload, lbp::err_file_mismatch, lbp::lbpcor_field_size_offset);
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
			m_movement.process(fparser.payload(), m_out_q, CmdSource::File, uint32_t(fparser.messageOffset()));
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
		endJob(lbp::err_fault); // a running job ends with err_fault (before stop(), which would report err_canceled)
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
		endJob(0);
		return;
	}

	// No terminator: Maybe the job file was malformed.
	// wait until the file is exhausted AND the movement queue
	// has drained, so we never report completion while the head is still moving.
	if (!m_filesystem.isReading() && m_movement.isIdle()) {
		gLog().warning("job file ended without cmd_job_end");
		endJob(0);
	}
}

void FirmwareSim::endJob(uint16_t result)
{
	if (!(m_fw_state & lbp::state_executing_job)) {
		return;
	}
	// Where it got to: the whole file if it ran to the end, else the command that was running (read this before
	// anything resets the movement simulator).
	const uint32_t progress = result == 0 ? m_job_size : m_movement.executedBytes();
	m_fw_state &= ~lbp::state_executing_job;
	m_filesystem.stop();
	// The final progress report comes first, then how the job ended (spec.md, "Job results").
	m_out_q.event(lbp::event_job_progress, lbp::progress_bytes_executed, progress);
	m_out_q.event(lbp::event_result, result, m_job_txid);
	gLog().push(Log::INFO, result == 0 ? QString("Job complete")
									   : QString("Job ended early, error 0x%1").arg(result, 4, 16, QLatin1Char('0')));
}

void FirmwareSim::beginJob(uint16_t txid)
{
	endJob(lbp::err_canceled); // a job still running is replaced: it ends canceled
	m_movement.stop();
	m_filesystem.reset();
	if (m_filesystem.startReading()) {
		m_fw_state |= lbp::state_executing_job;
		m_job_size = uint32_t(m_filesystem.jobFileSize());
		m_job_txid = txid;
		m_last_progress = UINT32_MAX;
		m_since_progress_ns = 0;
	}
}

void FirmwareSim::reportProgress(int64_t dt_ns)
{
	constexpr int64_t kMinIntervalNs = 250'000'000;	// at most every 250 ms, and only on change
	constexpr int64_t kMaxIntervalNs = 2'000'000'000;	// at least every 2 s
	m_since_progress_ns += dt_ns;
	const uint32_t bytes = m_movement.executedBytes();
	const bool changed = bytes != m_last_progress;
	if ((changed && m_since_progress_ns >= kMinIntervalNs) || m_since_progress_ns >= kMaxIntervalNs) {
		m_out_q.event(lbp::event_job_progress, lbp::progress_bytes_executed, bytes);
		m_last_progress = bytes;
		m_since_progress_ns = 0;
	}
}

void FirmwareSim::stop()
{
	endJob(lbp::err_canceled); // a running job ends canceled (no-op if a fault already ended it)
	m_movement.stop();
	m_filesystem.stop();
	m_fw_state &= ~lbp::state_executing_job;
	m_fw_state &= ~lbp::state_executing_frame;
	// Refresh the movement bits now, so a cmd_get_state answered before the next tick doesn't report moving.
	m_fw_state = m_movement.getFwState(m_fw_state);
}

bool FirmwareSim::process(lbp::MaxPayload &payload)
{
	const uint16_t txid = payload.txid();

	// LightBurn never sends txid 0. One arriving here is noise, or a job-file frame from inside a cmd_file_chunk that
	// the parser found after losing sync: running it would execute a job command live. Ignore it (spec.md, "Rules for
	// the Firmware"). Job files don't come through here; pumpJob feeds them to the movement simulator directly.
	if (txid == lbp::txid_none) {
		gLog().push(Log::WARNING, QString("Ignored a message with txid 0 (command 0x%1)").arg(payload.cmd(), 4, 16, QLatin1Char('0')));
		return true;
	}

	// A repeated transaction id with the same command is a retry: answer again without repeating side effects.
	// (The cached response reuses the request's command code, so comparing it compares the commands.)
	if (txid != lbp::txid_none && txid == m_last_txid) {
		lbp::CmdMsg prev;
		if (m_out_q.lastResponse(txid, prev) && prev.cmd() == payload.cmd()) {
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
		const int32_t field = m_config.getOrDefault(lbp::cfg_field_size);
		m_config.commit();
		gLog().push(Log::INFO, "Config Commit");
		if (field != m_config.getOrDefault(lbp::cfg_field_size)) {
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
		m_out_q.respond(payload); // the response comes before the job's eventual event_result
		beginJob(payload.txid());
		return true;
	}
	default:
		m_out_q.nack(payload, lbp::err_invalid_cmd);
		break;
	}
	return false;
}
