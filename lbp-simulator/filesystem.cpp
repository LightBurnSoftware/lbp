// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include "filesystem.h"
#include "log.h"

#include <lbp/spec.h>

using namespace lbp;

bool FileSystem::process(MaxPayload &request, OutputQueue &out_q)
{
	const uint16_t cmd = request.cmd();

	switch (cmd) {
	case cmd_begin_file: {
		gLog().push(Log::INFO, "BOF~~~~~~");
		out_q.push(CmdMsg(cmd));
		m_is_receiving = true;
		uint32_t size = request.readIntArg();
		m_file_buffer.reset(size);
		m_parser.clear();
		gLog().push(Log::INFO, QString("File size: %1").arg(size));
		return true;
	}
	case cmd_end_file:
		gLog().push(Log::INFO, "EOF~~~~~~");
		out_q.push(CmdMsg(cmd));
		m_is_receiving = false;
		return true;
	case cmd_file_chunk:
		if (m_is_receiving) {
			m_file_buffer.write(request.args(), request.size() - 2);
			gLog().push(Log::INFO, QString("Receiving %1").arg(request.size() - 2));
		}
		out_q.push(CmdMsg(cmd));
		return true;
	case cmd_get_filename:
	case cmd_set_filename:
	case cmd_delete_file:
	case cmd_new_file:
	case cmd_file_count:
	case cmd_file_time:
	case cmd_calc_file_time:
		out_q.push(CmdMsg(cmd));
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

uint32_t FileSystem::getFwState(uint32_t state)
{
	if (m_is_receiving) {
		state |= state_receiving;
	} else {
		state &= ~state_receiving;
	}
	return state;
}

void FileSystem::feedParser()
{
	m_file_buffer.feedParser(m_parser);
}

void FileSystem::stop()
{
	m_file_buffer.rewind();
	m_parser.clear();
	m_is_receiving = false;
}

void FileSystem::reset()
{
	m_file_buffer.rewind();
	m_parser.clear();
}
