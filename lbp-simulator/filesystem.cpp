// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include "filesystem.h"
#include "log.h"

#include <lbp/spec.h>
#include <QDir>
#include <QStandardPaths>

using namespace lbp;

static QString getTmpFilename()
{
	return QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
		.filePath("tmp.oz");
}

bool FileSystem::process(MaxPayload &request, OutputQueue &out_q)
{
	const uint16_t cmd = request.cmd();

	switch (cmd) {
	case cmd_file_begin: {
		gLog().push(Log::INFO, "BOF~~~~~~");
		if (isReading()) {
			out_q.nack(request, err_invalid_state, state_executing_job);
			return true;
		}
		reset();
		const uint16_t err = startReceiving(request);
		if (err) {
			out_q.nack(request, err, err == err_bad_length ? 4 : 0);
		} else {
			out_q.respond(request);
		}
		return true;
	}
	case cmd_file_end:
		gLog().push(Log::INFO, "EOF~~~~~~");
		if (m_state != State::Receiving) {
			out_q.nack(request, err_transfer);
			return true;
		}
		stopReceiving();
		out_q.respond(request);
		return true;
	case cmd_file_chunk: {
		const uint16_t err = receive(request);
		if (err) {
			out_q.nack(request, err);
		} else {
			out_q.respond(request);
		}
		return true;
	}
	default:
		break;
	}
	return false;
}

FileParser &FileSystem::parser()
{
	return m_parser;
}

uint16_t FileSystem::startReceiving(lbp::MaxPayload &payload)
{
	m_file.setFileName(getTmpFilename());

	if (m_state != FileSystem::State::Idle) {
		gLog().error("Cannot start receiving - state error");
		return err_invalid_state;
	}

	uint32_t size = 0;
	if (payload.size() != 4 || !payload.readArg(size)) {
		gLog().error("Cannot start receiving - malformed packet");
		return err_bad_length;
	}

	if (m_file.open(QIODevice::WriteOnly)) {
		m_state = FileSystem::State::Receiving;
		m_parser.clear();

		gLog().push(Log::INFO, QString("File size: %1").arg(size));
		return 0;
	}
	gLog().error("Could not open file for writing");
	m_state = FileSystem::State::Idle;
	return err_internal;
}

uint16_t FileSystem::receive(const lbp::MaxPayload &payload)
{
	if (m_state != FileSystem::State::Receiving) {
		gLog().error("Cannot receive - state error");
		return err_transfer;
	}
	if (!m_file.isOpen()) {
		gLog().error("Cannot receive - file not open");
		return err_internal;
	}

	const qint64 len = payload.size();
	gLog().push(Log::INFO, QString("Receiving %1").arg(len));
	if (m_file.write((const char *) payload.data(), len) != len) {
		gLog().error("Cannot receive - write failed");
		return err_storage_full;
	}
	return 0;
}

void FileSystem::stopReceiving()
{
	if (m_state == FileSystem::State::Receiving) {
		m_file.close();
		m_state = FileSystem::State::Idle;
	}
}

uint32_t FileSystem::getFwState(uint32_t state)
{
	if (m_state == FileSystem::State::Receiving) {
		state |= state_receiving;
	} else {
		state &= ~state_receiving;
	}
	return state;
}

bool FileSystem::startReading()
{
	if (m_state != FileSystem::State::Idle) {
		gLog().error("Cannot start reading - state error");
		return false;
	}

	m_file.setFileName(getTmpFilename());
	if (!m_file.open(QIODevice::ReadOnly)) {
		gLog().error(QString("failed to open %1 for reading").arg(m_file.fileName()));
		return false;
	}
	m_state = FileSystem::State::Reading;
	return true;
}

void FileSystem::stopReading()
{
	if (m_state == FileSystem::State::Reading) {
		if (m_file.isOpen()) {
			m_file.close();
		}
		m_state = FileSystem::State::Idle;
	}
}

size_t FileSystem::feedParser()
{
	if (m_state != FileSystem::State::Reading) {
		return 0;
	}

	if ((m_file.openMode() & QIODevice::ReadOnly) == 0) {
		gLog().error("Could not feed parser, no file is open");
		return 0;
	}

	const QByteArray b = m_file.read(m_parser.freeSpace());
	if (b.isEmpty()) {
		if (m_file.atEnd()) {
			gLog().push(Log::INFO, "Reached end of job file");
			stopReading();
		}
		return 0;
	}
	return m_parser.feed((const uint8_t *) b.constData(), b.size());
}

void FileSystem::stop()
{
	stopReceiving();
	stopReading();
	m_parser.clear();
}

void FileSystem::reset()
{
	stopReceiving();
	stopReading();
	m_parser.clear();
}

bool FileSystem::isReading() const
{
	return m_state == State::Reading;
}
