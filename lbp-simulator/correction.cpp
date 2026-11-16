// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include "correction.h"

#include <lbp/beio.h>
#include <lbp/spec.h>

#include <cstdlib>
#include <cstring>

uint16_t CorrectionTable::parse(const QByteArray &data, uint32_t &detail)
{
	*this = CorrectionTable();
	detail = 0;

	const uint8_t *p = (const uint8_t *) data.constData();
	const size_t size = size_t(data.size());

	if (size < lbp::lbpcor_header_size) {
		return lbp::err_invalid_file; // too short to hold a header
	}

	// Header: check each field, reporting the first bad one by its offset.
	if (std::memcmp(p + lbp::lbpcor_offset_magic, lbp::lbpcor_magic, sizeof(lbp::lbpcor_magic)) != 0) {
		detail = lbp::lbpcor_offset_magic;
		return lbp::err_invalid_header;
	}
	if (lbp::readBE16(p + lbp::lbpcor_offset_version) != lbp::lbpcor_version) {
		detail = lbp::lbpcor_offset_version;
		return lbp::err_invalid_header;
	}
	const uint16_t header_size = lbp::readBE16(p + lbp::lbpcor_offset_header_size);
	if (header_size < lbp::lbpcor_header_size) {
		detail = lbp::lbpcor_offset_header_size;
		return lbp::err_invalid_header;
	}
	const int32_t field = (int32_t) lbp::readBE32(p + lbp::lbpcor_offset_field_size);
	if (field <= 0) {
		detail = lbp::lbpcor_offset_field_size;
		return lbp::err_invalid_header;
	}
	// Odd, so a node sits at the field centre; no upper limit beyond what this simulator can hold.
	const uint16_t grid = lbp::readBE16(p + lbp::lbpcor_offset_grid_size);
	if (grid < 3 || (grid % 2) == 0) {
		detail = lbp::lbpcor_offset_grid_size;
		return lbp::err_invalid_header;
	}
	if (p[lbp::lbpcor_offset_cell_format] != lbp::lbpcor_cell_int32_nm) {
		detail = lbp::lbpcor_offset_cell_format;
		return lbp::err_invalid_header;
	}

	// Length. (No checksum of its own: the transfer's cmd_file_end CRC-32 was already checked.)
	const size_t nodes = size_t(grid) * grid;
	if (size != header_size + nodes * 8) {
		return lbp::err_invalid_file;
	}

	// Grid: (dx, dy) int32 nanometer pairs after the header.
	// The optional magnitude check: a cell larger than half the field is a broken table, most likely
	// one written in micrometers.
	const int64_t limit_nm = int64_t(field) * 500;
	dx_nm.resize(nodes);
	dy_nm.resize(nodes);
	const uint8_t *g = p + header_size;
	for (size_t i = 0; i < nodes; i++) {
		const int32_t dx = (int32_t) lbp::readBE32(g + i * 8);
		const int32_t dy = (int32_t) lbp::readBE32(g + i * 8 + 4);
		if (std::llabs(dx) > limit_nm || std::llabs(dy) > limit_nm) {
			*this = CorrectionTable();
			return lbp::err_invalid_file;
		}
		dx_nm[i] = dx;
		dy_nm[i] = dy;
	}

	field_size_um = field;
	grid_size = grid;
	axis_flags = p[lbp::lbpcor_offset_axis_flags];
	composition = p[lbp::lbpcor_offset_composition];
	valid = true;
	return 0;
}
