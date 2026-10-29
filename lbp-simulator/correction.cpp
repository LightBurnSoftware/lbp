// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include "correction.h"

#include <lbp/beio.h>
#include <lbp/spec.h>

// Byte offsets of the version 1 header fields (spec.md, "Lens Correction Files").
namespace {
constexpr uint32_t kOffMagic = 0;
constexpr uint32_t kOffVersion = 4;
constexpr uint32_t kOffHeaderSize = 6;
constexpr uint32_t kOffGridX = 8;
constexpr uint32_t kOffGridY = 10;
constexpr uint32_t kOffLayers = 12;
constexpr uint32_t kOffFlags = 14;
constexpr uint32_t kOffField = lbp::lbpcor_field_size_offset;
} // namespace

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
	if (lbp::readBE32(p + kOffMagic) != lbp::lbpcor_magic) {
		detail = kOffMagic;
		return lbp::err_invalid_header;
	}
	if (lbp::readBE16(p + kOffVersion) != lbp::lbpcor_version) {
		detail = kOffVersion;
		return lbp::err_invalid_header;
	}
	const uint16_t header_size = lbp::readBE16(p + kOffHeaderSize);
	if (header_size < lbp::lbpcor_header_size) {
		detail = kOffHeaderSize;
		return lbp::err_invalid_header;
	}
	const uint16_t grid_x_val = lbp::readBE16(p + kOffGridX);
	if (grid_x_val != lbp::lbpcor_grid_size) {
		detail = kOffGridX;
		return lbp::err_invalid_header;
	}
	const uint16_t grid_y_val = lbp::readBE16(p + kOffGridY);
	if (grid_y_val != lbp::lbpcor_grid_size) {
		detail = kOffGridY;
		return lbp::err_invalid_header;
	}
	if (lbp::readBE16(p + kOffLayers) != lbp::lbpcor_layers) {
		detail = kOffLayers;
		return lbp::err_invalid_header;
	}
	const uint16_t flags_val = lbp::readBE16(p + kOffFlags);
	if ((flags_val & lbp::lbpcor_flags_reserved) != 0) {
		detail = kOffFlags;
		return lbp::err_invalid_header;
	}
	const int32_t field = (int32_t) lbp::readBE32(p + kOffField);
	if (field <= 0) {
		detail = kOffField;
		return lbp::err_invalid_header;
	}

	// Length. (No checksum of its own: the transfer's cmd_file_end CRC-32 was already checked.)
	const size_t nodes = size_t(grid_x_val) * grid_y_val;
	if (size != header_size + nodes * 8) {
		return lbp::err_invalid_file;
	}

	// Grid: (dx, dy) int32 pairs after the header.
	dx_um.resize(nodes);
	dy_um.resize(nodes);
	const uint8_t *g = p + header_size;
	for (size_t i = 0; i < nodes; i++) {
		dx_um[i] = (int32_t) lbp::readBE32(g + i * 8);
		dy_um[i] = (int32_t) lbp::readBE32(g + i * 8 + 4);
	}

	flags = flags_val;
	field_size_um = field;
	grid_x = grid_x_val;
	grid_y = grid_y_val;
	valid = true;
	return 0;
}
