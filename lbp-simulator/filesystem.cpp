// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include "filesystem.h"
#include "log.h"

#include <lbp/beio.h>
#include <lbp/checksum.h>
#include <lbp/spec.h>
#include <QDir>
#include <QStandardPaths>

using namespace lbp;

static QString getTmpFilename()
{
	return QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
		.filePath("tmp.oz");
}

/** Correction tables are kept apart from the job file, so receiving one never replaces a loaded job. */
static QString getCorrectionFilename()
{
	return QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
		.filePath("correction.lbpcor");
}

bool FileSystem::process(MaxPayload &request, OutputQueue &out_q)
{
	const uint16_t cmd = request.cmd();

	switch (cmd) {
	case cmd_file_begin: {
		gLog().push(Log::INFO, "BOF~~~~~~");
		if (isReading()) {
			out_q.nack(request, err_wrong_state, state_executing_job);
			return true;
		}
		reset();
		const uint16_t err = startReceiving(request);
		if (err) {
			out_q.nack(request, err, err == err_invalid_length ? 5 : 0);
		} else {
			out_q.respond(request);
		}
		return true;
	}
	case cmd_file_end: {
		gLog().push(Log::INFO, "EOF~~~~~~");
		if (m_state != State::Receiving) {
			out_q.nack(request, err_transfer);
			return true;
		}
		const uint16_t end_err = finishReceiving(request);
		if (end_err) {
			out_q.nack(request, end_err, end_err == err_invalid_length ? 4 : 0);
			return true;
		}
		if (m_receiving_type == file_type_correction) {
			uint32_t detail = 0;
			const uint16_t err = finishCorrection(detail);
			if (err) {
				out_q.nack(request, err, detail);
				return true;
			}
		}
		out_q.respond(request);
		return true;
	}
	case cmd_file_chunk: {
		uint32_t detail = 0;
		const uint16_t err = receive(request, detail);
		if (err) {
			out_q.nack(request, err, detail);
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
	if (m_state != FileSystem::State::Idle) {
		gLog().error("Cannot start receiving - state error");
		return err_wrong_state;
	}

	uint8_t type = 0;
	uint32_t size = 0;
	if (payload.size() != 5 || !payload.readArgs(type, size)) {
		gLog().error("Cannot start receiving - malformed packet");
		return err_invalid_length;
	}
	const bool supported = type == file_type_job || (type == file_type_correction && m_galvo);
	if (!supported) {
		gLog().error(QString("Cannot start receiving - unsupported file type %1").arg(type));
		return err_invalid_arg; // detail 0: the file type argument
	}

	// Like firmware, write to storage as chunks arrive. Use a separate ".part" file, so a transfer that fails its
	// CRC never replaces the stored job or correction table; it's renamed into place at cmd_file_end.
	m_receiving_type = type;
	m_receive_target = type == file_type_correction ? getCorrectionFilename() : getTmpFilename();
	m_receive_file.setFileName(m_receive_target + QStringLiteral(".part"));
	if (!m_receive_file.open(QIODevice::ReadWrite | QIODevice::Truncate) || !m_receive_file.resize(qint64(size))) {
		gLog().error(QString("Cannot start receiving - can't prepare %1 bytes").arg(size));
		discardReceiveFile();
		return err_storage_full;
	}
	m_receive_size = size;
	if (type == file_type_job) {
		m_parser.clear();
	}
	m_state = FileSystem::State::Receiving;
	gLog().push(Log::INFO, QString("File size: %1").arg(size));
	return 0;
}

void FileSystem::discardReceiveFile()
{
	if (m_receive_file.isOpen()) {
		m_receive_file.close();
	}
	if (!m_receive_file.fileName().isEmpty()) {
		QFile::remove(m_receive_file.fileName());
	}
}

uint16_t FileSystem::receive(const lbp::MaxPayload &payload, uint32_t &detail)
{
	detail = 0;
	if (m_state != FileSystem::State::Receiving) {
		gLog().error("Cannot receive - state error");
		return err_transfer;
	}
	if (payload.size() < 4) {
		gLog().error("Cannot receive - no offset");
		return err_invalid_length;
	}

	// Write the data at its offset; order doesn't matter (the CRC at cmd_file_end checks the result).
	const uint64_t offset = lbp::readBE32(payload.data());
	const qint64 len = qint64(payload.size() - 4);
	if (offset + uint64_t(len) > m_receive_size) {
		gLog().error(QString("Cannot receive - chunk at %1 (+%2) is past the file's %3 bytes")
						 .arg(offset).arg(len).arg(m_receive_size));
		detail = m_receive_size;
		return err_transfer;
	}
	if (!m_receive_file.seek(qint64(offset))
		|| m_receive_file.write((const char *) payload.data() + 4, len) != len) {
		gLog().error("Cannot receive - write failed");
		return err_storage_full;
	}
	gLog().push(Log::INFO, QString("Received %1 bytes at %2").arg(len).arg(offset));
	return 0;
}

uint16_t FileSystem::finishReceiving(const lbp::MaxPayload &payload)
{
	// Whatever happens, the transfer is over. On any failure the .part file is discarded and the stored file kept.
	m_state = FileSystem::State::Idle;

	if (payload.size() != 4) {
		gLog().error("Cannot finish receiving - cmd_file_end needs a uint32 CRC-32");
		discardReceiveFile();
		return err_invalid_length;
	}
	const uint32_t expected = lbp::readBE32(payload.data());

	// Read the file back and check its CRC-32. The file was sized at cmd_file_begin, so this covers exactly the
	// declared size: a chunk that never arrived leaves bytes the CRC won't match.
	uint32_t crc = lbp::crc32_begin();
	if (!m_receive_file.seek(0)) {
		discardReceiveFile();
		return err_storage_full;
	}
	while (!m_receive_file.atEnd()) {
		const QByteArray block = m_receive_file.read(64 * 1024);
		if (block.isEmpty()) {
			break;
		}
		crc = lbp::crc32_update(crc, (const uint8_t *) block.constData(), size_t(block.size()));
	}
	crc = lbp::crc32_end(crc);
	if (crc != expected) {
		gLog().error(QString("File CRC-32 mismatch: got %1, expected %2")
						 .arg(crc, 8, 16, QLatin1Char('0')).arg(expected, 8, 16, QLatin1Char('0')));
		discardReceiveFile();
		return err_invalid_file;
	}

	// Complete and intact: move it to where the job reader or the correction loader expects it.
	m_receive_file.close();
	QFile::remove(m_receive_target);
	if (!QFile::rename(m_receive_file.fileName(), m_receive_target)) {
		gLog().error("Cannot store the received file");
		discardReceiveFile();
		return err_storage_full;
	}
	return 0;
}

void FileSystem::stopReceiving()
{
	if (m_state == FileSystem::State::Receiving) {
		discardReceiveFile(); // an unfinished transfer leaves the stored file as it was
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

void FileSystem::setGalvo(bool galvo)
{
	m_galvo = galvo;
	if (!galvo) {
		m_correction = CorrectionTable();
		m_new_correction = false;
	}
}

bool FileSystem::takeNewCorrection()
{
	const bool result = m_new_correction;
	m_new_correction = false;
	return result;
}

uint16_t FileSystem::finishCorrection(uint32_t &detail)
{
	detail = 0;
	QFile file(getCorrectionFilename());
	if (!file.open(QIODevice::ReadOnly)) {
		gLog().error("Cannot read the received correction file");
		return err_unknown;
	}

	// Validate before replacing the stored table: a bad file leaves the previous table in place.
	CorrectionTable table;
	const uint16_t err = table.parse(file.readAll(), detail);
	if (err) {
		gLog().error(QString("Correction file rejected: error 0x%1, detail %2").arg(err, 4, 16, QChar('0')).arg(detail));
		return err;
	}

	m_correction = std::move(table);
	m_new_correction = true;
	gLog().push(Log::INFO, QString("Correction table stored: %1 um field, flags 0x%2")
								.arg(m_correction.field_size_um)
								.arg(m_correction.flags, 0, 16));
	return 0;
}
