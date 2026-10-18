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
	case cmd_file_begin:
		gLog().push(Log::INFO, "BOF~~~~~~");
		reset();
		startReceiving(request);
		out_q.respond(request);
		return true;
	case cmd_file_end:
		gLog().push(Log::INFO, "EOF~~~~~~");
		stopReceiving();
		out_q.respond(request);
		return true;
	case cmd_file_chunk:
		receive(request);
		out_q.respond(request);
		return true;
	default:
		break;
	}
	return false;
}

FileParser &FileSystem::parser()
{
	return m_parser;
}

void FileSystem::startReceiving(lbp::MaxPayload &payload)
{
	m_file.setFileName(getTmpFilename());

	if (m_state != FileSystem::State::Idle) {
		gLog().error("Cannot start receiving - state error");
		return;
	}

	uint32_t size = 0;
	if (!payload.readArg(size)) {
		gLog().error("Cannot start receiving - malformed packet");
		return;
	}

	if (m_file.open(QIODevice::WriteOnly)) {
		m_state = FileSystem::State::Receiving;
		m_parser.clear();

		gLog().push(Log::INFO, QString("File size: %1").arg(size));
	} else {
		gLog().error("Could not open file for writing");
		m_state = FileSystem::State::Idle;
	}
}

void FileSystem::receive(const lbp::MaxPayload &payload)
{
	if (m_state != FileSystem::State::Receiving) {
		gLog().error("Cannot receive - state error");
		return;
	}
	if (!m_file.isOpen()) {
		gLog().error("Cannot receive - file not open");
		return;
	}

	gLog().push(Log::INFO, QString("Receiving %1").arg(payload.size() - 2));
	m_file.write((const char *) payload.args(), payload.size() - 2);
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
